/*
 * window_sdl.c — the SDL2 window: presentation, sound, keyboard, gamepads,
 * frame pacing. Only built when SDL2 is found; see platform.c for the
 * headless stubs.
 *
 * Keys: arrows, Z/X/C/V = A/B/C/D, 1/2 = start P1/P2, 3 = select,
 * 5/6 = coin 1/2, 9 = service, F2 = test switch, Esc = quit.
 *
 * Gamepads (SDL's GameController layout, so any pad SDL knows): the first
 * connected is player 1, the second player 2, plugged in at any time.
 * D-pad or left stick; south/east/west/north (Xbox A/B/X/Y) = Neo Geo
 * A/B/C/D; Start = start; Back = coin; left shoulder = select. Keyboard
 * and pads work together. A pad SDL doesn't know can be described with the
 * SDL_GAMECONTROLLERCONFIG environment variable.
 */
#include "ng_internal.h"
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ---- anti-alias filter for devices slower than the YM2610 ----
 *
 * The chip produces 55,555 Hz.  SDL's stream converter is a linear
 * resampler, so on a 48 kHz (Steam Deck / PipeWire) or 44.1 kHz device
 * everything above the new Nyquist folds back into the audible band.  A
 * native-rate capture is clean, but the same music sounds fizzy through
 * the window.  Apply a 64-tap Blackman-windowed sinc low-pass at
 * 0.45 * device rate before handing the samples to SDL; the converter then
 * only has to interpolate a band-limited signal.  Devices that accept
 * 55,555 Hz bypass it entirely. */
#define AA_TAPS 64
static float s_aa_coeff[AA_TAPS];
static float s_aa_hist[2][AA_TAPS];
static int s_aa_index;
static int s_aa_on;
static int16_t s_aa_buf[4096 * 2];

static double aa_sinc(double x) {
    return x == 0.0 ? 1.0 : sin(M_PI * x) / (M_PI * x);
}

static void aa_init(int src_rate, int dst_rate) {
    s_aa_on = dst_rate > 0 && dst_rate < src_rate;
    if (!s_aa_on) return;
    double fc = 0.45 * (double)dst_rate / (double)src_rate;  /* cycles/sample */
    double sum = 0.0;
    for (int i = 0; i < AA_TAPS; i++) {
        double n = i - (AA_TAPS - 1) / 2.0;
        double w = 0.42 - 0.5 * cos(2.0 * M_PI * i / (AA_TAPS - 1)) +
                   0.08 * cos(4.0 * M_PI * i / (AA_TAPS - 1));
        double h = 2.0 * fc * aa_sinc(2.0 * fc * n) * w;
        s_aa_coeff[i] = (float)h;
        sum += h;
    }
    if (sum != 0.0)
        for (int i = 0; i < AA_TAPS; i++) s_aa_coeff[i] = (float)(s_aa_coeff[i] / sum);
    memset(s_aa_hist, 0, sizeof(s_aa_hist));
    s_aa_index = 0;
}

static void aa_process(const int16_t *in, int16_t *out, size_t frames) {
    for (size_t f = 0; f < frames; f++) {
        int idx = s_aa_index;
        s_aa_hist[0][idx] = (float)in[f * 2];
        s_aa_hist[1][idx] = (float)in[f * 2 + 1];
        for (int ch = 0; ch < 2; ch++) {
            double acc = 0.0;
            int p = idx;
            for (int k = 0; k < AA_TAPS; k++) {
                acc += s_aa_coeff[k] * s_aa_hist[ch][p];
                p = (p + AA_TAPS - 1) % AA_TAPS;
            }
            out[f * 2 + ch] = (int16_t)(acc < -32768.0 ? -32768 :
                                        acc > 32767.0 ? 32767 : (int)(acc >= 0 ? acc + 0.5 : acc - 0.5));
        }
        s_aa_index = (idx + 1) % AA_TAPS;
    }
}

