/*
 * Virtual mouse for mouse-driven SDL2 apps on PS5.
 *
 * The PS5 has no mouse cursor, so the controller drives one: SDL calls the app
 * makes are redirected here with lld --wrap (scripts/package-native.sh with
 * PS5_SDL_APP=1), which turns controller input into mouse and key
 * events and draws the cursor before each frame is presented.
 *
 *   Left stick   move the cursor (accelerates with deflection)
 *   Right stick  reported to the app as the left stick (OpenRCT2 scrolls with it)
 *   Cross        left mouse button     Circle    right mouse button
 *   L1 / R1      wheel down / up       Triangle  Return
 *   Square       Backspace             Options   Escape
 *   D-pad        jump to the next button in that direction, when the app
 *                installs ps5_snap_hook (OpenRCT2: ps5/ui_snap.cpp)
 *   Everything else passes through unchanged.
 */
#include <math.h>
#include <stdbool.h>
#include <string.h>

#include <SDL.h>

int __real_SDL_PollEvent(SDL_Event *event);
Uint32 __real_SDL_GetMouseState(int *x, int *y);
void __real_SDL_WarpMouseInWindow(SDL_Window *window, int x, int y);
int __real_SDL_ShowCursor(int toggle);
void __real_SDL_RenderPresent(SDL_Renderer *renderer);
Sint16 __real_SDL_GameControllerGetAxis(SDL_GameController *controller,
                                        SDL_GameControllerAxis axis);
/* Called after each present when set (by ps5_sdl_trace.c). */
void (*ps5_present_hook)(SDL_Renderer *renderer);

/*
 * D-pad navigation, when set by the app: given a direction (0 left, 1 up,
 * 2 right, 3 down) and the cursor, return 1 and the next cursor position.
 */
int (*ps5_snap_hook)(int dir, int x, int y, int *out_x, int *out_y);

#define SNAP_REPEAT_DELAY 350 /* ms before a held D-pad repeats */
#define SNAP_REPEAT_RATE 140

static int snap_dir = -1;
static Uint32 snap_next;

static void snap(int dir);

#define DEADZONE 7000
#define MAX_SPEED 1400.0f /* pixels per second at full deflection */
#define QUEUE_SIZE 16

static float cursor_x = 960.0f, cursor_y = 540.0f;
static Uint32 buttons;
static bool cursor_visible = true;
static Uint64 last_counter;
static bool frame_started = true;

static SDL_Event queue[QUEUE_SIZE];
static int queue_head, queue_len;

static void push(const SDL_Event *ev)
{
    if (queue_len < QUEUE_SIZE) {
        queue[(queue_head + queue_len++) % QUEUE_SIZE] = *ev;
    }
}

static bool pop(SDL_Event *ev)
{
    if (queue_len == 0) {
        return false;
    }
    *ev = queue[queue_head];
    queue_head = (queue_head + 1) % QUEUE_SIZE;
    queue_len--;
    return true;
}

static SDL_GameController *controller(void)
{
    for (int i = 0; i < SDL_NumJoysticks(); i++) {
        if (SDL_IsGameController(i)) {
            SDL_GameController *gc = SDL_GameControllerOpen(i);
            if (gc) {
                return gc;
            }
        }
    }
    return NULL;
}

static void window_size(int *w, int *h)
{
    SDL_Window *win = SDL_GetMouseFocus();
    if (!win) {
        win = SDL_GetKeyboardFocus();
    }
    if (win) {
        SDL_GetWindowSize(win, w, h);
    } else {
        *w = 1920;
        *h = 1080;
    }
}

static Uint32 window_id(void)
{
    SDL_Window *win = SDL_GetMouseFocus();
    return win ? SDL_GetWindowID(win) : 1;
}

static float stick(Sint16 value)
{
    int v = value;
    if (v > -DEADZONE && v < DEADZONE) {
        return 0.0f;
    }
    float f = (v > 0 ? v - DEADZONE : v + DEADZONE) / (32767.0f - DEADZONE);
    return f * fabsf(f); /* fine control near the centre */
}

