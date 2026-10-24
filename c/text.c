/* FORMAT-style text preprocessing (chapter 2): split sentences, expand
 * numbers, abbreviations and symbols into words.  Port of mitalk/text.py,
 * which is written as a character scanner so this file can follow it
 * line for line.
 *
 * The input is decoded to code points first (UTF-8; a byte that does not
 * start a valid UTF-8 sequence is read as Windows-1252, which is what the
 * Windows C runtime hands main() in argv), then folded with the FOLD table.
 * Python's str predicates (isdigit, isalpha, isalnum, isupper, upper) are
 * mirrored with tables generated from Python, so non-ASCII characters take
 * the same paths through the scanner as in text.py.
 */
#include <stdlib.h>
#include <string.h>
#include "mitalk.h"

/* ------------------------------------------------------------ buffers */
typedef struct { char *s; size_t n, cap; } Buf;

static void buf_putc(Buf *b, char c)
{
    if (b->n + 2 > b->cap) {
        b->cap = b->cap ? b->cap * 2 : 64;
        b->s = xrealloc(b->s, b->cap);
    }
    b->s[b->n++] = c;
    b->s[b->n] = 0;
}

static void buf_puts(Buf *b, const char *s)
{
    while (*s)
        buf_putc(b, *s++);
}

static const char *buf_str(Buf *b)
{
    if (!b->s)
        buf_putc(b, 0), b->n = 0;
    return b->s;
}

typedef struct { char **v; int n, cap; } Toks;

static char *strndup_(const char *s, size_t len)
{
    char *p = xmalloc(len + 1);
    memcpy(p, s, len);
    p[len] = 0;
    return p;
}

static void toks_add(Toks *t, char *owned)
{
    if (t->n == t->cap) {
        t->cap = t->cap ? t->cap * 2 : 16;
        t->v = xrealloc(t->v, t->cap * sizeof *t->v);
    }
    t->v[t->n++] = owned;
}

/* Python's toks += s.split() */
static void toks_add_words(Toks *t, const char *s)
{
    while (*s) {
        while (*s == ' ')
            s++;
        const char *e = s;
        while (*e && *e != ' ')
            e++;
        if (e > s)
            toks_add(t, strndup_(s, e - s));
        s = e;
    }
}

static void toks_free(Toks *t)
{
    for (int i = 0; i < t->n; i++)
        free(t->v[i]);
    free(t->v);
    memset(t, 0, sizeof *t);
}

/* ---------------------------------------------------- character classes */
/* Non-ASCII ranges of Python's str.isalpha(), str.isdecimal() (each range
 * starts at a zero digit) and the other str.isalnum() characters (numerics),
 * generated from Python 3.13.13 (Unicode 15.1.0). */
