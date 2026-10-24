/* Play synthesized speech on the default audio device.
 *
 * Windows: the waveOut API (winmm, part of every Windows install).
 * Elsewhere: a temporary WAV file handed to aplay / paplay / afplay.
 */
#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif
#include <stdio.h>
#include <stdlib.h>
#include "mitalk.h"

#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>

int audio_play(const double *x, size_t n, int rate)
{
    short *pcm = pcm16(x, n);
    WAVEFORMATEX fmt = {0};
    fmt.wFormatTag = WAVE_FORMAT_PCM;
    fmt.nChannels = 1;
    fmt.nSamplesPerSec = rate;
    fmt.wBitsPerSample = 16;
    fmt.nBlockAlign = 2;
    fmt.nAvgBytesPerSec = 2 * rate;

    HWAVEOUT dev;
    if (waveOutOpen(&dev, WAVE_MAPPER, &fmt, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) {
        free(pcm);
        return -1;
    }
    WAVEHDR hdr = {0};
    hdr.lpData = (LPSTR)pcm;
    hdr.dwBufferLength = (DWORD)(2 * n);
    int ok = waveOutPrepareHeader(dev, &hdr, sizeof hdr) == MMSYSERR_NOERROR &&
             waveOutWrite(dev, &hdr, sizeof hdr) == MMSYSERR_NOERROR;
    while (ok && !(hdr.dwFlags & WHDR_DONE))
        Sleep(10);
    waveOutUnprepareHeader(dev, &hdr, sizeof hdr);
    waveOutClose(dev);
    free(pcm);
    return ok ? 0 : -1;
}

#else
#include <unistd.h>

int audio_play(const double *x, size_t n, int rate)
{
    char path[] = "/tmp/mitalkXXXXXX";
    int fd = mkstemp(path);
    if (fd < 0)
        return -1;
    close(fd);
    if (wav_write(path, x, n, rate) != 0)
        return -1;
    char cmd[256];
    snprintf(cmd, sizeof cmd,
             "aplay -q '%s' 2>/dev/null || paplay '%s' 2>/dev/null || afplay '%s' 2>/dev/null",
             path, path, path);
    int rc = system(cmd);
    remove(path);
    return rc == 0 ? 0 : -1;
}
#endif
