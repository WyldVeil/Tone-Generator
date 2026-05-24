#ifndef AUDIO_H
#define AUDIO_H

typedef struct AudioState AudioState;

typedef struct {
    double left_hz;   /* 0 means silent on the left channel */
    double right_hz;  /* equals left_hz in mono mode */
    double volume;    /* 0.0 .. 1.0 */
} AudioParams;

/* Opens waveOut, allocates buffers, starts the callback. Returns NULL on failure. */
AudioState* audio_init(void);

/* Stops device, frees buffers. Safe to call with NULL. */
void audio_shutdown(AudioState* st);

/* Thread-safe. Pushes new live parameters; takes effect on next buffer boundary. */
void audio_set_params(AudioState* st, AudioParams params);

/* Begins fade-in if not already playing. */
void audio_play(AudioState* st);

/* Begins fade-out; when silent, resets the device. */
void audio_stop(AudioState* st);

/* Returns 1 if currently producing sound (including during fade ramps), 0 otherwise. */
int  audio_is_playing(AudioState* st);

#endif