static const unsigned ALPHA[][2] = {
    {0xAA,0xAA}, {0xB5,0xB5}, {0xBA,0xBA}, {0xC0,0xD6}, {0xD8,0xF6}, {0xF8,0x2C1},
    {0x2C6,0x2D1}, {0x2E0,0x2E4}, {0x2EC,0x2EC}, {0x2EE,0x2EE}, {0x370,0x374}, {0x376,0x377},
    {0x37A,0x37D}, {0x37F,0x37F}, {0x386,0x386}, {0x388,0x38A}, {0x38C,0x38C}, {0x38E,0x3A1},
    {0x3A3,0x3F5}, {0x3F7,0x481}, {0x48A,0x52F}, {0x531,0x556}, {0x559,0x559}, {0x560,0x588},
    {0x5D0,0x5EA}, {0x5EF,0x5F2}, {0x620,0x64A}, {0x66E,0x66F}, {0x671,0x6D3}, {0x6D5,0x6D5},
    {0x6E5,0x6E6}, {0x6EE,0x6EF}, {0x6FA,0x6FC}, {0x6FF,0x6FF}, {0x710,0x710}, {0x712,0x72F},
    {0x74D,0x7A5}, {0x7B1,0x7B1}, {0x7CA,0x7EA}, {0x7F4,0x7F5}, {0x7FA,0x7FA}, {0x800,0x815},
    {0x81A,0x81A}, {0x824,0x824}, {0x828,0x828}, {0x840,0x858}, {0x860,0x86A}, {0x870,0x887},
    {0x889,0x88E}, {0x8A0,0x8C9}, {0x904,0x939}, {0x93D,0x93D}, {0x950,0x950}, {0x958,0x961},
    {0x971,0x980}, {0x985,0x98C}, {0x98F,0x990}, {0x993,0x9A8}, {0x9AA,0x9B0}, {0x9B2,0x9B2},
    {0x9B6,0x9B9}, {0x9BD,0x9BD}, {0x9CE,0x9CE}, {0x9DC,0x9DD}, {0x9DF,0x9E1}, {0x9F0,0x9F1},
    {0x9FC,0x9FC}, {0xA05,0xA0A}, {0xA0F,0xA10}, {0xA13,0xA28}, {0xA2A,0xA30}, {0xA32,0xA33},
    {0xA35,0xA36}, {0xA38,0xA39}, {0xA59,0xA5C}, {0xA5E,0xA5E}, {0xA72,0xA74}, {0xA85,0xA8D},
    {0xA8F,0xA91}, {0xA93,0xAA8}, {0xAAA,0xAB0}, {0xAB2,0xAB3}, {0xAB5,0xAB9}, {0xABD,0xABD},
    {0xAD0,0xAD0}, {0xAE0,0xAE1}, {0xAF9,0xAF9}, {0xB05,0xB0C}, {0xB0F,0xB10}, {0xB13,0xB28},
    {0xB2A,0xB30}, {0xB32,0xB33}, {0xB35,0xB39}, {0xB3D,0xB3D}, {0xB5C,0xB5D}, {0xB5F,0xB61},
    {0xB71,0xB71}, {0xB83,0xB83}, {0xB85,0xB8A}, {0xB8E,0xB90}, {0xB92,0xB95}, {0xB99,0xB9A},
    {0xB9C,0xB9C}, {0xB9E,0xB9F}, {0xBA3,0xBA4}, {0xBA8,0xBAA}, {0xBAE,0xBB9}, {0xBD0,0xBD0},
    {0xC05,0xC0C}, {0xC0E,0xC10}, {0xC12,0xC28}, {0xC2A,0xC39}, {0xC3D,0xC3D}, {0xC58,0xC5A},
    {0xC5D,0xC5D}, {0xC60,0xC61}, {0xC80,0xC80}, {0xC85,0xC8C}, {0xC8E,0xC90}, {0xC92,0xCA8},
    {0xCAA,0xCB3}, {0xCB5,0xCB9}, {0xCBD,0xCBD}, {0xCDD,0xCDE}, {0xCE0,0xCE1}, {0xCF1,0xCF2},
    {0xD04,0xD0C}, {0xD0E,0xD10}, {0xD12,0xD3A}, {0xD3D,0xD3D}, {0xD4E,0xD4E}, {0xD54,0xD56},
    {0xD5F,0xD61}, {0xD7A,0xD7F}, {0xD85,0xD96}, {0xD9A,0xDB1}, {0xDB3,0xDBB}, {0xDBD,0xDBD},
    {0xDC0,0xDC6}, {0xE01,0xE30}, {0xE32,0xE33}, {0xE40,0xE46}, {0xE81,0xE82}, {0xE84,0xE84},
    {0xE86,0xE8A}, {0xE8C,0xEA3}, {0xEA5,0xEA5}, {0xEA7,0xEB0}, {0xEB2,0xEB3}, {0xEBD,0xEBD},
    {0xEC0,0xEC4}, {0xEC6,0xEC6}, {0xEDC,0xEDF}, {0xF00,0xF00}, {0xF40,0xF47}, {0xF49,0xF6C},
    {0xF88,0xF8C}, {0x1000,0x102A}, {0x103F,0x103F}, {0x1050,0x1055}, {0x105A,0x105D},
    {0x1061,0x1061}, {0x1065,0x1066}, {0x106E,0x1070}, {0x1075,0x1081}, {0x108E,0x108E},
    {0x10A0,0x10C5}, {0x10C7,0x10C7}, {0x10CD,0x10CD}, {0x10D0,0x10FA}, {0x10FC,0x1248},
    {0x124A,0x124D}, {0x1250,0x1256}, {0x1258,0x1258}, {0x125A,0x125D}, {0x1260,0x1288},
    {0x128A,0x128D}, {0x1290,0x12B0}, {0x12B2,0x12B5}, {0x12B8,0x12BE}, {0x12C0,0x12C0},
    {0x12C2,0x12C5}, {0x12C8,0x12D6}, {0x12D8,0x1310}, {0x1312,0x1315}, {0x1318,0x135A},
    {0x1380,0x138F}, {0x13A0,0x13F5}, {0x13F8,0x13FD}, {0x1401,0x166C}, {0x166F,0x167F},
    {0x1681,0x169A}, {0x16A0,0x16EA}, {0x16F1,0x16F8}, {0x1700,0x1711}, {0x171F,0x1731},
    {0x1740,0x1751}, {0x1760,0x176C}, {0x176E,0x1770}, {0x1780,0x17B3}, {0x17D7,0x17D7},
    {0x17DC,0x17DC}, {0x1820,0x1878}, {0x1880,0x1884}, {0x1887,0x18A8}, {0x18AA,0x18AA},
    {0x18B0,0x18F5}, {0x1900,0x191E}, {0x1950,0x196D}, {0x1970,0x1974}, {0x1980,0x19AB},
    {0x19B0,0x19C9}, {0x1A00,0x1A16}, {0x1A20,0x1A54}, {0x1AA7,0x1AA7}, {0x1B05,0x1B33},
    {0x1B45,0x1B4C}, {0x1B83,0x1BA0}, {0x1BAE,0x1BAF}, {0x1BBA,0x1BE5}, {0x1C00,0x1C23},
    {0x1C4D,0x1C4F}, {0x1C5A,0x1C7D}, {0x1C80,0x1C88}, {0x1C90,0x1CBA}, {0x1CBD,0x1CBF},
    {0x1CE9,0x1CEC}, {0x1CEE,0x1CF3}, {0x1CF5,0x1CF6}, {0x1CFA,0x1CFA}, {0x1D00,0x1DBF},
    {0x1E00,0x1F15}, {0x1F18,0x1F1D}, {0x1F20,0x1F45}, {0x1F48,0x1F4D}, {0x1F50,0x1F57},
    {0x1F59,0x1F59}, {0x1F5B,0x1F5B}, {0x1F5D,0x1F5D}, {0x1F5F,0x1F7D}, {0x1F80,0x1FB4},
    {0x1FB6,0x1FBC}, {0x1FBE,0x1FBE}, {0x1FC2,0x1FC4}, {0x1FC6,0x1FCC}, {0x1FD0,0x1FD3},
    {0x1FD6,0x1FDB}, {0x1FE0,0x1FEC}, {0x1FF2,0x1FF4}, {0x1FF6,0x1FFC}, {0x2071,0x2071},
    {0x207F,0x207F}, {0x2090,0x209C}, {0x2102,0x2102}, {0x2107,0x2107}, {0x210A,0x2113},
    {0x2115,0x2115}, {0x2119,0x211D}, {0x2124,0x2124}, {0x2126,0x2126}, {0x2128,0x2128},
    {0x212A,0x212D}, {0x212F,0x2139}, {0x213C,0x213F}, {0x2145,0x2149}, {0x214E,0x214E},
    {0x2183,0x2184}, {0x2C00,0x2CE4}, {0x2CEB,0x2CEE}, {0x2CF2,0x2CF3}, {0x2D00,0x2D25},
    {0x2D27,0x2D27}, {0x2D2D,0x2D2D}, {0x2D30,0x2D67}, {0x2D6F,0x2D6F}, {0x2D80,0x2D96},
    {0x2DA0,0x2DA6}, {0x2DA8,0x2DAE}, {0x2DB0,0x2DB6}, {0x2DB8,0x2DBE}, {0x2DC0,0x2DC6},
    {0x2DC8,0x2DCE}, {0x2DD0,0x2DD6}, {0x2DD8,0x2DDE}, {0x2E2F,0x2E2F}, {0x3005,0x3006},
    {0x3031,0x3035}, {0x303B,0x303C}, {0x3041,0x3096}, {0x309D,0x309F}, {0x30A1,0x30FA},
    {0x30FC,0x30FF}, {0x3105,0x312F}, {0x3131,0x318E}, {0x31A0,0x31BF}, {0x31F0,0x31FF},
    {0x3400,0x4DBF}, {0x4E00,0xA48C}, {0xA4D0,0xA4FD}, {0xA500,0xA60C}, {0xA610,0xA61F},
    {0xA62A,0xA62B}, {0xA640,0xA66E}, {0xA67F,0xA69D}, {0xA6A0,0xA6E5}, {0xA717,0xA71F},
    {0xA722,0xA788}, {0xA78B,0xA7CA}, {0xA7D0,0xA7D1}, {0xA7D3,0xA7D3}, {0xA7D5,0xA7D9},
    {0xA7F2,0xA801}, {0xA803,0xA805}, {0xA807,0xA80A}, {0xA80C,0xA822}, {0xA840,0xA873},
    {0xA882,0xA8B3}, {0xA8F2,0xA8F7}, {0xA8FB,0xA8FB}, {0xA8FD,0xA8FE}, {0xA90A,0xA925},
    {0xA930,0xA946}, {0xA960,0xA97C}, {0xA984,0xA9B2}, {0xA9CF,0xA9CF}, {0xA9E0,0xA9E4},
    {0xA9E6,0xA9EF}, {0xA9FA,0xA9FE}, {0xAA00,0xAA28}, {0xAA40,0xAA42}, {0xAA44,0xAA4B},
    {0xAA60,0xAA76}, {0xAA7A,0xAA7A}, {0xAA7E,0xAAAF}, {0xAAB1,0xAAB1}, {0xAAB5,0xAAB6},
    {0xAAB9,0xAABD}, {0xAAC0,0xAAC0}, {0xAAC2,0xAAC2}, {0xAADB,0xAADD}, {0xAAE0,0xAAEA},
    {0xAAF2,0xAAF4}, {0xAB01,0xAB06}, {0xAB09,0xAB0E}, {0xAB11,0xAB16}, {0xAB20,0xAB26},
    {0xAB28,0xAB2E}, {0xAB30,0xAB5A}, {0xAB5C,0xAB69}, {0xAB70,0xABE2}, {0xAC00,0xD7A3},
    {0xD7B0,0xD7C6}, {0xD7CB,0xD7FB}, {0xF900,0xFA6D}, {0xFA70,0xFAD9}, {0xFB00,0xFB06},
    {0xFB13,0xFB17}, {0xFB1D,0xFB1D}, {0xFB1F,0xFB28}, {0xFB2A,0xFB36}, {0xFB38,0xFB3C},
    {0xFB3E,0xFB3E}, {0xFB40,0xFB41}, {0xFB43,0xFB44}, {0xFB46,0xFBB1}, {0xFBD3,0xFD3D},
    {0xFD50,0xFD8F}, {0xFD92,0xFDC7}, {0xFDF0,0xFDFB}, {0xFE70,0xFE74}, {0xFE76,0xFEFC},
    {0xFF21,0xFF3A}, {0xFF41,0xFF5A}, {0xFF66,0xFFBE}, {0xFFC2,0xFFC7}, {0xFFCA,0xFFCF},
    {0xFFD2,0xFFD7}, {0xFFDA,0xFFDC}, {0x10000,0x1000B}, {0x1000D,0x10026}, {0x10028,0x1003A},
    {0x1003C,0x1003D}, {0x1003F,0x1004D}, {0x10050,0x1005D}, {0x10080,0x100FA},
    {0x10280,0x1029C}, {0x102A0,0x102D0}, {0x10300,0x1031F}, {0x1032D,0x10340},
    {0x10342,0x10349}, {0x10350,0x10375}, {0x10380,0x1039D}, {0x103A0,0x103C3},
    {0x103C8,0x103CF}, {0x10400,0x1049D}, {0x104B0,0x104D3}, {0x104D8,0x104FB},
    {0x10500,0x10527}, {0x10530,0x10563}, {0x10570,0x1057A}, {0x1057C,0x1058A},
    {0x1058C,0x10592}, {0x10594,0x10595}, {0x10597,0x105A1}, {0x105A3,0x105B1},
    {0x105B3,0x105B9}, {0x105BB,0x105BC}, {0x10600,0x10736}, {0x10740,0x10755},
    {0x10760,0x10767}, {0x10780,0x10785}, {0x10787,0x107B0}, {0x107B2,0x107BA},
    {0x10800,0x10805}, {0x10808,0x10808}, {0x1080A,0x10835}, {0x10837,0x10838},
    {0x1083C,0x1083C}, {0x1083F,0x10855}, {0x10860,0x10876}, {0x10880,0x1089E},
    {0x108E0,0x108F2}, {0x108F4,0x108F5}, {0x10900,0x10915}, {0x10920,0x10939},
    {0x10980,0x109B7}, {0x109BE,0x109BF}, {0x10A00,0x10A00}, {0x10A10,0x10A13},
    {0x10A15,0x10A17}, {0x10A19,0x10A35}, {0x10A60,0x10A7C}, {0x10A80,0x10A9C},
    {0x10AC0,0x10AC7}, {0x10AC9,0x10AE4}, {0x10B00,0x10B35}, {0x10B40,0x10B55},
    {0x10B60,0x10B72}, {0x10B80,0x10B91}, {0x10C00,0x10C48}, {0x10C80,0x10CB2},
    {0x10CC0,0x10CF2}, {0x10D00,0x10D23}, {0x10E80,0x10EA9}, {0x10EB0,0x10EB1},
    {0x10F00,0x10F1C}, {0x10F27,0x10F27}, {0x10F30,0x10F45}, {0x10F70,0x10F81},
    {0x10FB0,0x10FC4}, {0x10FE0,0x10FF6}, {0x11003,0x11037}, {0x11071,0x11072},
    {0x11075,0x11075}, {0x11083,0x110AF}, {0x110D0,0x110E8}, {0x11103,0x11126},
    {0x11144,0x11144}, {0x11147,0x11147}, {0x11150,0x11172}, {0x11176,0x11176},
    {0x11183,0x111B2}, {0x111C1,0x111C4}, {0x111DA,0x111DA}, {0x111DC,0x111DC},
    {0x11200,0x11211}, {0x11213,0x1122B}, {0x1123F,0x11240}, {0x11280,0x11286},
    {0x11288,0x11288}, {0x1128A,0x1128D}, {0x1128F,0x1129D}, {0x1129F,0x112A8},
    {0x112B0,0x112DE}, {0x11305,0x1130C}, {0x1130F,0x11310}, {0x11313,0x11328},
    {0x1132A,0x11330}, {0x11332,0x11333}, {0x11335,0x11339}, {0x1133D,0x1133D},
    {0x11350,0x11350}, {0x1135D,0x11361}, {0x11400,0x11434}, {0x11447,0x1144A},
    {0x1145F,0x11461}, {0x11480,0x114AF}, {0x114C4,0x114C5}, {0x114C7,0x114C7},
    {0x11580,0x115AE}, {0x115D8,0x115DB}, {0x11600,0x1162F}, {0x11644,0x11644},
    {0x11680,0x116AA}, {0x116B8,0x116B8}, {0x11700,0x1171A}, {0x11740,0x11746},
    {0x11800,0x1182B}, {0x118A0,0x118DF}, {0x118FF,0x11906}, {0x11909,0x11909},
    {0x1190C,0x11913}, {0x11915,0x11916}, {0x11918,0x1192F}, {0x1193F,0x1193F},
    {0x11941,0x11941}, {0x119A0,0x119A7}, {0x119AA,0x119D0}, {0x119E1,0x119E1},
    {0x119E3,0x119E3}, {0x11A00,0x11A00}, {0x11A0B,0x11A32}, {0x11A3A,0x11A3A},
    {0x11A50,0x11A50}, {0x11A5C,0x11A89}, {0x11A9D,0x11A9D}, {0x11AB0,0x11AF8},
    {0x11C00,0x11C08}, {0x11C0A,0x11C2E}, {0x11C40,0x11C40}, {0x11C72,0x11C8F},
    {0x11D00,0x11D06}, {0x11D08,0x11D09}, {0x11D0B,0x11D30}, {0x11D46,0x11D46},
    {0x11D60,0x11D65}, {0x11D67,0x11D68}, {0x11D6A,0x11D89}, {0x11D98,0x11D98},
    {0x11EE0,0x11EF2}, {0x11F02,0x11F02}, {0x11F04,0x11F10}, {0x11F12,0x11F33},
    {0x11FB0,0x11FB0}, {0x12000,0x12399}, {0x12480,0x12543}, {0x12F90,0x12FF0},
    {0x13000,0x1342F}, {0x13441,0x13446}, {0x14400,0x14646}, {0x16800,0x16A38},
    {0x16A40,0x16A5E}, {0x16A70,0x16ABE}, {0x16AD0,0x16AED}, {0x16B00,0x16B2F},
    {0x16B40,0x16B43}, {0x16B63,0x16B77}, {0x16B7D,0x16B8F}, {0x16E40,0x16E7F},
    {0x16F00,0x16F4A}, {0x16F50,0x16F50}, {0x16F93,0x16F9F}, {0x16FE0,0x16FE1},
    {0x16FE3,0x16FE3}, {0x17000,0x187F7}, {0x18800,0x18CD5}, {0x18D00,0x18D08},
    {0x1AFF0,0x1AFF3}, {0x1AFF5,0x1AFFB}, {0x1AFFD,0x1AFFE}, {0x1B000,0x1B122},
    {0x1B132,0x1B132}, {0x1B150,0x1B152}, {0x1B155,0x1B155}, {0x1B164,0x1B167},
    {0x1B170,0x1B2FB}, {0x1BC00,0x1BC6A}, {0x1BC70,0x1BC7C}, {0x1BC80,0x1BC88},
    {0x1BC90,0x1BC99}, {0x1D400,0x1D454}, {0x1D456,0x1D49C}, {0x1D49E,0x1D49F},
    {0x1D4A2,0x1D4A2}, {0x1D4A5,0x1D4A6}, {0x1D4A9,0x1D4AC}, {0x1D4AE,0x1D4B9},
    {0x1D4BB,0x1D4BB}, {0x1D4BD,0x1D4C3}, {0x1D4C5,0x1D505}, {0x1D507,0x1D50A},
    {0x1D50D,0x1D514}, {0x1D516,0x1D51C}, {0x1D51E,0x1D539}, {0x1D53B,0x1D53E},
    {0x1D540,0x1D544}, {0x1D546,0x1D546}, {0x1D54A,0x1D550}, {0x1D552,0x1D6A5},
    {0x1D6A8,0x1D6C0}, {0x1D6C2,0x1D6DA}, {0x1D6DC,0x1D6FA}, {0x1D6FC,0x1D714},
    {0x1D716,0x1D734}, {0x1D736,0x1D74E}, {0x1D750,0x1D76E}, {0x1D770,0x1D788},
    {0x1D78A,0x1D7A8}, {0x1D7AA,0x1D7C2}, {0x1D7C4,0x1D7CB}, {0x1DF00,0x1DF1E},
    {0x1DF25,0x1DF2A}, {0x1E030,0x1E06D}, {0x1E100,0x1E12C}, {0x1E137,0x1E13D},
    {0x1E14E,0x1E14E}, {0x1E290,0x1E2AD}, {0x1E2C0,0x1E2EB}, {0x1E4D0,0x1E4EB},
    {0x1E7E0,0x1E7E6}, {0x1E7E8,0x1E7EB}, {0x1E7ED,0x1E7EE}, {0x1E7F0,0x1E7FE},
    {0x1E800,0x1E8C4}, {0x1E900,0x1E943}, {0x1E94B,0x1E94B}, {0x1EE00,0x1EE03},
    {0x1EE05,0x1EE1F}, {0x1EE21,0x1EE22}, {0x1EE24,0x1EE24}, {0x1EE27,0x1EE27},
    {0x1EE29,0x1EE32}, {0x1EE34,0x1EE37}, {0x1EE39,0x1EE39}, {0x1EE3B,0x1EE3B},
    {0x1EE42,0x1EE42}, {0x1EE47,0x1EE47}, {0x1EE49,0x1EE49}, {0x1EE4B,0x1EE4B},
    {0x1EE4D,0x1EE4F}, {0x1EE51,0x1EE52}, {0x1EE54,0x1EE54}, {0x1EE57,0x1EE57},
    {0x1EE59,0x1EE59}, {0x1EE5B,0x1EE5B}, {0x1EE5D,0x1EE5D}, {0x1EE5F,0x1EE5F},
    {0x1EE61,0x1EE62}, {0x1EE64,0x1EE64}, {0x1EE67,0x1EE6A}, {0x1EE6C,0x1EE72},
    {0x1EE74,0x1EE77}, {0x1EE79,0x1EE7C}, {0x1EE7E,0x1EE7E}, {0x1EE80,0x1EE89},
    {0x1EE8B,0x1EE9B}, {0x1EEA1,0x1EEA3}, {0x1EEA5,0x1EEA9}, {0x1EEAB,0x1EEBB},
    {0x20000,0x2A6DF}, {0x2A700,0x2B739}, {0x2B740,0x2B81D}, {0x2B820,0x2CEA1},
    {0x2CEB0,0x2EBE0}, {0x2EBF0,0x2EE5D}, {0x2F800,0x2FA1D}, {0x30000,0x3134A},
    {0x31350,0x323AF},
};
static const unsigned DECIMAL[][2] = {
    {0x660,0x669}, {0x6F0,0x6F9}, {0x7C0,0x7C9}, {0x966,0x96F}, {0x9E6,0x9EF}, {0xA66,0xA6F},
    {0xAE6,0xAEF}, {0xB66,0xB6F}, {0xBE6,0xBEF}, {0xC66,0xC6F}, {0xCE6,0xCEF}, {0xD66,0xD6F},
    {0xDE6,0xDEF}, {0xE50,0xE59}, {0xED0,0xED9}, {0xF20,0xF29}, {0x1040,0x1049},
    {0x1090,0x1099}, {0x17E0,0x17E9}, {0x1810,0x1819}, {0x1946,0x194F}, {0x19D0,0x19D9},
    {0x1A80,0x1A89}, {0x1A90,0x1A99}, {0x1B50,0x1B59}, {0x1BB0,0x1BB9}, {0x1C40,0x1C49},
    {0x1C50,0x1C59}, {0xA620,0xA629}, {0xA8D0,0xA8D9}, {0xA900,0xA909}, {0xA9D0,0xA9D9},
    {0xA9F0,0xA9F9}, {0xAA50,0xAA59}, {0xABF0,0xABF9}, {0xFF10,0xFF19}, {0x104A0,0x104A9},
    {0x10D30,0x10D39}, {0x11066,0x1106F}, {0x110F0,0x110F9}, {0x11136,0x1113F},
    {0x111D0,0x111D9}, {0x112F0,0x112F9}, {0x11450,0x11459}, {0x114D0,0x114D9},
    {0x11650,0x11659}, {0x116C0,0x116C9}, {0x11730,0x11739}, {0x118E0,0x118E9},
    {0x11950,0x11959}, {0x11C50,0x11C59}, {0x11D50,0x11D59}, {0x11DA0,0x11DA9},
    {0x11F50,0x11F59}, {0x16A60,0x16A69}, {0x16AC0,0x16AC9}, {0x16B50,0x16B59},
    {0x1D7CE,0x1D7FF}, {0x1E140,0x1E149}, {0x1E2F0,0x1E2F9}, {0x1E4F0,0x1E4F9},
    {0x1E950,0x1E959}, {0x1FBF0,0x1FBF9},
};
static const unsigned NUMERIC[][2] = {
    {0xB2,0xB3}, {0xB9,0xB9}, {0xBC,0xBE}, {0x9F4,0x9F9}, {0xB72,0xB77}, {0xBF0,0xBF2},
    {0xC78,0xC7E}, {0xD58,0xD5E}, {0xD70,0xD78}, {0xF2A,0xF33}, {0x1369,0x137C},
    {0x16EE,0x16F0}, {0x17F0,0x17F9}, {0x19DA,0x19DA}, {0x2070,0x2070}, {0x2074,0x2079},
    {0x2080,0x2089}, {0x2150,0x2182}, {0x2185,0x2189}, {0x2460,0x249B}, {0x24EA,0x24FF},
    {0x2776,0x2793}, {0x2CFD,0x2CFD}, {0x3007,0x3007}, {0x3021,0x3029}, {0x3038,0x303A},
    {0x3192,0x3195}, {0x3220,0x3229}, {0x3248,0x324F}, {0x3251,0x325F}, {0x3280,0x3289},
    {0x32B1,0x32BF}, {0xA6E6,0xA6EF}, {0xA830,0xA835}, {0x10107,0x10133}, {0x10140,0x10178},
    {0x1018A,0x1018B}, {0x102E1,0x102FB}, {0x10320,0x10323}, {0x10341,0x10341},
    {0x1034A,0x1034A}, {0x103D1,0x103D5}, {0x10858,0x1085F}, {0x10879,0x1087F},
    {0x108A7,0x108AF}, {0x108FB,0x108FF}, {0x10916,0x1091B}, {0x109BC,0x109BD},
    {0x109C0,0x109CF}, {0x109D2,0x109FF}, {0x10A40,0x10A48}, {0x10A7D,0x10A7E},
    {0x10A9D,0x10A9F}, {0x10AEB,0x10AEF}, {0x10B58,0x10B5F}, {0x10B78,0x10B7F},
    {0x10BA9,0x10BAF}, {0x10CFA,0x10CFF}, {0x10E60,0x10E7E}, {0x10F1D,0x10F26},
    {0x10F51,0x10F54}, {0x10FC5,0x10FCB}, {0x11052,0x11065}, {0x111E1,0x111F4},
    {0x1173A,0x1173B}, {0x118EA,0x118F2}, {0x11C5A,0x11C6C}, {0x11FC0,0x11FD4},
    {0x12400,0x1246E}, {0x16B5B,0x16B61}, {0x16E80,0x16E96}, {0x1D2C0,0x1D2D3},
    {0x1D2E0,0x1D2F3}, {0x1D360,0x1D378}, {0x1E8C7,0x1E8CF}, {0x1EC71,0x1ECAB},
    {0x1ECAD,0x1ECAF}, {0x1ECB1,0x1ECB4}, {0x1ED01,0x1ED2D}, {0x1ED2F,0x1ED3D},
    {0x1F100,0x1F10C},
};

