/*
 * PS5 entry point for OpenRCT2.
 *
 * Points OpenRCT2 at the packaged data, the player's RollerCoaster Tycoon 2
 * files and the title's writable storage, then runs OpenRCT2's own main()
 * (renamed to openrct2_main by scripts/package-openrct2.sh).
 */
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

#include <SDL2/SDL.h>

#define DATA_PATH "/app0/assets/openrct2"
#define RCT2_PATH "/app0/assets/rct2"
#define USER_PATH "/download0/openrct2"
#define DEFAULT_CONFIG "/app0/assets/config.ini"
#define LOADING_SCREEN "/app0/assets/loading.bmp"

/*
 * Bump when ps5/assets/config.ini changes in a way existing installs need:
 * the defaults are then reinstalled once over the player's config.
 */
#define CONFIG_DEFAULTS_VERSION 2
#define CONFIG_VERSION_FILE USER_PATH "/ps5-config-version"

/*
 * Bump to make existing installs rebuild OpenRCT2's object, scenario and
 * track indexes once (e.g. after an index was built by a broken build).
 */
#define CACHE_VERSION 5
#define CACHE_VERSION_FILE USER_PATH "/ps5-cache-version"

#define AUTOTEST_PLUGIN USER_PATH "/plugin/ps5-autotest.js"
#define AUTOTEST_SOURCE "/app0/assets/autotest/ps5-autotest.js"
#define AUTOTEST_SECONDS_PER_PARK 20

int openrct2_main(int argc, const char **argv);

static int read_marker(const char *path)
{
    int value = 0;
    FILE *f = fopen(path, "r");
    if (f) {
        if (fscanf(f, "%d", &value) != 1) {
            value = 0;
        }
        fclose(f);
    }
    return value;
}

static void write_marker(const char *path, int value)
{
    FILE *f = fopen(path, "w");
    if (f) {
        fprintf(f, "%d\n", value);
        fclose(f);
    }
}

/* Delete OpenRCT2's indexes when CACHE_VERSION is newer than the installed one. */
static void reset_caches(void)
{
    if (read_marker(CACHE_VERSION_FILE) >= CACHE_VERSION) {
        return;
    }
    printf("openrct2-ps5: rebuilding indexes (cache v%d)\n", CACHE_VERSION);
    remove(USER_PATH "/objects.idx");
    remove(USER_PATH "/scenarios.idx");
    remove(USER_PATH "/tracks.idx");
    write_marker(CACHE_VERSION_FILE, CACHE_VERSION);
}

/*
 * Install the TV-friendly default configuration on first run, and again when
 * CONFIG_DEFAULTS_VERSION is newer than the installed one.
 */
static void install_default_config(void)
{
    int installed = read_marker(CONFIG_VERSION_FILE);
    struct stat st;
    if (stat(USER_PATH "/config.ini", &st) == 0 && installed >= CONFIG_DEFAULTS_VERSION) {
        return;
    }
    printf("openrct2-ps5: installing default config (v%d)\n", CONFIG_DEFAULTS_VERSION);

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

    write_marker(CONFIG_VERSION_FILE, CONFIG_DEFAULTS_VERSION);
}

#ifdef OPENRCT2_PS5_AUTOTEST
static int compare_names(const void *a, const void *b)
{
    return strcasecmp(*(const char *const *)a, *(const char *const *)b);
}

static int is_scenario(const char *name)
{
    const char *ext = strrchr(name, '.');
    return ext && (strcasecmp(ext, ".sc6") == 0 || strcasecmp(ext, ".sea") == 0 ||
                   strcasecmp(ext, ".park") == 0);
}

/*
 * Install the scenario tour plugin (ps5/autotest/ps5-autotest.js), prefixed
 * with the list of installed scenarios, which plugins cannot read themselves.
 */
static void install_autotest(void)
{
    char *names[512];
    int count = 0;
    DIR *dir = opendir(RCT2_PATH "/Scenarios");
    struct dirent *entry;
    while (dir && (entry = readdir(dir)) != NULL && count < 512) {
        if (is_scenario(entry->d_name)) {
            names[count++] = strdup(entry->d_name);
        }
    }
    if (dir) {
        closedir(dir);
    }
    qsort(names, count, sizeof(names[0]), compare_names);

    mkdir(USER_PATH "/plugin", 0755);
    FILE *in = fopen(AUTOTEST_SOURCE, "rb");
    FILE *out = fopen(AUTOTEST_PLUGIN, "wb");
    if (in && out) {
        fprintf(out, "var PS5_AUTOTEST = { secondsPerPark: %d, scenarios: [\n",
                AUTOTEST_SECONDS_PER_PARK);
        for (int i = 0; i < count; i++) {
            fprintf(out, "    \"%s/Scenarios/%s\",\n", RCT2_PATH, names[i]);
        }
        fprintf(out, "] };\n");
        char buf[4096];
        size_t n;
        while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
            fwrite(buf, 1, n, out);
        }
        printf("openrct2-ps5: autotest installed (%d scenarios)\n", count);
    } else {
        printf("openrct2-ps5: cannot install autotest\n");
    }
    if (in) {
        fclose(in);
    }
    if (out) {
        fclose(out);
    }
    for (int i = 0; i < count; i++) {
        free(names[i]);
    }
}
#endif

/*
 * Show the loading screen until OpenRCT2 presents its first frame. The video
 * subsystem stays initialised, so the PS5 keeps displaying the last flipped
 * buffer while OpenRCT2 starts up (which takes a while on first launch).
 */
static void show_loading_screen(void)
{
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
        printf("openrct2-ps5: no loading screen: %s\n", SDL_GetError());
        return;
    }

    SDL_Surface *image = SDL_LoadBMP(LOADING_SCREEN);
    SDL_DisplayMode mode;
    SDL_GetCurrentDisplayMode(0, &mode);
    SDL_Window *window = SDL_CreateWindow("OpenRCT2", 0, 0, mode.w, mode.h, 0);
    SDL_Surface *screen = window ? SDL_GetWindowSurface(window) : NULL;

    if (image && screen) {
        SDL_BlitScaled(image, NULL, screen, NULL);
        SDL_UpdateWindowSurface(window);
    } else {
        printf("openrct2-ps5: no loading screen: %s\n", SDL_GetError());
    }

    SDL_FreeSurface(image);
    if (window) {
        SDL_DestroyWindow(window);
    }
}

int main(void)
{
    printf("openrct2-ps5: starting\n");
    mkdir(USER_PATH, 0755);
    install_default_config();
    reset_caches();
#ifdef OPENRCT2_PS5_AUTOTEST
    install_autotest();
#else
    remove(AUTOTEST_PLUGIN);
#endif

    /* The PS5 SDL port only has the software renderer. */
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "software");
    SDL_SetHint(SDL_HINT_FRAMEBUFFER_ACCELERATION, "0");
    show_loading_screen();

    const char *argv[] = {
        "openrct2",
#ifdef OPENRCT2_PS5_DEBUG
        "--verbose",
#endif
        "--openrct2-data-path", DATA_PATH,
        "--rct2-data-path", RCT2_PATH,
        "--user-data-path", USER_PATH,
        NULL,
    };
    int rc = openrct2_main((int)(sizeof(argv) / sizeof(argv[0])) - 1, argv);
    printf("openrct2-ps5: exited with %d\n", rc);
    return rc;
}