static SDL_Window *s_win;
static SDL_Renderer *s_ren;
static SDL_Texture *s_tex;
static int s_fullscreen;
static uint64_t s_next_tick;
static SDL_AudioDeviceID s_audio;
static SDL_AudioStream *s_audio_stream;   /* YM2610 rate -> device rate */
static SDL_AudioSpec s_audio_spec;
static int s_audio_started;
static SDL_GameController *s_pad[2];     /* player 1, player 2 */
static int s_audio_trace;                /* NG_AUDIO_TRACE=1 diagnostic */
static long long s_audio_frames;         /* source frames handed to the stream */
static long long s_audio_queued_max;
static int s_audio_stalls;               /* times the queue had to be trimmed */
static int s_audio_underruns;            /* times a dry queue was re-cushioned */

/* ponytail: a fixed layout; remapping belongs to the frontend (ROADMAP.md) */
static void pad_added(int index) {
    for (int i = 0; i < 2; i++)
        if (!s_pad[i]) {
            s_pad[i] = SDL_GameControllerOpen(index);
            if (s_pad[i]) fprintf(stderr, "gamepad: %s is player %d\n", SDL_GameControllerName(s_pad[i]), i + 1);
            return;
        }
}

static void pad_removed(SDL_JoystickID id) {
    for (int i = 0; i < 2; i++)
        if (s_pad[i] && SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(s_pad[i])) == id) {
            SDL_GameControllerClose(s_pad[i]);
            s_pad[i] = NULL;
        }
}

int window_open(const char *title, int scale, int fullscreen) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "error: SDL_Init: %s\n", SDL_GetError());
        return -1;
    }
    Uint32 flags = SDL_WINDOW_RESIZABLE;
    int w = NG_SCREEN_W * scale, h = NG_SCREEN_H * scale;
    if (fullscreen) { flags |= SDL_WINDOW_FULLSCREEN_DESKTOP; w = 0; h = 0; }
    s_fullscreen = fullscreen;
    s_win = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                             w, h, flags);
    if (!s_win) {
        fprintf(stderr, "error: SDL_CreateWindow: %s\n", SDL_GetError());
        return -1;
    }
    s_ren = SDL_CreateRenderer(s_win, -1, SDL_RENDERER_ACCELERATED);
    if (!s_ren) {
        /* Headless X servers, some remote sessions and a few old drivers have
         * no accelerated renderer; fall back to SDL's software one rather
         * than presenting nothing. */
        fprintf(stderr, "warning: no accelerated renderer (%s), using software\n", SDL_GetError());
        s_ren = SDL_CreateRenderer(s_win, -1, SDL_RENDERER_SOFTWARE);
    }
    s_tex = s_ren ? SDL_CreateTexture(s_ren, SDL_PIXELFORMAT_ARGB8888,
                                      SDL_TEXTUREACCESS_STREAMING,
                                      NG_SCREEN_W, NG_SCREEN_H) : NULL;
    if (!s_ren || !s_tex) {
        fprintf(stderr, "error: SDL renderer: %s\n", SDL_GetError());
        return -1;
    }
    s_next_tick = SDL_GetPerformanceCounter();
    /* The YM2610's 55,555 Hz is not a rate sound cards commonly accept.
     * Open the device at whatever it supports and convert with SDL's
     * audio stream, instead of letting SDL_OpenAudioDevice fail (which
     * left the game silent on some Windows and SteamOS setups). */
    SDL_AudioSpec want = {0}, have;
    want.freq = NG_AUDIO_RATE; want.format = AUDIO_S16SYS; want.channels = 2; want.samples = 1024;
    /* Diagnostic hook: reproduce a 48 kHz Steam Deck/PipeWire device on a
     * desktop that accepts 55,555 Hz.  Not a game setting. */
    const char *force_rate = getenv("NG_AUDIO_DEVICE_RATE");
    if (force_rate && atoi(force_rate) > 0) want.freq = atoi(force_rate);
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) == 0)
        s_audio = SDL_OpenAudioDevice(NULL, 0, &want, &have,
                                      SDL_AUDIO_ALLOW_FREQUENCY_CHANGE |
                                      SDL_AUDIO_ALLOW_FORMAT_CHANGE |
                                      SDL_AUDIO_ALLOW_CHANNELS_CHANGE);
    if (s_audio) {
        s_audio_spec = have;
        aa_init(NG_AUDIO_RATE, have.freq);
        s_audio_stream = SDL_NewAudioStream(AUDIO_S16SYS, 2, NG_AUDIO_RATE,
                                            have.format, have.channels, have.freq);
        if (!s_audio_stream) {
            fprintf(stderr, "warning: no sound: %s\n", SDL_GetError());
            SDL_CloseAudioDevice(s_audio);
            s_audio = 0;
        }
    } else {
        fprintf(stderr, "warning: no sound: %s\n", SDL_GetError());
    }
    s_audio_trace = getenv("NG_AUDIO_TRACE") != NULL;
    if (s_audio_trace && s_audio) {
        fprintf(stderr, "[audio-sdl] device %d Hz, %d ch, %d-bit, %u-sample chunks; "
                        "source %d Hz stereo%s\n",
                have.freq, have.channels, SDL_AUDIO_BITSIZE(have.format),
                (unsigned)have.samples, NG_AUDIO_RATE,
                s_aa_on ? ", 64-tap anti-alias filter on" : ", no resampling");
        fflush(stderr);
    }
    /* Pads already connected arrive as SDL_CONTROLLERDEVICEADDED events. */
    if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) != 0)
        fprintf(stderr, "warning: no gamepads: %s\n", SDL_GetError());
    return 0;
}

