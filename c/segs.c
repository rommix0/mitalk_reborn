/* Segment lookup and class tests shared by prosod, phono and phonet
 * (mirrors mitalk/phonet.py has()). */
#include <string.h>
#include "mitalk.h"

int seg_lookup(const char *name)
{
    for (int i = 0; i < NSEG; i++)
        if (!strcmp(SEG_NAME[i], name))
            return i;
    return -1;
}

/* True if the segment carries the feature: an Appendix C.3 class (with the
 * aliases sonor, fric, liqgli), "stress", "glotstop"/"gltstop" (QQ, TQ),
 * or a segment name such as "LL". */
int seg_has(int sym, int stress, const char *feature)
{
    if (!strcmp(feature, "sonor")) feature = "sonorant";
    else if (!strcmp(feature, "fric")) feature = "fricative";
    else if (!strcmp(feature, "liqgli")) feature = "liqglide";
    if (!strcmp(feature, "stress"))
        return stress;
    if (!strcmp(feature, "glotstop") || !strcmp(feature, "gltstop"))
        return sym == S_QQ || sym == S_TQ;
    for (int c = 0; c < NCLASS; c++)
        if (!strcmp(CLASS_NAME[c], feature))
            return (SEG_CLASSES[sym] >> c) & 1u;
    return !strcmp(SEG_NAME[sym], feature);
}