static int in_ranges(const unsigned (*r)[2], int n, unsigned c)
{
    int lo = 0, hi = n - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (c < r[mid][0])
            hi = mid - 1;
        else if (c > r[mid][1])
            lo = mid + 1;
        else
            return mid + 1;
    }
    return 0;
}
#define NRANGES(r) ((int)(sizeof r / sizeof *r))

/* str.isdigit(): characters that are digits but not decimal (superscripts)
 * make Python's int() fail, so only decimal digits count here. */
static int is_digit_cp(unsigned c)
{
    return (c >= '0' && c <= '9') || (c >= 0x80 && in_ranges(DECIMAL, NRANGES(DECIMAL), c));
}

/* Value of a digit as an ASCII character (int() accepts any decimal digit). */
static char digit_char(unsigned c)
{
    if (c < 0x80)
        return (char)c;
    int k = in_ranges(DECIMAL, NRANGES(DECIMAL), c);
    return (char)('0' + (c - DECIMAL[k - 1][0]) % 10);
}

static int is_alpha_cp(unsigned c)
{
    if (c < 0x80)
        return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
    return in_ranges(ALPHA, NRANGES(ALPHA), c) != 0;
}

static int is_alnum_cp(unsigned c)
{
    return is_alpha_cp(c) || is_digit_cp(c) ||
           (c >= 0x80 && in_ranges(NUMERIC, NRANGES(NUMERIC), c));
}

