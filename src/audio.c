#include "audio.h"
#include "synth.h"

#include <windows.h>
#include <mmsystem.h>
#include <stdlib.h>
#include <string.h>

#define BUFFER_FRAMES   882                /* ~20 ms at 44.1 kHz */
#define BUFFER_COUNT    4
#define FADE_MS         10
#define FADE_STEP       (1.0 / (FADE_MS * 0.001 * SYNTH_SAMPLE_RATE))

struct AudioState {
    HWAVEOUT        hwo;
    WAVEHDR         hdr[BUFFER_COUNT];
    int16_t*        buf[BUFFER_COUNT];
    CRITICAL_SECTION cs;
    AudioParams     params;          /* live, guarded by cs */
    SynthPhase      phase;           /* owned by callback thread */
    double          current_gain;    /* owned by callback thread */
    double          target_gain;     /* guarded by cs */
    int             playing;         /* 1 between play() and audio going silent */
    int             shutting_down;   /* signal from main → callback */
};

static void CALLBACK wave_callback(HWAVEOUT hwo, UINT msg,
                                   DWORD_PTR inst, DWORD_PTR p1, DWORD_PTR p2)
{
    (void)hwo; (void)p1; (void)p2;
    if (msg != WOM_DONE) return;
    AudioState* st = (AudioState*)inst;
    WAVEHDR* hdr = (WAVEHDR*)p1;

    if (st->shutting_down) return;

    /* Snapshot live params + target_gain under lock, then release before
     * doing the (relatively slow) sample math so the GUI thread never
     * waits on us. */
    AudioParams snap;
    double      target;
    EnterCriticalSection(&st->cs);
    snap   = st->params;
    target = st->target_gain;
    LeaveCriticalSection(&st->cs);

    SynthFrameParams fp = {
        .left_hz     = snap.left_hz,
        .right_hz    = snap.right_hz,
        .volume      = snap.volume,
        .gain_start  = st->current_gain,
        .gain_target = target,
        .gain_step   = FADE_STEP,
    };

    st->current_gain = synth_fill_buffer((int16_t*)hdr->lpData,
                                         BUFFER_FRAMES, &st->phase, fp);

    waveOutWrite(hwo, hdr, sizeof *hdr);
}

AudioState* audio_init(void) {
    AudioState* st = (AudioState*)calloc(1, sizeof *st);
    if (!st) return NULL;
    InitializeCriticalSection(&st->cs);
    st->params.volume = 0.5;

    WAVEFORMATEX fmt = {
        .wFormatTag      = WAVE_FORMAT_PCM,
        .nChannels       = 2,
        .nSamplesPerSec  = SYNTH_SAMPLE_RATE,
        .wBitsPerSample  = 16,
        .nBlockAlign     = 4,                           /* 2 ch * 16 bit / 8 */
        .nAvgBytesPerSec = SYNTH_SAMPLE_RATE * 4,
        .cbSize          = 0,
    };

    MMRESULT r = waveOutOpen(&st->hwo, WAVE_MAPPER, &fmt,
                             (DWORD_PTR)wave_callback,
                             (DWORD_PTR)st, CALLBACK_FUNCTION);
    if (r != MMSYSERR_NOERROR) {
        char msg[256];
        waveOutGetErrorTextA(r, msg, sizeof msg);
        MessageBoxA(NULL, msg, "waveOutOpen failed", MB_ICONERROR);
        DeleteCriticalSection(&st->cs);
        free(st);
        return NULL;
    }

    /* Allocate buffers, prime them with silence, queue them. */
    for (int i = 0; i < BUFFER_COUNT; i++) {
        st->buf[i] = (int16_t*)calloc(BUFFER_FRAMES * 2, sizeof(int16_t));
        st->hdr[i].lpData         = (LPSTR)st->buf[i];
        st->hdr[i].dwBufferLength = BUFFER_FRAMES * 2 * sizeof(int16_t);
        waveOutPrepareHeader(st->hwo, &st->hdr[i], sizeof st->hdr[i]);
        waveOutWrite(st->hwo, &st->hdr[i], sizeof st->hdr[i]);
    }
    return st;
}

void audio_shutdown(AudioState* st) {
    if (!st) return;
    st->shutting_down = 1;
    waveOutReset(st->hwo);
    for (int i = 0; i < BUFFER_COUNT; i++) {
        waveOutUnprepareHeader(st->hwo, &st->hdr[i], sizeof st->hdr[i]);
        free(st->buf[i]);
    }
    waveOutClose(st->hwo);
    DeleteCriticalSection(&st->cs);
    free(st);
}

void audio_set_params(AudioState* st, AudioParams params) {
    if (!st) return;
    EnterCriticalSection(&st->cs);
    st->params = params;
    LeaveCriticalSection(&st->cs);
}

void audio_play(AudioState* st) {
    if (!st) return;
    EnterCriticalSection(&st->cs);
    st->target_gain = 1.0;
    st->playing     = 1;
    LeaveCriticalSection(&st->cs);
}

void audio_stop(AudioState* st) {
    if (!st) return;
    EnterCriticalSection(&st->cs);
    st->target_gain = 0.0;
    /* Clear playing immediately so the GUI status reflects user intent.
     * The callback finishes the ~10ms fade-out and then idles on silent
     * buffers; the device stays open. */
    st->playing = 0;
    LeaveCriticalSection(&st->cs);
}

int audio_is_playing(const AudioState* st) {
    if (!st) return 0;
    return st->playing;
}