/* Convert the YM2610's samples to the device's rate and queue them.  The
 * old path queued at 55,555 Hz and inserted silence whenever the queue
 * dipped below 30 ms, then threw a whole 17 ms frame away above 120 ms:
 * scheduling jitter made that happen regularly, which is audible as clicks
 * and dropouts.  Here the stream resamples, converted data is moved in whole
 * device frames (so stereo cannot slip by half a sample), the device starts
 * with a 60 ms cushion, and a genuine >250 ms stall trims only its oldest
 * excess instead of clearing everything. */
void window_audio(const int16_t *samples, size_t frames) {
    if (!s_audio || !s_audio_stream || frames == 0) return;
    if (s_aa_on) {
        /* Band-limit before SDL's linear rate converter. */
        size_t done = 0;
        while (done < frames) {
            size_t n = frames - done;
            if (n > 4096) n = 4096;
            aa_process(samples + done * 2, s_aa_buf, n);
            if (SDL_AudioStreamPut(s_audio_stream, s_aa_buf, (int)(n * 4)) < 0) return;
            done += n;
        }
    } else if (SDL_AudioStreamPut(s_audio_stream, samples, (int)(frames * 4)) < 0) {
        return;
    }
    s_audio_frames += (long long)frames;

    Uint32 bytes_per_frame = (Uint32)SDL_AUDIO_BITSIZE(s_audio_spec.format) / 8u *
                             (Uint32)s_audio_spec.channels;
    if (bytes_per_frame == 0 || s_audio_spec.freq <= 0) return;
    Uint32 high = (Uint32)((double)s_audio_spec.freq * bytes_per_frame * 0.250);
    Uint32 low  = (Uint32)((double)s_audio_spec.freq * bytes_per_frame * 0.060);
    Uint32 dry  = (Uint32)((double)s_audio_spec.freq * bytes_per_frame * 0.008);

    /* Drain converted audio in whole destination frames.  A partial frame
     * left in the stream is harmless; queuing one to the device would swap
     * left/right for everything that follows. */
    Uint8 buf[16384];
    for (;;) {
        int avail = SDL_AudioStreamAvailable(s_audio_stream);
        if (avail < (int)bytes_per_frame) break;
        int want = avail < (int)sizeof(buf) ? avail : (int)sizeof(buf);
        want -= want % (int)bytes_per_frame;
        if (want <= 0) break;
        int got = SDL_AudioStreamGet(s_audio_stream, buf, want);
        if (got <= 0) break;
        if (SDL_QueueAudio(s_audio, buf, (Uint32)got) < 0) break;
    }

    Uint32 queued = SDL_GetQueuedAudioSize(s_audio);
    if (queued > high) {
        /* A genuine stall (window drag, load hitch).  Drop the oldest
         * excess only: SDL_ClearQueuedAudio() would throw away good audio
         * and cut the music. */
        Uint32 drop = queued - low;
        Uint8 scratch[4096];
        while (drop > 0) {
            Uint32 n = drop < (Uint32)sizeof(scratch) ? drop : (Uint32)sizeof(scratch);
            Uint32 got = SDL_DequeueAudio(s_audio, scratch, n);
            if (got == 0) break;   /* SDL returns the byte count, 0 if empty */
            drop -= got;
        }
        s_audio_stalls++;
        queued = SDL_GetQueuedAudioSize(s_audio);
    }
    if (s_audio_started && queued < dry) {
        /* The device ran dry between two frames (scheduler jitter, a busy
         * compositor).  Rebuild the same cushion the stream starts with,
         * once, rather than letting SDL underrun on every following frame. */
        Uint32 pad = low - queued;
        static const Uint8 zeros[4096];
        while (pad > 0) {
            Uint32 n = pad < (Uint32)sizeof(zeros) ? pad : (Uint32)sizeof(zeros);
            if (SDL_QueueAudio(s_audio, zeros, n) < 0) break;
            pad -= n;
        }
        s_audio_underruns++;
        queued = SDL_GetQueuedAudioSize(s_audio);
    }
    if (!s_audio_started && queued >= low) {
        SDL_PauseAudioDevice(s_audio, 0);
        s_audio_started = 1;
    }
    if (s_audio_trace) {
        Uint32 q = SDL_GetQueuedAudioSize(s_audio);
        if (q > s_audio_queued_max) s_audio_queued_max = q;
        if ((s_audio_frames % 55555) < (long long)frames)
            fprintf(stderr, "[audio-sdl] fed %lld frames, queued %.1f ms, "
                            "max %.1f ms, stalls %d, underruns %d\n",
                    s_audio_frames, 1000.0 * q / ((double)s_audio_spec.freq * bytes_per_frame),
                    1000.0 * s_audio_queued_max / ((double)s_audio_spec.freq * bytes_per_frame),
                    s_audio_stalls, s_audio_underruns);
    }
}