/* Once per frame: repeat a held D-pad, and move the cursor by the left stick. */
static void move_cursor(void)
{
    if (snap_dir >= 0 && SDL_TICKS_PASSED(SDL_GetTicks(), snap_next)) {
        snap(snap_dir);
        snap_next = SDL_GetTicks() + SNAP_REPEAT_RATE;
    }

    Uint64 now = SDL_GetPerformanceCounter();
    float dt = last_counter ? (float)(now - last_counter) / SDL_GetPerformanceFrequency() : 0.0f;
    last_counter = now;
    if (dt > 0.1f) {
        dt = 0.1f;
    }

    SDL_GameController *gc = controller();
    if (!gc) {
        return;
    }
    float dx = stick(__real_SDL_GameControllerGetAxis(gc, SDL_CONTROLLER_AXIS_LEFTX)) * MAX_SPEED * dt;
    float dy = stick(__real_SDL_GameControllerGetAxis(gc, SDL_CONTROLLER_AXIS_LEFTY)) * MAX_SPEED * dt;
    if (dx == 0.0f && dy == 0.0f) {
        return;
    }

    int w, h;
    window_size(&w, &h);
    int old_x = (int)cursor_x, old_y = (int)cursor_y;
    cursor_x = SDL_clamp(cursor_x + dx, 0.0f, (float)(w - 1));
    cursor_y = SDL_clamp(cursor_y + dy, 0.0f, (float)(h - 1));
    if ((int)cursor_x == old_x && (int)cursor_y == old_y) {
        return;
    }

    SDL_Event ev = { 0 };
    ev.motion.type = SDL_MOUSEMOTION;
    ev.motion.timestamp = SDL_GetTicks();
    ev.motion.windowID = window_id();
    ev.motion.state = buttons;
    ev.motion.x = (int)cursor_x;
    ev.motion.y = (int)cursor_y;
    ev.motion.xrel = (int)cursor_x - old_x;
    ev.motion.yrel = (int)cursor_y - old_y;
    push(&ev);
}

static void warp_cursor(int x, int y)
{
    int old_x = (int)cursor_x, old_y = (int)cursor_y;
    cursor_x = (float)x;
    cursor_y = (float)y;

    SDL_Event ev = { 0 };
    ev.motion.type = SDL_MOUSEMOTION;
    ev.motion.timestamp = SDL_GetTicks();
    ev.motion.windowID = window_id();
    ev.motion.state = buttons;
    ev.motion.x = x;
    ev.motion.y = y;
    ev.motion.xrel = x - old_x;
    ev.motion.yrel = y - old_y;
    push(&ev);
}

static void snap(int dir)
{
    int x, y;
    if (ps5_snap_hook && ps5_snap_hook(dir, (int)cursor_x, (int)cursor_y, &x, &y)) {
        warp_cursor(x, y);
    }
}

static void mouse_button(Uint8 button, bool down)
{
    SDL_Event ev = { 0 };
    ev.button.type = down ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
    ev.button.timestamp = SDL_GetTicks();
    ev.button.windowID = window_id();
    ev.button.button = button;
    ev.button.state = down ? SDL_PRESSED : SDL_RELEASED;
    ev.button.clicks = 1;
    ev.button.x = (int)cursor_x;
    ev.button.y = (int)cursor_y;
    if (down) {
        buttons |= SDL_BUTTON(button);
    } else {
        buttons &= ~SDL_BUTTON(button);
    }
    push(&ev);
}

static void wheel(int y)
{
    SDL_Event ev = { 0 };
    ev.wheel.type = SDL_MOUSEWHEEL;
    ev.wheel.timestamp = SDL_GetTicks();
    ev.wheel.windowID = window_id();
    ev.wheel.y = y;
    ev.wheel.preciseY = (float)y;
    ev.wheel.direction = SDL_MOUSEWHEEL_NORMAL;
    ev.wheel.mouseX = (int)cursor_x;
    ev.wheel.mouseY = (int)cursor_y;
    push(&ev);
}

static void key(SDL_Scancode scancode, bool down)
{
    SDL_Event ev = { 0 };
    ev.key.type = down ? SDL_KEYDOWN : SDL_KEYUP;
    ev.key.timestamp = SDL_GetTicks();
    ev.key.windowID = window_id();
    ev.key.state = down ? SDL_PRESSED : SDL_RELEASED;
    ev.key.keysym.scancode = scancode;
    ev.key.keysym.sym = SDL_GetKeyFromScancode(scancode);
    push(&ev);
}