/* Python's str.isupper() and str.upper() outside ASCII (generated from Python 3.13.13):
 * runs {first, last, step} of uppercase letters, runs {first, last, step, delta}
 * of one-character upper cases, and the multi-character upper cases. */
typedef struct { unsigned first, last, step; int delta; } Run;
static const Run ISUPPER[] = {
    {0xC0,0xD6,1,0}, {0xD8,0xDE,1,0}, {0x100,0x136,2,0}, {0x139,0x147,2,0}, {0x14A,0x178,2,0},
    {0x179,0x17D,2,0}, {0x181,0x182,1,0}, {0x184,0x186,2,0}, {0x187,0x189,2,0},
    {0x18A,0x18B,1,0}, {0x18E,0x191,1,0}, {0x193,0x194,1,0}, {0x196,0x198,1,0},
    {0x19C,0x19D,1,0}, {0x19F,0x1A0,1,0}, {0x1A2,0x1A6,2,0}, {0x1A7,0x1A9,2,0},
    {0x1AC,0x1AE,2,0}, {0x1AF,0x1B1,2,0}, {0x1B2,0x1B3,1,0}, {0x1B5,0x1B7,2,0},
    {0x1B8,0x1B8,1,0}, {0x1BC,0x1BC,1,0}, {0x1C4,0x1C4,1,0}, {0x1C7,0x1C7,1,0},
    {0x1CA,0x1CA,1,0}, {0x1CD,0x1DB,2,0}, {0x1DE,0x1EE,2,0}, {0x1F1,0x1F1,1,0},
    {0x1F4,0x1F6,2,0}, {0x1F7,0x1F8,1,0}, {0x1FA,0x232,2,0}, {0x23A,0x23B,1,0},
    {0x23D,0x23E,1,0}, {0x241,0x243,2,0}, {0x244,0x246,1,0}, {0x248,0x24E,2,0},
    {0x370,0x372,2,0}, {0x376,0x376,1,0}, {0x37F,0x37F,1,0}, {0x386,0x388,2,0},
    {0x389,0x38A,1,0}, {0x38C,0x38E,2,0}, {0x38F,0x391,2,0}, {0x392,0x3A1,1,0},
    {0x3A3,0x3AB,1,0}, {0x3CF,0x3CF,1,0}, {0x3D2,0x3D4,1,0}, {0x3D8,0x3EE,2,0},
    {0x3F4,0x3F4,1,0}, {0x3F7,0x3F9,2,0}, {0x3FA,0x3FA,1,0}, {0x3FD,0x42F,1,0},
    {0x460,0x480,2,0}, {0x48A,0x4C0,2,0}, {0x4C1,0x4CD,2,0}, {0x4D0,0x52E,2,0},
    {0x531,0x556,1,0}, {0x10A0,0x10C5,1,0}, {0x10C7,0x10C7,1,0}, {0x10CD,0x10CD,1,0},
    {0x13A0,0x13F5,1,0}, {0x1C90,0x1CBA,1,0}, {0x1CBD,0x1CBF,1,0}, {0x1E00,0x1E94,2,0},
    {0x1E9E,0x1EFE,2,0}, {0x1F08,0x1F0F,1,0}, {0x1F18,0x1F1D,1,0}, {0x1F28,0x1F2F,1,0},
    {0x1F38,0x1F3F,1,0}, {0x1F48,0x1F4D,1,0}, {0x1F59,0x1F5F,2,0}, {0x1F68,0x1F6F,1,0},
    {0x1FB8,0x1FBB,1,0}, {0x1FC8,0x1FCB,1,0}, {0x1FD8,0x1FDB,1,0}, {0x1FE8,0x1FEC,1,0},
    {0x1FF8,0x1FFB,1,0}, {0x2102,0x2102,1,0}, {0x2107,0x2107,1,0}, {0x210B,0x210D,1,0},
    {0x2110,0x2112,1,0}, {0x2115,0x2115,1,0}, {0x2119,0x211D,1,0}, {0x2124,0x212A,2,0},
    {0x212B,0x212D,1,0}, {0x2130,0x2133,1,0}, {0x213E,0x213F,1,0}, {0x2145,0x2145,1,0},
    {0x2160,0x216F,1,0}, {0x2183,0x2183,1,0}, {0x24B6,0x24CF,1,0}, {0x2C00,0x2C2F,1,0},
    {0x2C60,0x2C62,2,0}, {0x2C63,0x2C64,1,0}, {0x2C67,0x2C6D,2,0}, {0x2C6E,0x2C70,1,0},
    {0x2C72,0x2C72,1,0}, {0x2C75,0x2C75,1,0}, {0x2C7E,0x2C80,1,0}, {0x2C82,0x2CE2,2,0},
    {0x2CEB,0x2CED,2,0}, {0x2CF2,0x2CF2,1,0}, {0xA640,0xA66C,2,0}, {0xA680,0xA69A,2,0},
    {0xA722,0xA72E,2,0}, {0xA732,0xA76E,2,0}, {0xA779,0xA77D,2,0}, {0xA77E,0xA786,2,0},
    {0xA78B,0xA78D,2,0}, {0xA790,0xA792,2,0}, {0xA796,0xA7AA,2,0}, {0xA7AB,0xA7AE,1,0},
    {0xA7B0,0xA7B4,1,0}, {0xA7B6,0xA7C4,2,0}, {0xA7C5,0xA7C7,1,0}, {0xA7C9,0xA7C9,1,0},
    {0xA7D0,0xA7D0,1,0}, {0xA7D6,0xA7D8,2,0}, {0xA7F5,0xA7F5,1,0}, {0xFF21,0xFF3A,1,0},
    {0x10400,0x10427,1,0}, {0x104B0,0x104D3,1,0}, {0x10570,0x1057A,1,0}, {0x1057C,0x1058A,1,0},
    {0x1058C,0x10592,1,0}, {0x10594,0x10595,1,0}, {0x10C80,0x10CB2,1,0}, {0x118A0,0x118BF,1,0},
    {0x16E40,0x16E5F,1,0}, {0x1D400,0x1D419,1,0}, {0x1D434,0x1D44D,1,0}, {0x1D468,0x1D481,1,0},
    {0x1D49C,0x1D49E,2,0}, {0x1D49F,0x1D49F,1,0}, {0x1D4A2,0x1D4A2,1,0}, {0x1D4A5,0x1D4A6,1,0},
    {0x1D4A9,0x1D4AC,1,0}, {0x1D4AE,0x1D4B5,1,0}, {0x1D4D0,0x1D4E9,1,0}, {0x1D504,0x1D505,1,0},
    {0x1D507,0x1D50A,1,0}, {0x1D50D,0x1D514,1,0}, {0x1D516,0x1D51C,1,0}, {0x1D538,0x1D539,1,0},
    {0x1D53B,0x1D53E,1,0}, {0x1D540,0x1D544,1,0}, {0x1D546,0x1D546,1,0}, {0x1D54A,0x1D550,1,0},
    {0x1D56C,0x1D585,1,0}, {0x1D5A0,0x1D5B9,1,0}, {0x1D5D4,0x1D5ED,1,0}, {0x1D608,0x1D621,1,0},
    {0x1D63C,0x1D655,1,0}, {0x1D670,0x1D689,1,0}, {0x1D6A8,0x1D6C0,1,0}, {0x1D6E2,0x1D6FA,1,0},
    {0x1D71C,0x1D734,1,0}, {0x1D756,0x1D76E,1,0}, {0x1D790,0x1D7A8,1,0}, {0x1D7CA,0x1D7CA,1,0},
    {0x1E900,0x1E921,1,0}, {0x1F130,0x1F149,1,0}, {0x1F150,0x1F169,1,0}, {0x1F170,0x1F189,1,0},
};
static const Run UPPER[] = {
    {0xB5,0xB5,1,743}, {0xE0,0xF6,1,-32}, {0xF8,0xFE,1,-32}, {0xFF,0xFF,1,121},
    {0x101,0x12F,2,-1}, {0x131,0x131,1,-232}, {0x133,0x137,2,-1}, {0x13A,0x148,2,-1},
    {0x14B,0x177,2,-1}, {0x17A,0x17E,2,-1}, {0x17F,0x17F,1,-300}, {0x180,0x180,1,195},
    {0x183,0x185,2,-1}, {0x188,0x188,1,-1}, {0x18C,0x18C,1,-1}, {0x192,0x192,1,-1},
    {0x195,0x195,1,97}, {0x199,0x199,1,-1}, {0x19A,0x19A,1,163}, {0x19E,0x19E,1,130},
    {0x1A1,0x1A5,2,-1}, {0x1A8,0x1A8,1,-1}, {0x1AD,0x1AD,1,-1}, {0x1B0,0x1B0,1,-1},
    {0x1B4,0x1B6,2,-1}, {0x1B9,0x1B9,1,-1}, {0x1BD,0x1BD,1,-1}, {0x1BF,0x1BF,1,56},
    {0x1C5,0x1C5,1,-1}, {0x1C6,0x1C6,1,-2}, {0x1C8,0x1C8,1,-1}, {0x1C9,0x1C9,1,-2},
    {0x1CB,0x1CB,1,-1}, {0x1CC,0x1CC,1,-2}, {0x1CE,0x1DC,2,-1}, {0x1DD,0x1DD,1,-79},
    {0x1DF,0x1EF,2,-1}, {0x1F2,0x1F2,1,-1}, {0x1F3,0x1F3,1,-2}, {0x1F5,0x1F5,1,-1},
    {0x1F9,0x21F,2,-1}, {0x223,0x233,2,-1}, {0x23C,0x23C,1,-1}, {0x23F,0x240,1,10815},
    {0x242,0x242,1,-1}, {0x247,0x24F,2,-1}, {0x250,0x250,1,10783}, {0x251,0x251,1,10780},
    {0x252,0x252,1,10782}, {0x253,0x253,1,-210}, {0x254,0x254,1,-206}, {0x256,0x257,1,-205},
    {0x259,0x259,1,-202}, {0x25B,0x25B,1,-203}, {0x25C,0x25C,1,42319}, {0x260,0x260,1,-205},
    {0x261,0x261,1,42315}, {0x263,0x263,1,-207}, {0x265,0x265,1,42280}, {0x266,0x266,1,42308},
    {0x268,0x268,1,-209}, {0x269,0x269,1,-211}, {0x26A,0x26A,1,42308}, {0x26B,0x26B,1,10743},
    {0x26C,0x26C,1,42305}, {0x26F,0x26F,1,-211}, {0x271,0x271,1,10749}, {0x272,0x272,1,-213},
    {0x275,0x275,1,-214}, {0x27D,0x27D,1,10727}, {0x280,0x280,1,-218}, {0x282,0x282,1,42307},
    {0x283,0x283,1,-218}, {0x287,0x287,1,42282}, {0x288,0x288,1,-218}, {0x289,0x289,1,-69},
    {0x28A,0x28B,1,-217}, {0x28C,0x28C,1,-71}, {0x292,0x292,1,-219}, {0x29D,0x29D,1,42261},
    {0x29E,0x29E,1,42258}, {0x345,0x345,1,84}, {0x371,0x373,2,-1}, {0x377,0x377,1,-1},
    {0x37B,0x37D,1,130}, {0x3AC,0x3AC,1,-38}, {0x3AD,0x3AF,1,-37}, {0x3B1,0x3C1,1,-32},
    {0x3C2,0x3C2,1,-31}, {0x3C3,0x3CB,1,-32}, {0x3CC,0x3CC,1,-64}, {0x3CD,0x3CE,1,-63},
    {0x3D0,0x3D0,1,-62}, {0x3D1,0x3D1,1,-57}, {0x3D5,0x3D5,1,-47}, {0x3D6,0x3D6,1,-54},
    {0x3D7,0x3D7,1,-8}, {0x3D9,0x3EF,2,-1}, {0x3F0,0x3F0,1,-86}, {0x3F1,0x3F1,1,-80},
    {0x3F2,0x3F2,1,7}, {0x3F3,0x3F3,1,-116}, {0x3F5,0x3F5,1,-96}, {0x3F8,0x3F8,1,-1},
    {0x3FB,0x3FB,1,-1}, {0x430,0x44F,1,-32}, {0x450,0x45F,1,-80}, {0x461,0x481,2,-1},
    {0x48B,0x4BF,2,-1}, {0x4C2,0x4CE,2,-1}, {0x4CF,0x4CF,1,-15}, {0x4D1,0x52F,2,-1},
    {0x561,0x586,1,-48}, {0x10D0,0x10FA,1,3008}, {0x10FD,0x10FF,1,3008}, {0x13F8,0x13FD,1,-8},
    {0x1C80,0x1C80,1,-6254}, {0x1C81,0x1C81,1,-6253}, {0x1C82,0x1C82,1,-6244},
    {0x1C83,0x1C84,1,-6242}, {0x1C85,0x1C85,1,-6243}, {0x1C86,0x1C86,1,-6236},
    {0x1C87,0x1C87,1,-6181}, {0x1C88,0x1C88,1,35266}, {0x1D79,0x1D79,1,35332},
    {0x1D7D,0x1D7D,1,3814}, {0x1D8E,0x1D8E,1,35384}, {0x1E01,0x1E95,2,-1},
    {0x1E9B,0x1E9B,1,-59}, {0x1EA1,0x1EFF,2,-1}, {0x1F00,0x1F07,1,8}, {0x1F10,0x1F15,1,8},
    {0x1F20,0x1F27,1,8}, {0x1F30,0x1F37,1,8}, {0x1F40,0x1F45,1,8}, {0x1F51,0x1F57,2,8},
    {0x1F60,0x1F67,1,8}, {0x1F70,0x1F71,1,74}, {0x1F72,0x1F75,1,86}, {0x1F76,0x1F77,1,100},
    {0x1F78,0x1F79,1,128}, {0x1F7A,0x1F7B,1,112}, {0x1F7C,0x1F7D,1,126}, {0x1FB0,0x1FB1,1,8},
    {0x1FBE,0x1FBE,1,-7205}, {0x1FD0,0x1FD1,1,8}, {0x1FE0,0x1FE1,1,8}, {0x1FE5,0x1FE5,1,7},
    {0x214E,0x214E,1,-28}, {0x2170,0x217F,1,-16}, {0x2184,0x2184,1,-1}, {0x24D0,0x24E9,1,-26},
    {0x2C30,0x2C5F,1,-48}, {0x2C61,0x2C61,1,-1}, {0x2C65,0x2C65,1,-10795},
    {0x2C66,0x2C66,1,-10792}, {0x2C68,0x2C6C,2,-1}, {0x2C73,0x2C73,1,-1}, {0x2C76,0x2C76,1,-1},
    {0x2C81,0x2CE3,2,-1}, {0x2CEC,0x2CEE,2,-1}, {0x2CF3,0x2CF3,1,-1}, {0x2D00,0x2D25,1,-7264},
    {0x2D27,0x2D27,1,-7264}, {0x2D2D,0x2D2D,1,-7264}, {0xA641,0xA66D,2,-1},
    {0xA681,0xA69B,2,-1}, {0xA723,0xA72F,2,-1}, {0xA733,0xA76F,2,-1}, {0xA77A,0xA77C,2,-1},
    {0xA77F,0xA787,2,-1}, {0xA78C,0xA78C,1,-1}, {0xA791,0xA793,2,-1}, {0xA794,0xA794,1,48},
    {0xA797,0xA7A9,2,-1}, {0xA7B5,0xA7C3,2,-1}, {0xA7C8,0xA7CA,2,-1}, {0xA7D1,0xA7D1,1,-1},
    {0xA7D7,0xA7D9,2,-1}, {0xA7F6,0xA7F6,1,-1}, {0xAB53,0xAB53,1,-928},
    {0xAB70,0xABBF,1,-38864}, {0xFF41,0xFF5A,1,-32}, {0x10428,0x1044F,1,-40},
    {0x104D8,0x104FB,1,-40}, {0x10597,0x105A1,1,-39}, {0x105A3,0x105B1,1,-39},
    {0x105B3,0x105B9,1,-39}, {0x105BB,0x105BC,1,-39}, {0x10CC0,0x10CF2,1,-64},
    {0x118C0,0x118DF,1,-32}, {0x16E60,0x16E7F,1,-32}, {0x1E922,0x1E943,1,-34},
};
static const struct { unsigned cp; const char *up; } UPPER_MULTI[] = {
    { 0xDF, "\123\123" },
    { 0x149, "\312\274\116" },
    { 0x1F0, "\112\314\214" },
    { 0x390, "\316\231\314\210\314\201" },
    { 0x3B0, "\316\245\314\210\314\201" },
    { 0x587, "\324\265\325\222" },
    { 0x1E96, "\110\314\261" },
    { 0x1E97, "\124\314\210" },
    { 0x1E98, "\127\314\212" },
    { 0x1E99, "\131\314\212" },
    { 0x1E9A, "\101\312\276" },
    { 0x1F50, "\316\245\314\223" },
    { 0x1F52, "\316\245\314\223\314\200" },
    { 0x1F54, "\316\245\314\223\314\201" },
    { 0x1F56, "\316\245\314\223\315\202" },
    { 0x1F80, "\341\274\210\316\231" },
    { 0x1F81, "\341\274\211\316\231" },
    { 0x1F82, "\341\274\212\316\231" },
    { 0x1F83, "\341\274\213\316\231" },
    { 0x1F84, "\341\274\214\316\231" },
    { 0x1F85, "\341\274\215\316\231" },
    { 0x1F86, "\341\274\216\316\231" },
    { 0x1F87, "\341\274\217\316\231" },
    { 0x1F88, "\341\274\210\316\231" },
    { 0x1F89, "\341\274\211\316\231" },
    { 0x1F8A, "\341\274\212\316\231" },
    { 0x1F8B, "\341\274\213\316\231" },
    { 0x1F8C, "\341\274\214\316\231" },
    { 0x1F8D, "\341\274\215\316\231" },
    { 0x1F8E, "\341\274\216\316\231" },
    { 0x1F8F, "\341\274\217\316\231" },
    { 0x1F90, "\341\274\250\316\231" },
    { 0x1F91, "\341\274\251\316\231" },
    { 0x1F92, "\341\274\252\316\231" },
    { 0x1F93, "\341\274\253\316\231" },
    { 0x1F94, "\341\274\254\316\231" },
    { 0x1F95, "\341\274\255\316\231" },
    { 0x1F96, "\341\274\256\316\231" },
    { 0x1F97, "\341\274\257\316\231" },
    { 0x1F98, "\341\274\250\316\231" },
    { 0x1F99, "\341\274\251\316\231" },
    { 0x1F9A, "\341\274\252\316\231" },
    { 0x1F9B, "\341\274\253\316\231" },
    { 0x1F9C, "\341\274\254\316\231" },
    { 0x1F9D, "\341\274\255\316\231" },
    { 0x1F9E, "\341\274\256\316\231" },
    { 0x1F9F, "\341\274\257\316\231" },
    { 0x1FA0, "\341\275\250\316\231" },
    { 0x1FA1, "\341\275\251\316\231" },
    { 0x1FA2, "\341\275\252\316\231" },
    { 0x1FA3, "\341\275\253\316\231" },
    { 0x1FA4, "\341\275\254\316\231" },
    { 0x1FA5, "\341\275\255\316\231" },
    { 0x1FA6, "\341\275\256\316\231" },
    { 0x1FA7, "\341\275\257\316\231" },
    { 0x1FA8, "\341\275\250\316\231" },
    { 0x1FA9, "\341\275\251\316\231" },
    { 0x1FAA, "\341\275\252\316\231" },
    { 0x1FAB, "\341\275\253\316\231" },
    { 0x1FAC, "\341\275\254\316\231" },
    { 0x1FAD, "\341\275\255\316\231" },
    { 0x1FAE, "\341\275\256\316\231" },
    { 0x1FAF, "\341\275\257\316\231" },
    { 0x1FB2, "\341\276\272\316\231" },
    { 0x1FB3, "\316\221\316\231" },
    { 0x1FB4, "\316\206\316\231" },
    { 0x1FB6, "\316\221\315\202" },
    { 0x1FB7, "\316\221\315\202\316\231" },
    { 0x1FBC, "\316\221\316\231" },
    { 0x1FC2, "\341\277\212\316\231" },
    { 0x1FC3, "\316\227\316\231" },
    { 0x1FC4, "\316\211\316\231" },
    { 0x1FC6, "\316\227\315\202" },
    { 0x1FC7, "\316\227\315\202\316\231" },
    { 0x1FCC, "\316\227\316\231" },
    { 0x1FD2, "\316\231\314\210\314\200" },
    { 0x1FD3, "\316\231\314\210\314\201" },
    { 0x1FD6, "\316\231\315\202" },
    { 0x1FD7, "\316\231\314\210\315\202" },
    { 0x1FE2, "\316\245\314\210\314\200" },
    { 0x1FE3, "\316\245\314\210\314\201" },
    { 0x1FE4, "\316\241\314\223" },
    { 0x1FE6, "\316\245\315\202" },
    { 0x1FE7, "\316\245\314\210\315\202" },
    { 0x1FF2, "\341\277\272\316\231" },
    { 0x1FF3, "\316\251\316\231" },
    { 0x1FF4, "\316\217\316\231" },
    { 0x1FF6, "\316\251\315\202" },
    { 0x1FF7, "\316\251\315\202\316\231" },
    { 0x1FFC, "\316\251\316\231" },
    { 0xFB00, "\106\106" },
    { 0xFB01, "\106\111" },
    { 0xFB02, "\106\114" },
    { 0xFB03, "\106\106\111" },
    { 0xFB04, "\106\106\114" },
    { 0xFB05, "\123\124" },
    { 0xFB06, "\123\124" },
    { 0xFB13, "\325\204\325\206" },
    { 0xFB14, "\325\204\324\265" },
    { 0xFB15, "\325\204\324\273" },
    { 0xFB16, "\325\216\325\206" },
    { 0xFB17, "\325\204\324\275" },
};

