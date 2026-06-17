#include "audio.h"
#include "synth.h"

/* COBJMACROS lets us call COM interfaces from C (IAudioClient_Initialize(...)).
 * INITGUID makes the DEFINE_GUID entries in the COM headers below allocate the
 * actual CLSID_/IID_ symbols in this translation unit, so we only need to link
 * -lole32 (no -luuid/-lksuser). */
#define COBJMACROS
#define INITGUID
#include <windows.h>
#include <initguid.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <mmreg.h>
#include <mmsystem.h>
#include <stdlib.h>
#include <string.h>

/* Older MinGW headers may not define these. */
#ifndef WAVE_FORMAT_IEEE_FLOAT
#define WAVE_FORMAT_IEEE_FLOAT 0x0003
#endif
#ifndef AUDCLNT_STREAMFLAGS_EVENTCALLBACK
#define AUDCLNT_STREAMFLAGS_EVENTCALLBACK 0x00040000
#endif
#ifndef AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM
#define AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM 0x80000000
#endif
#ifndef AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY
#define AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY 0x08000000
#endif

#define FADE_MS         10
#define FADE_STEP       (1.0 / (FADE_MS * 0.001 * SYNTH_SAMPLE_RATE))

/* WASAPI requests a 20 ms shared buffer; the engine rounds to its own period. */
#define WASAPI_BUFFER_HNS   200000          /* 20 ms in 100-ns units */

/* waveOut fallback keeps the original fixed-buffer layout. */
#define WAVEOUT_FRAMES  882                 /* ~20 ms at 44.1 kHz */
#define WAVEOUT_COUNT   4

typedef enum { BACKEND_NONE, BACKEND_WASAPI, BACKEND_WAVEOUT } Backend;

struct AudioState {
    Backend          backend;

    CRITICAL_SECTION cs;
    AudioParams      params;          /* guarded by cs */
    double           target_gain;     /* guarded by cs */
    int              playing;         /* guarded by cs (user intent) */
    volatile int     shutting_down;   /* main → render thread */

    /* Owned exclusively by whichever thread renders (WASAPI loop or the
     * waveOut callback); never touched concurrently. */
    SynthPhase       phase;
    NoiseState       noise;
    double           current_gain;

    /* WASAPI backend */
    HANDLE           render_thread;
    HANDLE           buffer_event;    /* mixer signals "feed me" */
    HANDLE           ready_event;     /* render thread → audio_init: init done */
    int              wasapi_ok;       /* set by render thread before ready_event */

    /* waveOut fallback */
    HWAVEOUT         hwo;
    WAVEHDR          hdr[WAVEOUT_COUNT];
    int16_t*         buf[WAVEOUT_COUNT];
};

/* Render `frames` stereo float samples from a locked snapshot of the live
 * params. Shared by both backends so the synthesis path is identical. */
static void render_float(AudioState* st, float* out, UINT32 frames) {
    AudioParams snap;
    double      target;
    EnterCriticalSection(&st->cs);
    snap   = st->params;
    target = st->target_gain;
    LeaveCriticalSection(&st->cs);

    if (snap.noise_type > 0) {
        st->current_gain = synth_fill_noise(out, frames, &st->noise,
                               snap.noise_type, snap.volume,
                               st->current_gain, target, FADE_STEP);
    } else {
        SynthFrameParams fp = {
            .left_hz     = snap.left_hz,
            .right_hz    = snap.right_hz,
            .volume      = snap.volume,
            .gain_start  = st->current_gain,
            .gain_target = target,
            .gain_step   = FADE_STEP,
            .mod_hz      = snap.mod_hz,
        };
        st->current_gain = synth_fill_buffer(out, frames, &st->phase, fp);
    }
}

/* ---------------------------------------------------------------------------
 * WASAPI shared-mode, event-driven, 32-bit float backend.
 * Runs on its own thread; owns all COM objects and cleans them up itself.
 * ------------------------------------------------------------------------- */