/* Returns true when the event was consumed (replaced by queued events). */
static bool translate(const SDL_Event *ev)
{
    switch (ev->type) {
    case SDL_CONTROLLERBUTTONDOWN:
    case SDL_CONTROLLERBUTTONUP: {
        bool down = ev->type == SDL_CONTROLLERBUTTONDOWN;
        switch (ev->cbutton.button) {
        case SDL_CONTROLLER_BUTTON_A:
            mouse_button(SDL_BUTTON_LEFT, down);
            return true;
        case SDL_CONTROLLER_BUTTON_B:
            mouse_button(SDL_BUTTON_RIGHT, down);
            return true;
        case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
            if (down) {
                wheel(-1);
            }
            return true;
        case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
            if (down) {
                wheel(1);
            }
            return true;
        case SDL_CONTROLLER_BUTTON_Y:
            key(SDL_SCANCODE_RETURN, down);
            return true;
        case SDL_CONTROLLER_BUTTON_X:
            key(SDL_SCANCODE_BACKSPACE, down);
            return true;
        case SDL_CONTROLLER_BUTTON_START:
            key(SDL_SCANCODE_ESCAPE, down);
            return true;
        case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
        case SDL_CONTROLLER_BUTTON_DPAD_UP:
        case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
        case SDL_CONTROLLER_BUTTON_DPAD_DOWN: {
            if (!ps5_snap_hook) {
                return false;
            }
            int dir = ev->cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_LEFT ? 0
                    : ev->cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_UP   ? 1
                    : ev->cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_RIGHT ? 2
                                                                            : 3;
            if (down) {
                snap(dir);
                snap_dir = dir;
                snap_next = SDL_GetTicks() + SNAP_REPEAT_DELAY;
            } else if (snap_dir == dir) {
                snap_dir = -1;
            }
            return true;
        }
        }
        return false;
    }
    case SDL_CONTROLLERAXISMOTION:
        /* The left stick is the cursor; the right one stands in for it. */
        return ev->caxis.axis == SDL_CONTROLLER_AXIS_LEFTX ||
               ev->caxis.axis == SDL_CONTROLLER_AXIS_LEFTY;
    }
    return false;
}

int __wrap_SDL_PollEvent(SDL_Event *event)
{
    /*
     * Controller button events need the game controller subsystem, which an
     * app that only initialises SDL_INIT_JOYSTICK (OpenRCT2) never starts.
     */
    if (!SDL_WasInit(SDL_INIT_GAMECONTROLLER) && SDL_WasInit(SDL_INIT_VIDEO)) {
        SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER);
        controller();
    }

    if (frame_started) {
        frame_started = false;
        move_cursor();
    }

    for (;;) {
        SDL_Event ev;
        if (pop(&ev)) {
            if (event) {
                *event = ev;
            } else {
                push(&ev); /* a NULL poll only asks whether events are pending */
            }
            return 1;
        }
        if (!__real_SDL_PollEvent(&ev)) {
            frame_started = true;
            return 0;
        }
        if (translate(&ev)) {
            continue;
        }
        if (ev.type == SDL_CONTROLLERAXISMOTION) {
            if (ev.caxis.axis == SDL_CONTROLLER_AXIS_RIGHTX) {
                ev.caxis.axis = SDL_CONTROLLER_AXIS_LEFTX;
            } else if (ev.caxis.axis == SDL_CONTROLLER_AXIS_RIGHTY) {
                ev.caxis.axis = SDL_CONTROLLER_AXIS_LEFTY;
            }
        }
        if (event) {
            *event = ev;
        } else {
            push(&ev);
        }
        return 1;
    }
}

Sint16 __wrap_SDL_GameControllerGetAxis(SDL_GameController *gc, SDL_GameControllerAxis axis)
{
    switch (axis) {
    case SDL_CONTROLLER_AXIS_LEFTX:
        return __real_SDL_GameControllerGetAxis(gc, SDL_CONTROLLER_AXIS_RIGHTX);
    case SDL_CONTROLLER_AXIS_LEFTY:
        return __real_SDL_GameControllerGetAxis(gc, SDL_CONTROLLER_AXIS_RIGHTY);
    case SDL_CONTROLLER_AXIS_RIGHTX:
    case SDL_CONTROLLER_AXIS_RIGHTY:
        return 0;
    default:
        return __real_SDL_GameControllerGetAxis(gc, axis);
    }
}

