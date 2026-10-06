/*
 * SDL2 smoke test for the PS5 toolchain.
 *
 * Fills the screen with a colour that changes when a controller button is
 * pressed, plays a short tone on start, logs controller events to stdout and
 * exits when Options is pressed (or after 60 seconds).
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include <SDL2/SDL.h>

#define TONE_HZ 440.0
#define SAMPLE_RATE 48000

static void play_tone(void)
{
    SDL_AudioSpec want = { 0 }, have;
    want.freq = SAMPLE_RATE;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = 1024;

    SDL_AudioDeviceID dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (dev == 0) {
        printf("SDL_OpenAudioDevice: %s\n", SDL_GetError());
        return;
    }

    static Sint16 buf[SAMPLE_RATE / 2 * 2];
    for (int i = 0; i < SAMPLE_RATE / 2; i++) {
        Sint16 s = (Sint16)(sin(2.0 * M_PI * TONE_HZ * i / SAMPLE_RATE) * 8000);
        buf[i * 2] = s;
        buf[i * 2 + 1] = s;
    }
    SDL_QueueAudio(dev, buf, sizeof(buf));
    SDL_PauseAudioDevice(dev, 0);
    printf("audio: %d Hz, %d channels\n", have.freq, have.channels);
}

int sceKernelAvailableFlexibleMemorySize(size_t *);
int sceKernelConfiguredFlexibleMemorySize(size_t *);

/* Log the flexible memory budget. */
static void probe_memory(void)
{
    size_t avail = 0, configured = 0;
    sceKernelAvailableFlexibleMemorySize(&avail);
    sceKernelConfiguredFlexibleMemorySize(&configured);
    printf("sdl-smoke: flexible memory %zu MiB available, %zu MiB configured\n",
           avail >> 20, configured >> 20);
}

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    printf("sdl-smoke: start\n");
    probe_memory();
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) != 0) {
        printf("SDL_Init: %s\n", SDL_GetError());
        return 1;
    }

    SDL_DisplayMode mode;
    SDL_GetCurrentDisplayMode(0, &mode);
    printf("display: %dx%d @ %d Hz\n", mode.w, mode.h, mode.refresh_rate);

    SDL_Surface *test = SDL_CreateRGBSurfaceWithFormat(0, mode.w, mode.h, 32,
                                                       SDL_PIXELFORMAT_ABGR8888);
    printf("sdl-smoke: direct surface %s (%s)\n", test ? "ok" : "failed",
           test ? "-" : SDL_GetError());
    SDL_FreeSurface(test);
    SDL_SetHint(SDL_HINT_FRAMEBUFFER_ACCELERATION, "0");

    SDL_Window *win = SDL_CreateWindow("sdl-smoke", 0, 0, mode.w, mode.h, 0);
    if (!win) {
        printf("SDL_CreateWindow: %s\n", SDL_GetError());
        return 1;
    }
    int ww = 0, wh = 0;
    SDL_GetWindowSizeInPixels(win, &ww, &wh);
    printf("sdl-smoke: window %dx%d\n", ww, wh);
    SDL_ClearError();
    SDL_Surface *screen = SDL_GetWindowSurface(win);
    if (!screen) {
        printf("SDL_GetWindowSurface: %s\n", SDL_GetError());
        return 1;
    }

    for (int i = 0; i < SDL_NumJoysticks(); i++) {
        if (SDL_IsGameController(i) && SDL_GameControllerOpen(i)) {
            printf("controller %d: %s\n", i, SDL_GameControllerNameForIndex(i));
        }
    }

    play_tone();

    static const Uint8 colours[][3] = {
        { 32, 96, 200 }, { 200, 48, 48 }, { 48, 160, 64 }, { 220, 180, 32 },
    };
    int colour = 0;
    Uint32 start = SDL_GetTicks();
    int running = 1;

    while (running && SDL_GetTicks() - start < 60000) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            switch (ev.type) {
            case SDL_QUIT:
                running = 0;
                break;
            case SDL_CONTROLLERDEVICEADDED:
                SDL_GameControllerOpen(ev.cdevice.which);
                printf("controller added: %d\n", ev.cdevice.which);
                break;
            case SDL_CONTROLLERBUTTONDOWN:
                printf("button: %s\n",
                       SDL_GameControllerGetStringForButton(ev.cbutton.button));
                if (ev.cbutton.button == SDL_CONTROLLER_BUTTON_START) {
                    running = 0;
                }
                colour = (colour + 1) % (int)SDL_arraysize(colours);
                break;
            case SDL_CONTROLLERAXISMOTION:
                if (ev.caxis.value > 16000 || ev.caxis.value < -16000) {
                    printf("axis %s: %d\n",
                           SDL_GameControllerGetStringForAxis(ev.caxis.axis),
                           ev.caxis.value);
                }
                break;
            }
        }

        const Uint8 *c = colours[colour];
        SDL_FillRect(screen, NULL, SDL_MapRGB(screen->format, c[0], c[1], c[2]));
        SDL_Rect box = { (int)((SDL_GetTicks() / 4) % (Uint32)(mode.w - 100)),
                         mode.h / 2 - 50, 100, 100 };
        SDL_FillRect(screen, &box, SDL_MapRGB(screen->format, 255, 255, 255));
        SDL_UpdateWindowSurface(win);
    }

    printf("exiting\n");
    SDL_Quit();
    return 0;
}
