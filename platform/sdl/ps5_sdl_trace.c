/*
 * Startup trace of the SDL calls that set up an app's display: window,
 * renderer and texture creation, size and fullscreen changes, and the first
 * presented frames. Output goes to the kernel log. Enabled with
 * PS5_SDL_TRACE=1 in scripts/package-native.sh.
 */
#include <stdio.h>

#include <SDL.h>

SDL_Window *__real_SDL_CreateWindow(const char *, int, int, int, int, Uint32);
SDL_Renderer *__real_SDL_CreateRenderer(SDL_Window *, int, Uint32);
SDL_Texture *__real_SDL_CreateTexture(SDL_Renderer *, Uint32, int, int, int);
void __real_SDL_SetWindowSize(SDL_Window *, int, int);
int __real_SDL_SetWindowFullscreen(SDL_Window *, Uint32);

/* Present hook in ps5_virtual_mouse.c. */
extern void (*ps5_present_hook)(SDL_Renderer *renderer);
static void trace_present(SDL_Renderer *renderer);

__attribute__((constructor)) static void install_present_hook(void)
{
    ps5_present_hook = trace_present;
}

SDL_Window *__wrap_SDL_CreateWindow(const char *title, int x, int y, int w, int h, Uint32 flags)
{
    SDL_Window *win = __real_SDL_CreateWindow(title, x, y, w, h, flags);
    printf("sdl-trace: CreateWindow(%dx%d, flags 0x%x) -> %p %s\n", w, h, flags,
           (void *)win, win ? "" : SDL_GetError());
    return win;
}

SDL_Renderer *__wrap_SDL_CreateRenderer(SDL_Window *win, int index, Uint32 flags)
{
    SDL_Renderer *r = __real_SDL_CreateRenderer(win, index, flags);
    SDL_RendererInfo info = { 0 };
    int ow = 0, oh = 0;
    if (r) {
        SDL_GetRendererInfo(r, &info);
        SDL_GetRendererOutputSize(r, &ow, &oh);
    }
    printf("sdl-trace: CreateRenderer(flags 0x%x) -> %p %s, info flags 0x%x, output %dx%d %s\n",
           flags, (void *)r, info.name ? info.name : "-", info.flags, ow, oh,
           r ? "" : SDL_GetError());
    return r;
}

SDL_Texture *__wrap_SDL_CreateTexture(SDL_Renderer *r, Uint32 format, int access, int w, int h)
{
    SDL_Texture *t = __real_SDL_CreateTexture(r, format, access, w, h);
    printf("sdl-trace: CreateTexture(%s, access %d, %dx%d) -> %p %s\n",
           SDL_GetPixelFormatName(format), access, w, h, (void *)t, t ? "" : SDL_GetError());
    return t;
}

void __wrap_SDL_SetWindowSize(SDL_Window *win, int w, int h)
{
    __real_SDL_SetWindowSize(win, w, h);
    int nw, nh;
    SDL_GetWindowSize(win, &nw, &nh);
    printf("sdl-trace: SetWindowSize(%dx%d) -> %dx%d\n", w, h, nw, nh);
}

int __wrap_SDL_SetWindowFullscreen(SDL_Window *win, Uint32 flags)
{
    int rc = __real_SDL_SetWindowFullscreen(win, flags);
    printf("sdl-trace: SetWindowFullscreen(0x%x) -> %d %s\n", flags, rc, rc ? SDL_GetError() : "");
    return rc;
}

static void trace_present(SDL_Renderer *renderer)
{
    static unsigned frames;

    frames++;
    if (frames > 3 && frames != 60 && frames != 600 && frames % 3600 != 0) {
        return;
    }

    SDL_Window *win = SDL_RenderGetWindow(renderer);
    int ww = 0, wh = 0, ow = 0, oh = 0;
    if (win) {
        SDL_GetWindowSize(win, &ww, &wh);
    }
    SDL_GetRendererOutputSize(renderer, &ow, &oh);

    SDL_Surface *s = win ? SDL_GetWindowSurface(win) : NULL;
    /* How much of a 16x16 sample grid is not black. */
    int lit = 0;
    if (s && s->pixels && s->format->BytesPerPixel == 4) {
        for (int gy = 0; gy < 16; gy++) {
            for (int gx = 0; gx < 16; gx++) {
                int x = (2 * gx + 1) * s->w / 32, y = (2 * gy + 1) * s->h / 32;
                Uint32 px = ((Uint32 *)((Uint8 *)s->pixels + y * s->pitch))[x];
                lit += (px & 0x00ffffff) != 0;
            }
        }
    }
    printf("sdl-trace: present #%u window %dx%d output %dx%d surface %dx%d %s, %d/256 samples lit\n",
           frames, ww, wh, ow, oh, s ? s->w : 0, s ? s->h : 0,
           s ? SDL_GetPixelFormatName(s->format->format) : "-", lit);
}