static void read_keys(ng_input_t *in) {
    const Uint8 *k = SDL_GetKeyboardState(NULL);
    if (k[SDL_SCANCODE_UP]) in->p1 |= NG_UP;
    if (k[SDL_SCANCODE_DOWN]) in->p1 |= NG_DOWN;
    if (k[SDL_SCANCODE_LEFT]) in->p1 |= NG_LEFT;
    if (k[SDL_SCANCODE_RIGHT]) in->p1 |= NG_RIGHT;
    if (k[SDL_SCANCODE_Z]) in->p1 |= NG_A;
    if (k[SDL_SCANCODE_X]) in->p1 |= NG_B;
    if (k[SDL_SCANCODE_C]) in->p1 |= NG_C;
    if (k[SDL_SCANCODE_V]) in->p1 |= NG_D;
    in->start1 = k[SDL_SCANCODE_1];
    in->start2 = k[SDL_SCANCODE_2];
    in->select1 = k[SDL_SCANCODE_3];
    in->coin1 = k[SDL_SCANCODE_5];
    in->coin2 = k[SDL_SCANCODE_6];
    in->service = k[SDL_SCANCODE_9];
    in->test = k[SDL_SCANCODE_F2];
}

static void read_pad(SDL_GameController *c, uint8_t *bits, uint8_t *start, uint8_t *coin, uint8_t *select) {
    enum { DEAD = 12000 };   /* about a third of the stick's travel */
    int x = SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_LEFTX);
    int y = SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_LEFTY);