static int in_run(const Run *r, size_t n, unsigned c, int *delta)
{
    for (size_t i = 0; i < n; i++)
        if (c >= r[i].first && c <= r[i].last && (c - r[i].first) % r[i].step == 0) {
            *delta = r[i].delta;
            return 1;
        }
    return 0;
}

static int is_upper_cp(unsigned c)
{
    int d;
    if (c < 0x80)
        return c >= 'A' && c <= 'Z';
    return in_run(ISUPPER, sizeof ISUPPER / sizeof *ISUPPER, c, &d);
}

static void put_utf8(Buf *b, unsigned c)
{
    if (c < 0x80) {
        buf_putc(b, (char)c);
    } else if (c < 0x800) {
        buf_putc(b, (char)(0xC0 | c >> 6));
        buf_putc(b, (char)(0x80 | (c & 63)));
    } else if (c < 0x10000) {
        buf_putc(b, (char)(0xE0 | c >> 12));
        buf_putc(b, (char)(0x80 | (c >> 6 & 63)));
        buf_putc(b, (char)(0x80 | (c & 63)));
    } else {
        buf_putc(b, (char)(0xF0 | c >> 18));
        buf_putc(b, (char)(0x80 | (c >> 12 & 63)));
        buf_putc(b, (char)(0x80 | (c >> 6 & 63)));
        buf_putc(b, (char)(0x80 | (c & 63)));
    }
}