static DWORD WINAPI wasapi_thread(LPVOID arg) {
    AudioState* st = (AudioState*)arg;

    IMMDeviceEnumerator* enumr  = NULL;
    IMMDevice*           dev    = NULL;
    IAudioClient*        client = NULL;
    IAudioRenderClient*  render = NULL;
    UINT32               buf_frames = 0;
    int                  com_inited = 0;
    HRESULT              hr;

    hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (hr != S_OK && hr != S_FALSE) goto fail;
    com_inited = 1;

    hr = CoCreateInstance(&CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL,
                          &IID_IMMDeviceEnumerator, (void**)&enumr);
    if (FAILED(hr)) goto fail;

    hr = IMMDeviceEnumerator_GetDefaultAudioEndpoint(enumr, eRender, eConsole, &dev);
    if (FAILED(hr)) goto fail;

    hr = IMMDevice_Activate(dev, &IID_IAudioClient, CLSCTX_ALL, NULL,
                            (void**)&client);
    if (FAILED(hr)) goto fail;

    /* Ask for exactly what the synth produces: 44.1 kHz stereo float32.
     * AUTOCONVERTPCM + SRC lets the shared engine resample to its own mix
     * rate (often 48 kHz) so SYNTH_SAMPLE_RATE stays the source of truth. */
    WAVEFORMATEX fmt = {
        .wFormatTag      = WAVE_FORMAT_IEEE_FLOAT,
        .nChannels       = 2,
        .nSamplesPerSec  = SYNTH_SAMPLE_RATE,
        .wBitsPerSample  = 32,
        .nBlockAlign     = 2 * 32 / 8,                  /* 8 bytes/frame */
        .nAvgBytesPerSec = SYNTH_SAMPLE_RATE * 8,
        .cbSize          = 0,
    };

    DWORD flags = AUDCLNT_STREAMFLAGS_EVENTCALLBACK
                | AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM
                | AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY;
    hr = IAudioClient_Initialize(client, AUDCLNT_SHAREMODE_SHARED, flags,
                                 WASAPI_BUFFER_HNS, 0, &fmt, NULL);
    if (FAILED(hr)) goto fail;

    hr = IAudioClient_SetEventHandle(client, st->buffer_event);
    if (FAILED(hr)) goto fail;

    hr = IAudioClient_GetBufferSize(client, &buf_frames);
    if (FAILED(hr)) goto fail;

    hr = IAudioClient_GetService(client, &IID_IAudioRenderClient,
                                 (void**)&render);
    if (FAILED(hr)) goto fail;

    /* Pre-roll one buffer of silence so the stream never starts starved. */
    {
        BYTE* data = NULL;
        hr = IAudioRenderClient_GetBuffer(render, buf_frames, &data);
        if (FAILED(hr)) goto fail;
        IAudioRenderClient_ReleaseBuffer(render, buf_frames,
                                         AUDCLNT_BUFFERFLAGS_SILENT);
    }

    hr = IAudioClient_Start(client);
    if (FAILED(hr)) goto fail;

    /* Init fully succeeded — tell audio_init we're live before we block. */
    st->wasapi_ok = 1;
    SetEvent(st->ready_event);

    while (!st->shutting_down) {
        if (WaitForSingleObject(st->buffer_event, 200) != WAIT_OBJECT_0)
            continue;                       /* timeout: re-check shutdown */
        if (st->shutting_down) break;

        UINT32 padding = 0;
        if (FAILED(IAudioClient_GetCurrentPadding(client, &padding)))
            break;                          /* device lost: drop to silence */
        UINT32 avail = buf_frames - padding;
        if (avail == 0) continue;

        BYTE* data = NULL;
        if (FAILED(IAudioRenderClient_GetBuffer(render, avail, &data)))
            break;
        render_float(st, (float*)data, avail);
        IAudioRenderClient_ReleaseBuffer(render, avail, 0);
    }

    IAudioClient_Stop(client);
    goto cleanup;

fail:
    /* Signal failure so audio_init can fall back to waveOut. */
    st->wasapi_ok = 0;
    SetEvent(st->ready_event);

cleanup:
    if (render) IAudioRenderClient_Release(render);
    if (client) IAudioClient_Release(client);
    if (dev)    IMMDevice_Release(dev);
    if (enumr)  IMMDeviceEnumerator_Release(enumr);
    if (com_inited) CoUninitialize();
    return 0;
}

/* ---------------------------------------------------------------------------
 * waveOut fallback (legacy MME). 16-bit PCM via the OS resampler — only used
 * when WASAPI initialization fails, so reliability never regresses.
 * ------------------------------------------------------------------------- */
static void f32_to_i16(const float* in, int16_t* out, size_t n) {
    for (size_t i = 0; i < n; i++) {
        double s = in[i];
        if (s >  1.0) s =  1.0;
        if (s < -1.0) s = -1.0;
        out[i] = (int16_t)(s * 32767.0);
    }
}

