#ifndef AUDIO_H
#define AUDIO_H

typedef struct AudioState AudioState;

typedef struct {
    double left_hz;   /* 0 means silent on the left channel */
    double right_hz;  /* equals left_hz in mono mode */
    double volume;    /* 0.0 .. 1.0 */
    double mod_hz;    /* 0 = no modulation; >0 = AM pulse rate (e.g. 40 for GENUS) */
} AudioParams;

/* Opens waveOut, allocates buffers, starts the callback. Returns NULL on failure. */
AudioState* audio_init(void);

/* Stops device, frees buffers. Safe to call with NULL. */
void audio_shutdown(AudioState* st);

/* Thread-safe. Pushes new live parameters; takes effect on next buffer boundary. */
void audio_set_params(AudioState* st, AudioParams params);

/* Thread-safe. Begins fade-in if not already playing. */
void audio_play(AudioState* st);

/* Thread-safe. Begins fade-out. After the ramp, the audio thread keeps the
 * device open (queuing silent buffers) so the next audio_play has no startup
 * latency; only audio_shutdown closes the device. */
void audio_stop(AudioState* st);

/* Lock-free read; safe to poll from the GUI thread.
 * Returns 1 between audio_play() and audio_stop() (user intent), 0 otherwise.
 * The actual fade-out continues briefly after this flips back to 0. */
int  audio_is_playing(const AudioState* st);

#endif