/* str.upper() of one letter, appended as UTF-8. */
static void put_upper(Buf *b, unsigned c)
{
    int d;
    if (c < 0x80) {
        buf_putc(b, (char)(c >= 'a' && c <= 'z' ? c - 32 : c));
        return;
    }
    for (size_t i = 0; i < sizeof UPPER_MULTI / sizeof *UPPER_MULTI; i++)
        if (UPPER_MULTI[i].cp == c) {
            buf_puts(b, UPPER_MULTI[i].up);
            return;
        }
    if (in_run(UPPER, sizeof UPPER / sizeof *UPPER, c, &d))
        c = (unsigned)((int)c + d);
    put_utf8(b, c);
}

/* Decode one UTF-8 character of a token; returns its code point. */
static unsigned first_cp(const char *s, int *len)
{
    const unsigned char *u = (const unsigned char *)s;
    if (u[0] < 0x80) { *len = 1; return u[0]; }
    if ((u[0] & 0xE0) == 0xC0 && u[1]) { *len = 2; return (u[0] & 31u) << 6 | (u[1] & 63u); }
    if ((u[0] & 0xF0) == 0xE0 && u[1] && u[2]) {
        *len = 3;
        return (u[0] & 15u) << 12 | (u[1] & 63u) << 6 | (u[2] & 63u);
    }
    if (u[1] && u[2] && u[3]) {
        *len = 4;
        return (u[0] & 7u) << 18 | (u[1] & 63u) << 12 | (u[2] & 63u) << 6 | (u[3] & 63u);
    }
    *len = 1;
    return u[0];
}

static int char_len(const char *s)
{
    int n = 0, k;
    while (*s) {
        first_cp(s, &k);
        s += k;
        n++;
    }
    return n;
}

/* --------------------------------------------------- input decoding */
static const unsigned short CP1252[32] = {
    0x20AC, 0x81, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021,
    0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0x8D, 0x017D, 0x8F,
    0x90, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
    0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0x9D, 0x017E, 0x0178 };

/* One UTF-8 sequence at s, or 0 if it is not valid there. */
static int utf8_seq(const unsigned char *s, unsigned *cp)
{
    unsigned c = s[0];
    int len;
    unsigned lo = 0x80, hi = 0xBF;
    if (c >= 0xC2 && c <= 0xDF) len = 2, c &= 31;
    else if (c >= 0xE0 && c <= 0xEF) {
        len = 3;
        if (c == 0xE0) lo = 0xA0;
        if (c == 0xED) hi = 0x9F;
        c &= 15;
    } else if (c >= 0xF0 && c <= 0xF4) {
        len = 4;
        if (c == 0xF0) lo = 0x90;
        if (c == 0xF4) hi = 0x8F;
        c &= 7;
    } else
        return 0;
    for (int k = 1; k < len; k++) {
        unsigned b = s[k];
        if (b < (k == 1 ? lo : 0x80) || b > (k == 1 ? hi : 0xBF))
            return 0;
        c = c << 6 | (b & 63);
    }
    *cp = c;
    return len;
}

/* FOLD: typographic and accented characters folded to ASCII. */
static unsigned fold(unsigned c)
{
    static const char LATIN1[] =        /* U+00C0 .. U+00FF, 0 = unchanged */
        "AAAAAA\0CEEEEIIII\0NOOOOO\0\0UUUUY\0\0"
        "aaaaaa\0ceeeeiiii\0nooooo\0\0uuuuy\0y";
    if (c >= 0xC0 && c <= 0xFF)
        return LATIN1[c - 0xC0] ? (unsigned char)LATIN1[c - 0xC0] : c;
    switch (c) {
    case 0x2018: case 0x2019: return '\'';
    case 0x201C: case 0x201D: return '"';
    case 0x2013: case 0x2014: return '-';
    case 0x178: return 'Y';
    default: return c;
    }
}

typedef struct { unsigned *c; long n; } Txt;

static unsigned at(const Txt *t, long i)
{
    return i >= 0 && i < t->n ? t->c[i] : 0;
}

