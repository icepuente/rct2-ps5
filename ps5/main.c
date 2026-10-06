/*
 * PS5 entry point for OpenRCT2.
 *
 * Points OpenRCT2 at the packaged data, the player's RollerCoaster Tycoon 2
 * files and the title's writable storage, then runs OpenRCT2's own main()
 * (renamed to openrct2_main by scripts/package-openrct2.sh).
 */
#include <stdio.h>
#include <sys/stat.h>

#include <SDL2/SDL.h>

#define DATA_PATH "/app0/assets/openrct2"
#define RCT2_PATH "/app0/assets/rct2"
#define USER_PATH "/download0/openrct2"
#define DEFAULT_CONFIG "/app0/assets/config.ini"

int openrct2_main(int argc, const char **argv);

/* Install the TV-friendly default configuration on first run. */
static void install_default_config(void)
{
    struct stat st;
    if (stat(USER_PATH "/config.ini", &st) == 0) {
        return;
    }

    FILE *in = fopen(DEFAULT_CONFIG, "rb");
    FILE *out = fopen(USER_PATH "/config.ini", "wb");
    if (in && out) {
        char buf[4096];
        size_t n;
        while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
            fwrite(buf, 1, n, out);
        }
    }
    if (in) {
        fclose(in);
    }
    if (out) {
        fclose(out);
    }
}

int main(void)
{
    printf("openrct2-ps5: starting\n");
    mkdir(USER_PATH, 0755);
    install_default_config();

    /* The PS5 SDL port only has the software renderer. */
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "software");
    SDL_SetHint(SDL_HINT_FRAMEBUFFER_ACCELERATION, "0");

    const char *argv[] = {
        "openrct2",
        "--openrct2-data-path", DATA_PATH,
        "--rct2-data-path", RCT2_PATH,
        "--user-data-path", USER_PATH,
        NULL,
    };
    int rc = openrct2_main((int)(sizeof(argv) / sizeof(argv[0])) - 1, argv);
    printf("openrct2-ps5: exited with %d\n", rc);
    return rc;
}