#define PAD(b) SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_##b)
    if (PAD(DPAD_UP) || y < -DEAD) *bits |= NG_UP;
    if (PAD(DPAD_DOWN) || y > DEAD) *bits |= NG_DOWN;
    if (PAD(DPAD_LEFT) || x < -DEAD) *bits |= NG_LEFT;
    if (PAD(DPAD_RIGHT) || x > DEAD) *bits |= NG_RIGHT;
    if (PAD(A)) *bits |= NG_A;
    if (PAD(B)) *bits |= NG_B;
    if (PAD(X)) *bits |= NG_C;
    if (PAD(Y)) *bits |= NG_D;
    *start |= PAD(START);
    *coin |= PAD(BACK);
    *select |= PAD(LEFTSHOULDER);
#undef PAD
}

bool window_frame(const uint32_t *fb, ng_input_t *in) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) return false;
        if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) return false;
        if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_F11) {
            s_fullscreen = !s_fullscreen;
            SDL_SetWindowFullscreen(s_win, s_fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
        }
        if (e.type == SDL_CONTROLLERDEVICEADDED) pad_added(e.cdevice.which);
        if (e.type == SDL_CONTROLLERDEVICEREMOVED) pad_removed(e.cdevice.which);
    }
    read_keys(in);
    if (s_pad[0]) read_pad(s_pad[0], &in->p1, &in->start1, &in->coin1, &in->select1);
    if (s_pad[1]) read_pad(s_pad[1], &in->p2, &in->start2, &in->coin2, &in->select2);
    if (fb) {
        SDL_UpdateTexture(s_tex, NULL, fb, NG_SCREEN_W * 4);
        SDL_RenderClear(s_ren);
        int ow = 0, oh = 0;
        SDL_GetRendererOutputSize(s_ren, &ow, &oh);
        SDL_Rect dst = {0, 0, ow, oh};
        if (s_fullscreen && ow > 0 && oh > 0) {
            /* The Neo Geo's 320x224 is a 4:3 display on a CRT. Fit that into
             * the 16:10 panel and pillarbox; never stretch to the panel. */
            int vw = oh * 4 / 3, vh = oh;
            if (vw > ow) { vw = ow; vh = ow * 3 / 4; }
            dst.x = (ow - vw) / 2; dst.y = (oh - vh) / 2;
            dst.w = vw; dst.h = vh;
        }
        /* Presentation diagnostic: one line with the renderer's real output
         * size and the destination rectangle. Used to check the Deck's
         * 1280x800 pillarboxing without a physical screen. */
        if (getenv("NG_WINDOW_TRACE")) {
            static int traced;
            if (!traced++) fprintf(stderr, "[window] output %dx%d dst %d,%d %dx%d fullscreen=%d\n",
                                   ow, oh, dst.x, dst.y, dst.w, dst.h, s_fullscreen);
        }
        SDL_RenderCopy(s_ren, s_tex, NULL, &dst);
        SDL_RenderPresent(s_ren);
    }
    /* Pace to the MVS refresh rate, 59.1856 Hz. */
    uint64_t freq = SDL_GetPerformanceFrequency();
    s_next_tick += (uint64_t)((double)freq / 59.185606);
    uint64_t now = SDL_GetPerformanceCounter();
    if (now < s_next_tick) SDL_Delay((Uint32)((s_next_tick - now) * 1000 / freq));
    else if (now - s_next_tick > freq / 10) s_next_tick = now;
    return true;
}

void window_close(void) {
    for (int i = 0; i < 2; i++)
        if (s_pad[i]) SDL_GameControllerClose(s_pad[i]);
    if (s_audio_stream) SDL_FreeAudioStream(s_audio_stream);
    if (s_audio) SDL_CloseAudioDevice(s_audio);
    if (s_tex) SDL_DestroyTexture(s_tex);
    if (s_ren) SDL_DestroyRenderer(s_ren);
    if (s_win) SDL_DestroyWindow(s_win);
    SDL_Quit();
}