/* UTF-8 if the whole input is valid UTF-8, else Windows-1252. */
static void decode(const char *text, Txt *t)
{
    const unsigned char *s = (const unsigned char *)text;
    unsigned cp;
    int utf8 = 1;
    for (const unsigned char *p = s; *p && utf8; ) {
        int len = *p < 0x80 ? 1 : utf8_seq(p, &cp);
        utf8 = len > 0;
        p += len;
    }
    t->c = xmalloc((strlen(text) + 1) * sizeof *t->c);
    t->n = 0;
    while (*s) {
        int len = 1;
        if (*s < 0x80)
            cp = *s;
        else if (utf8)
            len = utf8_seq(s, &cp);
        else
            cp = *s < 0xA0 ? CP1252[*s - 0x80] : *s;
        t->c[t->n++] = fold(cp);
        s += len;
    }
}

/* -------------------------------------------------------- number words */
static const char *const ONES[20] = {
    "zero", "one", "two", "three", "four", "five", "six", "seven", "eight", "nine", "ten",
    "eleven", "twelve", "thirteen", "fourteen", "fifteen", "sixteen", "seventeen",
    "eighteen", "nineteen" };
static const char *const TENS[10] = {
    "_", "_", "twenty", "thirty", "forty", "fifty", "sixty", "seventy", "eighty", "ninety" };
static const struct { long long value; const char *name; } SCALES[5] = {
    { 1000000000000LL, "trillion" }, { 1000000000LL, "billion" }, { 1000000LL, "million" },
    { 1000LL, "thousand" }, { 100LL, "hundred" } };
static const char *const ORDINAL_FIX[7][2] = {
    { "one", "first" }, { "two", "second" }, { "three", "third" }, { "five", "fifth" },
    { "eight", "eighth" }, { "nine", "ninth" }, { "twelve", "twelfth" } };
static const char *const SCALE_WORDS[3] = { "million", "billion", "trillion" };
#define MAX_DIGITS 15

static const char *const TITLES[8][2] = {
    { "mr", "mister" }, { "mrs", "missus" }, { "ms", "miz" }, { "dr", "doctor" },
    { "prof", "professor" }, { "mt", "mount" }, { "jr", "junior" }, { "sr", "senior" } };
static const char *const TRAILING[2][2] = { { "etc", "et cetera" }, { "vs", "versus" } };
static const char *const DOTTED[4][2] = {
    { "e.g.", "for example" }, { "i.e.", "that is" }, { "a.m.", "A M" }, { "p.m.", "P M" } };

static const char *symbol_word(unsigned c)
{
    switch (c) {
    case '&': return "and";
    case '%': return "percent";
    case '+': return "plus";
    case '=': return "equals";
    case '@': return "at";
    case '#': return "number";
    default: return NULL;
    }
}

static void number_words(Buf *b, long long n)
{
    if (n < 20) {
        buf_puts(b, ONES[n]);
        return;
    }
    if (n < 100) {
        buf_puts(b, TENS[n / 10]);
        if (n % 10) {
            buf_putc(b, ' ');
            buf_puts(b, ONES[n % 10]);
        }
        return;
    }
    for (int s = 0; s < 5; s++) {
        if (n >= SCALES[s].value) {
            long long head = n / SCALES[s].value, rest = n % SCALES[s].value;
            number_words(b, head);
            buf_putc(b, ' ');
            buf_puts(b, SCALES[s].name);
            if (rest) {
                buf_putc(b, ' ');
                number_words(b, rest);
            }
            return;
        }
    }
}

static void digit_words(Buf *b, const char *digits)
{
    for (const char *d = digits; *d; d++) {
        if (d != digits)
            buf_putc(b, ' ');
        buf_puts(b, ONES[*d - '0']);
    }
}

static long long to_int(const char *digits, size_t len)
{
    long long v = 0;
    for (size_t i = 0; i < len; i++)
        v = v * 10 + (digits[i] - '0');
    return v;
}

/* Words for a digit string (no commas). */
static void cardinal(Buf *b, const char *digits)
{
    size_t len = strlen(digits);
    if (len > MAX_DIGITS)
        digit_words(b, digits);
    else
        number_words(b, to_int(digits, len));
}

static void year_words(Buf *b, long long n)
{
    long long hi = n / 100, lo = n % 100;
    number_words(b, hi);
    if (lo == 0) {
        buf_puts(b, " hundred");
        return;
    }
    buf_putc(b, ' ');
    if (lo < 10) {
        buf_puts(b, "oh ");
        buf_puts(b, ONES[lo]);
    } else
        number_words(b, lo);
}

static int is_year(const char *digits)
{
    if (strlen(digits) != 4)
        return 0;
    long long n = to_int(digits, 4);
    return (n >= 1100 && n <= 1999) || (n >= 2010 && n <= 2099);
}

static int ends_with(const char *s, const char *suf)
{
    size_t n = strlen(s), m = strlen(suf);
    return n >= m && !memcmp(s + n - m, suf, m);
}

/* Rewrite the last word of w (words separated by single spaces). */
static void ordinal_words(Buf *w)
{
    char *sp = strrchr(w->s, ' ');
    char *last = sp ? sp + 1 : w->s;
    for (int k = 0; k < 7; k++) {
        if (!strcmp(last, ORDINAL_FIX[k][0])) {
            w->n = last - w->s;
            w->s[w->n] = 0;
            buf_puts(w, ORDINAL_FIX[k][1]);
            return;
        }
    }
    if (ends_with(last, "y")) {
        w->s[--w->n] = 0;
        buf_puts(w, "ieth");
    } else
        buf_puts(w, "th");
}

static void plural_words(Buf *w)
{
    char *sp = strrchr(w->s, ' ');
    char *last = sp ? sp + 1 : w->s;
    if (ends_with(last, "y")) {
        w->s[--w->n] = 0;
        buf_puts(w, "ies");
    } else if (ends_with(last, "s") || ends_with(last, "x"))
        buf_puts(w, "es");
    else
        buf_puts(w, "s");
}

/* Scan digits with optional 3-digit comma groups and a decimal part. */
static long scan_number(const Txt *t, long i, Buf *whole, Buf *frac, int *has_frac)
{
    long j = i, n = t->n;
    while (j < n && is_digit_cp(t->c[j]))
        buf_putc(whole, digit_char(t->c[j++]));
    if (whole->n <= 3) {
        while (j + 3 < n + 1 && at(t, j) == ',' && is_digit_cp(at(t, j + 1)) &&
               is_digit_cp(at(t, j + 2)) && is_digit_cp(at(t, j + 3)) &&
               !is_digit_cp(at(t, j + 4))) {
            for (int k = 1; k <= 3; k++)
                buf_putc(whole, digit_char(t->c[j + k]));
            j += 4;
        }
    }
    *has_frac = 0;
    if (at(t, j) == '.' && is_digit_cp(at(t, j + 1))) {
        long k = j + 1;
        while (k < n && is_digit_cp(t->c[k]))
            buf_putc(frac, digit_char(t->c[k++]));
        *has_frac = 1;
        j = k;
    }
    return j;
}

static unsigned ascii_lower(unsigned c)
{
    return c >= 'A' && c <= 'Z' ? c + 32 : c;
}

/* Expand the number starting at t[i] (a digit, '$', '.', or '-') into out.
 * Returns the end index, or -1 if this is not a number. */
static long number_token(const Txt *t, long i, Buf *out)
{
    long n = t->n, j = i;
    int money = 0, negative = 0, has_frac = 0;
    Buf whole = { 0 }, frac = { 0 }, w = { 0 };
    long end;

    if (t->c[j] == '-')
        negative = 1, j++;
    if (j < n && t->c[j] == '$')
        money = 1, j++;
    if (j < n && t->c[j] == '.' && j + 1 < n && is_digit_cp(t->c[j + 1])) {
        long k = j + 1;
        while (k < n && is_digit_cp(t->c[k]))
            buf_putc(&frac, digit_char(t->c[k++]));
        has_frac = 1;
        j = k;
    } else if (j < n && is_digit_cp(t->c[j])) {
        j = scan_number(t, j, &whole, &frac, &has_frac);
    } else
        return -1;
    const char *ws = buf_str(&whole), *fs = buf_str(&frac);
    if (negative)
        buf_puts(out, "minus ");

    /* suffixes: ordinal (1st, 2ND), decade (1990s, 1990's) */
    if (!money && !has_frac && whole.n && j + 2 <= n) {
        unsigned a = ascii_lower(t->c[j]), b = ascii_lower(t->c[j + 1]);
        if (((a == 's' && b == 't') || (a == 'n' && b == 'd') || (a == 'r' && b == 'd') ||
             (a == 't' && b == 'h')) && !is_alpha_cp(at(t, j + 2))) {
            cardinal(&w, ws);
            ordinal_words(&w);
            buf_puts(out, w.s);
            end = j + 2;
            goto done;
        }
    }
    int dec = at(t, j) == '\'' && at(t, j + 1) == 's' ? 2 : at(t, j) == 's' ? 1 : 0;
    if (!money && !has_frac && dec && !is_alpha_cp(at(t, j + dec)) &&
        (is_year(ws) || (whole.n == 2 && ws[1] == '0'))) {
        if (whole.n == 4)
            year_words(&w, to_int(ws, 4));
        else
            number_words(&w, to_int(ws, 2));
        plural_words(&w);
        buf_puts(out, w.s);
        end = j + dec;
        goto done;
    }

    if (money) {
        /* "$1.5 million" -> "one point five million dollars" */
        long k = j;
        while (k < n && t->c[k] == ' ')
            k++;
        for (int s = 0; s < 3; s++) {
            const char *sw = SCALE_WORDS[s];
            long len = (long)strlen(sw), m;
            if (k + len > n)
                continue;
            for (m = 0; m < len && ascii_lower(t->c[k + m]) == (unsigned char)sw[m]; m++)
                ;
            if (m < len || is_alpha_cp(at(t, k + len)))
                continue;
            cardinal(out, whole.n ? ws : "0");
            if (has_frac) {
                buf_puts(out, " point ");
                digit_words(out, fs);
            }
            buf_putc(out, ' ');
            buf_puts(out, sw);
            buf_puts(out, " dollars");
            end = k + len;
            goto done;
        }
        /* dollars = int(whole): only zero / one matter */
        const char *d = ws;
        while (*d == '0')
            d++;
        int dollars_nonzero = *d != 0, dollars_one = !strcmp(d, "1");
        int cents = 0;
        if (has_frac)
            cents = (fs[0] - '0') * 10 + (fs[1] ? fs[1] - '0' : 0);
        int any = 0;
        if (dollars_nonzero || !cents) {
            cardinal(out, whole.n ? ws : "0");
            buf_puts(out, dollars_one ? " dollar" : " dollars");
            any = 1;
        }
        if (cents) {
            if (any)
                buf_puts(out, " and ");
            number_words(out, cents);
            buf_puts(out, cents == 1 ? " cent" : " cents");
        }
        end = j;
        goto done;
    }

    if (has_frac) {
        if (whole.n)
            cardinal(out, ws);
        buf_puts(out, " point ");
        digit_words(out, fs);
        end = j;
        goto done;
    }
    if (is_year(ws))
        year_words(out, to_int(ws, 4));
    else
        cardinal(out, ws);
    end = j;
done:
    free(whole.s);
    free(frac.s);
    free(w.s);
    return end;
}