Uint32 __wrap_SDL_GetMouseState(int *x, int *y)
{
    if (x) {
        *x = (int)cursor_x;
    }
    if (y) {
        *y = (int)cursor_y;
    }
    return buttons;
}

void __wrap_SDL_WarpMouseInWindow(SDL_Window *window, int x, int y)
{
    (void)window;
    cursor_x = (float)x;
    cursor_y = (float)y;
}

int __wrap_SDL_ShowCursor(int toggle)
{
    if (toggle == SDL_ENABLE || toggle == SDL_DISABLE) {
        cursor_visible = toggle == SDL_ENABLE;
    }
    return cursor_visible ? SDL_ENABLE : SDL_DISABLE;
}

/* Classic arrow pointer: '#' outline, '.' fill. */
static const char *const arrow[] = {
    "#",
    "##",
    "#.#",
    "#..#",
    "#...#",
    "#....#",
    "#.....#",
    "#......#",
    "#.......#",
    "#........#",
    "#.....#####",
    "#..#..#",
    "#.# #..#",
    "##  #..#",
    "#    #..#",
    "     #..#",
    "      ##",
};

#define CURSOR_SCALE 2

static SDL_Texture *cursor_texture(SDL_Renderer *renderer, int *w, int *h)
{
    static SDL_Texture *texture;
    static SDL_Renderer *owner;
    static int tw, th;

    if (texture && owner == renderer) {
        *w = tw;
        *h = th;
        return texture;
    }

    int rows = (int)(sizeof(arrow) / sizeof(arrow[0]));
    int cols = 0;
    for (int r = 0; r < rows; r++) {
        int len = (int)strlen(arrow[r]);
        cols = len > cols ? len : cols;
    }

    SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, cols, rows, 32, SDL_PIXELFORMAT_RGBA8888);
    if (!s) {
        return NULL;
    }
    SDL_FillRect(s, NULL, SDL_MapRGBA(s->format, 0, 0, 0, 0));
    for (int r = 0; r < rows; r++) {
        for (int c = 0; arrow[r][c]; c++) {
            Uint32 px = arrow[r][c] == '#' ? SDL_MapRGBA(s->format, 0, 0, 0, 255)
                      : arrow[r][c] == '.' ? SDL_MapRGBA(s->format, 255, 255, 255, 255)
                                           : SDL_MapRGBA(s->format, 0, 0, 0, 0);
            ((Uint32 *)((Uint8 *)s->pixels + r * s->pitch))[c] = px;
        }
    }

    if (texture) {
        SDL_DestroyTexture(texture);
    }
    texture = SDL_CreateTextureFromSurface(renderer, s);
    SDL_FreeSurface(s);
    if (texture) {
        SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    }
    owner = renderer;
    tw = cols * CURSOR_SCALE;
    th = rows * CURSOR_SCALE;
    *w = tw;
    *h = th;
    return texture;
}

void __wrap_SDL_RenderPresent(SDL_Renderer *renderer)
{
    int w, h;
    SDL_Texture *texture;

    if (cursor_visible && (texture = cursor_texture(renderer, &w, &h))) {
        /* Draw in window pixels, whatever logical size the app uses. */
        SDL_Rect viewport;
        float sx, sy;
        SDL_RenderGetViewport(renderer, &viewport);
        SDL_RenderGetScale(renderer, &sx, &sy);
        SDL_Texture *target = SDL_GetRenderTarget(renderer);
        SDL_SetRenderTarget(renderer, NULL);
        SDL_RenderSetScale(renderer, 1.0f, 1.0f);
        SDL_RenderSetViewport(renderer, NULL);

        SDL_Rect dst = { (int)cursor_x, (int)cursor_y, w, h };
        SDL_RenderCopy(renderer, texture, NULL, &dst);

        SDL_RenderSetViewport(renderer, &viewport);
        SDL_RenderSetScale(renderer, sx, sy);
        SDL_SetRenderTarget(renderer, target);
    }
    __real_SDL_RenderPresent(renderer);
    if (ps5_present_hook) {
        ps5_present_hook(renderer);
    }
}
