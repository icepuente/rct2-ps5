/*
 * Texture formats for SDL's software renderer on PS5.
 *
 * The software renderer advertises 16-bit formats too, and some apps pick the
 * smallest one (OpenRCT2 does, and its 16-bit upload path is broken). Report
 * only 32-bit formats, with the window surface's ABGR8888 first, so textures
 * are copied to the screen without conversion.
 */
#include <SDL.h>

int __real_SDL_GetRendererInfo(SDL_Renderer *renderer, SDL_RendererInfo *info);

int __wrap_SDL_GetRendererInfo(SDL_Renderer *renderer, SDL_RendererInfo *info)
{
    int rc = __real_SDL_GetRendererInfo(renderer, info);
    if (rc != 0 || !info->name || SDL_strcmp(info->name, "software") != 0) {
        return rc;
    }

    Uint32 formats[SDL_arraysize(info->texture_formats)];
    Uint32 count = 0;
    formats[count++] = SDL_PIXELFORMAT_ABGR8888;
    for (Uint32 i = 0; i < info->num_texture_formats; i++) {
        Uint32 f = info->texture_formats[i];
        if (f != SDL_PIXELFORMAT_ABGR8888 && !SDL_ISPIXELFORMAT_FOURCC(f) &&
            SDL_BYTESPERPIXEL(f) == 4 && count < SDL_arraysize(formats)) {
            formats[count++] = f;
        }
    }
    SDL_memcpy(info->texture_formats, formats, count * sizeof(Uint32));
    info->num_texture_formats = count;
    return 0;
}