/* --------------------------------------------------------- tokenizer */
static int is_ascii_alpha(unsigned c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

/* Words (with expansions already split into words) and . , ? ! tokens. */
static void tokenize(const Txt *t, Toks *toks)
{
    long i = 0, n = t->n;
    while (i < n) {
        unsigned c = t->c[i];
        unsigned prev = i ? t->c[i - 1] : ' ';
        if (is_digit_cp(c) ||
            (c == '$' && i + 1 < n && (is_digit_cp(t->c[i + 1]) ||
                                       (t->c[i + 1] == '.' && i + 2 < n && is_digit_cp(t->c[i + 2])))) ||
            (c == '.' && i + 1 < n && is_digit_cp(t->c[i + 1]) && !is_digit_cp(prev)) ||
            (c == '-' && !is_alnum_cp(prev) && i + 1 < n &&
             (is_digit_cp(t->c[i + 1]) || t->c[i + 1] == '$'))) {
            Buf out = { 0 };
            long end = number_token(t, i, &out);
            if (end >= 0) {
                toks_add_words(toks, buf_str(&out));
                free(out.s);
                i = end;
                continue;
            }
            free(out.s);
        }
        if (c == '\'' && is_digit_cp(at(t, i + 1)) && is_digit_cp(at(t, i + 2)) &&
            at(t, i + 3) == 's') {
            Buf w = { 0 };
            number_words(&w, (digit_char(at(t, i + 1)) - '0') * 10 + (digit_char(at(t, i + 2)) - '0'));   /* '80s */
            plural_words(&w);
            toks_add_words(toks, w.s);
            free(w.s);
            i += 4;
            continue;
        }
        if (is_ascii_alpha(c)) {
            long j = i;
            while (j < n && is_ascii_alpha(t->c[j]))
                j++;
            /* dotted abbreviations: e.g. i.e. a.m. U.S. */
            if (j - i == 1 && at(t, j) == '.' && is_alpha_cp(at(t, j + 1)) && at(t, j + 2) == '.') {
                long k = j;
                unsigned letters[64];
                int nl = 0, cap = 64;
                unsigned *lp = letters;
                lp[nl++] = t->c[i];
                while (at(t, k) == '.' && is_alpha_cp(at(t, k + 1)) && !is_alpha_cp(at(t, k + 2))) {
                    if (nl == cap) {
                        unsigned *np = xmalloc(2 * cap * sizeof *np);
                        memcpy(np, lp, nl * sizeof *np);
                        if (lp != letters)
                            free(lp);
                        lp = np;
                        cap *= 2;
                    }
                    lp[nl++] = t->c[k + 1];
                    k += 2;
                }
                if (at(t, k) == '.')
                    k++;
                const char *exp = NULL;
                if (nl == 2)
                    for (int d = 0; d < 4; d++)
                        if (ascii_lower(lp[0]) == (unsigned char)DOTTED[d][0][0] &&
                            ascii_lower(lp[1]) == (unsigned char)DOTTED[d][0][2])
                            exp = DOTTED[d][1];
                if (exp)
                    toks_add_words(toks, exp);
                else {
                    for (int l = 0; l < nl; l++) {
                        Buf u = { 0 };
                        put_upper(&u, lp[l]);
                        toks_add_words(toks, u.s);
                        free(u.s);
                    }
                }
                if (lp != letters)
                    free(lp);
                i = k;
                continue;
            }
            while (at(t, j) == '\'' && is_ascii_alpha(at(t, j + 1))) {
                j++;
                while (j < n && is_ascii_alpha(t->c[j]))
                    j++;
            }
            char *w = xmalloc(j - i + 1);
            for (long k = i; k < j; k++)
                w[k - i] = (char)t->c[k];
            w[j - i] = 0;
            toks_add(toks, w);
            i = j;
            continue;
        }
        const char *sym = c < 0x80 ? symbol_word(c) : NULL;
        if (sym)
            toks_add(toks, strndup_(sym, strlen(sym)));
        else if (c == '.' || c == '!' || c == '?') {
            char p[2] = { (char)c, 0 };
            toks_add(toks, strndup_(p, 1));
        } else if (c == ',' || c == ';' || c == ':')
            toks_add(toks, strndup_(",", 1));
        i++;
    }
}

/* ---------------------------------------------------------- normalize */
static int capitalized(const char *tok)
{
    int len;
    return tok && *tok && is_upper_cp(first_cp(tok, &len));
}

static void lower_ascii(char *dst, const char *src, size_t max)
{
    size_t i;
    for (i = 0; src[i] && i + 1 < max; i++)
        dst[i] = (char)ascii_lower((unsigned char)src[i]);
    dst[i] = 0;
}

static int is_number_word(const char *s)
{
    for (int k = 0; k < 20; k++)
        if (!strcmp(s, ONES[k]))
            return 1;
    for (int k = 0; k < 10; k++)
        if (!strcmp(s, TENS[k]))
            return 1;
    return !strcmp(s, "minus");
}

static void replace_last(Toks *cur, const char *s)
{
    free(cur->v[cur->n - 1]);
    cur->v[cur->n - 1] = strndup_(s, strlen(s));
}

typedef struct { Sentence *v; int n, cap; } Sents;

/* Append cur (splitting multi-word tokens) as a sentence; empties cur. */
static void emit(Sents *ss, Toks *cur)
{
    if (ss->n == ss->cap) {
        ss->cap = ss->cap ? ss->cap * 2 : 4;
        ss->v = xrealloc(ss->v, ss->cap * sizeof *ss->v);
    }
    Toks out = { 0 };
    for (int i = 0; i < cur->n; i++)
        toks_add_words(&out, cur->v[i]);
    toks_free(cur);
    ss->v[ss->n].tok = out.v;
    ss->v[ss->n].n = out.n;
    ss->v[ss->n].cap = out.cap;
    ss->n++;
}

static int is_punct(const char *s)
{
    return !strcmp(s, ".") || !strcmp(s, "?") || !strcmp(s, ",");
}

Sentence *text_normalize(const char *text, int *nsent)
{
    Txt t;
    Toks toks = { 0 }, cur = { 0 };
    Sents ss = { 0 };

    decode(text, &t);
    tokenize(&t, &toks);
    free(t.c);
    for (int i = 0; i < toks.n; i++) {
        const char *tok = toks.v[i];
        const char *nxt = i + 1 < toks.n ? toks.v[i + 1] : NULL;
        if (!strcmp(tok, ".") || !strcmp(tok, "!") || !strcmp(tok, "?")) {
            const char *last = cur.n ? cur.v[cur.n - 1] : NULL;
            char low[16] = "";
            if (last) {
                if (strlen(last) < sizeof low)
                    lower_ascii(low, last, sizeof low);
                else
                    strcpy(low, "-");          /* too long to be an abbreviation */
            }
            if (!strcmp(tok, ".")) {
                int title = -1, trail = -1;
                for (int k = 0; k < 8; k++)
                    if (!strcmp(low, TITLES[k][0]))
                        title = k;
                for (int k = 0; k < 2; k++)
                    if (!strcmp(low, TRAILING[k][0]))
                        trail = k;
                if (title >= 0 || !strcmp(low, "st")) {
                    if (!strcmp(low, "st"))
                        replace_last(&cur, capitalized(nxt) ? "saint" : "street");
                    else
                        replace_last(&cur, TITLES[title][1]);
                    if (nxt)
                        continue;
                } else if (!strcmp(low, "no") && nxt && *nxt && is_number_word(nxt)) {
                    replace_last(&cur, "number");
                    continue;
                } else if (trail >= 0) {
                    replace_last(&cur, TRAILING[trail][1]);
                    if (nxt && !capitalized(nxt))
                        continue;
                } else if (last && *last && char_len(last) == 1 && capitalized(last) &&
                           strcmp(last, "I") && capitalized(nxt)) {
                    continue;                  /* an initial, as in "J. Allen" */
                }
            }
            if (!cur.n || is_punct(cur.v[cur.n - 1])) {
                if (cur.n && !strcmp(tok, "?"))
                    replace_last(&cur, "?");
                continue;                      /* "...", "?!" and the like */
            }
            toks_add(&cur, strndup_(!strcmp(tok, "?") ? "?" : ".", 1));
            emit(&ss, &cur);
        } else if (!strcmp(tok, ",")) {
            if (cur.n && strcmp(cur.v[cur.n - 1], ","))
                toks_add(&cur, strndup_(",", 1));
        } else
            toks_add(&cur, strndup_(tok, strlen(tok)));
    }
    if (cur.n) {
        if (!strcmp(cur.v[cur.n - 1], ",")) {
            free(cur.v[--cur.n]);
        }
        if (cur.n) {
            toks_add(&cur, strndup_(".", 1));
            emit(&ss, &cur);
        }
    }
    toks_free(&cur);
    toks_free(&toks);
    *nsent = ss.n;
    return ss.v;
}

void sentences_free(Sentence *s, int n)
{
    for (int i = 0; i < n; i++) {
        for (int k = 0; k < s[i].n; k++)
            free(s[i].tok[k]);
        free(s[i].tok);
    }
    free(s);
}