static void CALLBACK waveout_callback(HWAVEOUT hwo, UINT msg, DWORD_PTR inst,
                                      DWORD_PTR p1, DWORD_PTR p2) {
    (void)p2;
    if (msg != WOM_DONE) return;
    AudioState* st = (AudioState*)inst;
    if (st->shutting_down) return;
    WAVEHDR* hdr = (WAVEHDR*)p1;

    float tmp[WAVEOUT_FRAMES * 2];
    render_float(st, tmp, WAVEOUT_FRAMES);
    f32_to_i16(tmp, (int16_t*)hdr->lpData, WAVEOUT_FRAMES * 2);

    waveOutWrite(hwo, hdr, sizeof *hdr);
}

static int waveout_init(AudioState* st) {
    WAVEFORMATEX fmt = {
        .wFormatTag      = WAVE_FORMAT_PCM,
        .nChannels       = 2,
        .nSamplesPerSec  = SYNTH_SAMPLE_RATE,
        .wBitsPerSample  = 16,
        .nBlockAlign     = 4,
        .nAvgBytesPerSec = SYNTH_SAMPLE_RATE * 4,
        .cbSize          = 0,
    };
    if (waveOutOpen(&st->hwo, WAVE_MAPPER, &fmt,
                    (DWORD_PTR)waveout_callback, (DWORD_PTR)st,
                    CALLBACK_FUNCTION) != MMSYSERR_NOERROR)
        return 0;

    for (int i = 0; i < WAVEOUT_COUNT; i++) {
        st->buf[i] = (int16_t*)calloc(WAVEOUT_FRAMES * 2, sizeof(int16_t));
        st->hdr[i].lpData         = (LPSTR)st->buf[i];
        st->hdr[i].dwBufferLength = WAVEOUT_FRAMES * 2 * sizeof(int16_t);
        waveOutPrepareHeader(st->hwo, &st->hdr[i], sizeof st->hdr[i]);
        waveOutWrite(st->hwo, &st->hdr[i], sizeof st->hdr[i]);
    }
    return 1;
}

/* ---------------------------------------------------------------------------
 * Public API (audio.h) — backend-agnostic.
 * ------------------------------------------------------------------------- */
AudioState* audio_init(void) {
    AudioState* st = (AudioState*)calloc(1, sizeof *st);
    if (!st) return NULL;
    InitializeCriticalSection(&st->cs);
    st->params.volume = 0.5;
    noise_state_init(&st->noise);

    /* Preferred path: WASAPI shared + float32. */
    st->buffer_event = CreateEventA(NULL, FALSE, FALSE, NULL);
    st->ready_event  = CreateEventA(NULL, FALSE, FALSE, NULL);
    if (st->buffer_event && st->ready_event) {
        st->render_thread = CreateThread(NULL, 0, wasapi_thread, st, 0, NULL);
        if (st->render_thread) {
            WaitForSingleObject(st->ready_event, INFINITE);
            if (st->wasapi_ok) {
                st->backend = BACKEND_WASAPI;
                return st;
            }
            /* WASAPI failed; the thread is unwinding on its own. Reap it. */
            WaitForSingleObject(st->render_thread, INFINITE);
            CloseHandle(st->render_thread);
            st->render_thread = NULL;
        }
    }

    /* Fallback: legacy waveOut. */
    if (waveout_init(st)) {
        st->backend = BACKEND_WAVEOUT;
        return st;
    }

    MessageBoxA(NULL, "Both WASAPI and waveOut failed to initialize.",
                "Audio init failed", MB_ICONERROR);
    if (st->buffer_event) CloseHandle(st->buffer_event);
    if (st->ready_event)  CloseHandle(st->ready_event);
    DeleteCriticalSection(&st->cs);
    free(st);
    return NULL;
}

void audio_shutdown(AudioState* st) {
    if (!st) return;
    st->shutting_down = 1;

    if (st->backend == BACKEND_WASAPI) {
        SetEvent(st->buffer_event);             /* wake the render loop now */
        if (st->render_thread) {
            WaitForSingleObject(st->render_thread, INFINITE);
            CloseHandle(st->render_thread);
        }
    } else if (st->backend == BACKEND_WAVEOUT) {
        waveOutReset(st->hwo);
        for (int i = 0; i < WAVEOUT_COUNT; i++) {
            waveOutUnprepareHeader(st->hwo, &st->hdr[i], sizeof st->hdr[i]);
            free(st->buf[i]);
        }
        waveOutClose(st->hwo);
    }

    if (st->buffer_event) CloseHandle(st->buffer_event);
    if (st->ready_event)  CloseHandle(st->ready_event);
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
     * The render path finishes the ~10ms fade-out and then idles on silent
     * buffers; the device/stream stays open for instant restart. */
    st->playing = 0;
    LeaveCriticalSection(&st->cs);
}

int audio_is_playing(const AudioState* st) {
    if (!st) return 0;
    return st->playing;
}
