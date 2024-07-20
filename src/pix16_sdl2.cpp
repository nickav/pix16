#include <stdio.h>
#include <SDL2/SDL.h>

#define STB_SPRINTF_IMPLEMENTATION
#include "third_party/stb_sprintf.h"

static char *unix__print_callback(const char *buf, void *user, int len) {
    fprintf(stdout, "%.*s", len, buf);
    return (char *)buf;
}

static void unix__print(const char *format, ...) {
    char buffer[1024];

    va_list args;
    va_start(args, format);
    stbsp_vsprintfcb(unix__print_callback, 0, buffer, format, args);
    fflush(stdout);

    va_end(args);
}

#define PrintToBuffer stbsp_vsnprintf
#define print unix__print

#define impl
#include "third_party/na.h"
#include "third_party/na_math.h"

//
// NOTE(nick): game display settings
//

static i32 game_width = 320;
static i32 game_height = 240;
static b32 game_pixel_perfect = true;

#include "game.h"
#include "game.cpp"

#include "user.cpp"

inline uint32_t u32_color_argb(uint8_t a, uint8_t r, uint8_t g, uint8_t b) { return (a<<24) | (r << 16) | (g << 8) | (b << 0); }
inline uint32_t u32_color_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a) { return (a<<24) | (r << 16) | (g << 8) | (b << 0); }

static b32 should_quit = false;

int main()
{
    os_init();

    if (SDL_Init(SDL_INIT_EVERYTHING) < 0)
    {
        printf("could not initialize sdl2: %s\n", SDL_GetError());
        return 1;
    }

    u32 flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN;
    SDL_Rect screenRect = { 0,0,512,512 };
    SDL_Window *window = SDL_CreateWindow("pix16", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 1024, flags);
    SDL_SetWindowTitle(window, "pix16");

    if (!window)
    {
        printf("could not create window: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    SDL_Texture *texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, screenRect.w, screenRect.h);

    GameInit();

    int frame = 0;

    f64 then = os_time();
    f64 accumulator = 0.0;
    f64 average_dt = 0.0;
    f64 min_dt = 0.0;
    f64 max_dt = 0.0;
    i64 frame_index = 0;

    while (!should_quit)
    {
        // NOTE(nick): running at 60 fps locked because this is more similar to retro consoles
        f64 target_dt = (1.0 / 60.0);

        f64 now = os_time();
        f64 dt = now - then;
        then = now;

        // NOTE(nick): Debug frame timings
        {
            if (frame_index % 100 == 10)
            {
                min_dt = dt;
                max_dt = dt;
            }

            average_dt = 0.9 * average_dt + 0.1 * dt;
            min_dt = min_f64(min_dt, dt);
            max_dt = max_f64(max_dt, dt);

            frame_index += 1;

            String debug_fps = sprint("%02dms | %02dms | %02dms", (i32)(min_dt * 1000), (i32)(average_dt * 1000), (i32)(max_dt * 1000));
            char *title_c = string_to_cstr(temp_arena(), debug_fps);
            SDL_SetWindowTitle(window, title_c);
        }

        // NOTE(nick): clamp large spikes in framerate
        if (dt > 0.25) dt = 0.25;

        // NOTE(nick): poll for events
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            switch (event.type)
            {
                case SDL_QUIT:
                {
                    should_quit = true;
                } break;

                case SDL_KEYDOWN:
                {
                    if (event.key.keysym.sym == SDLK_F11)
                    {
                        u32 flags = SDL_GetWindowFlags(window);
                        bool is_fullscreen = flags & SDL_WINDOW_FULLSCREEN;

                        SDL_SetWindowFullscreen(window, !is_fullscreen);
                        if (is_fullscreen)
                        {
                            SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
                        }
                    }
                } break;
            }
        }

        int pitch;
        uint32_t *pixels;
        SDL_LockTexture(texture, &screenRect, (void**)&pixels, &pitch);
        for (int y = 0; y < screenRect.h; y++) {
            for (int x = 0; x < screenRect.w; x++) {
                // pixels[y*screenRect.w + x] = u32_color_argb(255, frame>>3, y+frame, x+frame);
                // pixels[y*screenRect.w + x] = rgba_u32_from_u8(frame>>3, y+frame, x+frame, 255);
                pixels[y*screenRect.w + x] = u32_color_rgba(frame>>3, y+frame, x+frame, 255);
            }
        }
        SDL_UnlockTexture(texture);
        frame += 1;

        // NOTE(nick): reset temporary storage
        arena_reset(temp_arena());

        static Game_Input input = {};
        {
        }

        static Game_Output output = {};
        // output.pixels = win32_framebuffer.pixels;
        // output.width  = win32_framebuffer.width;
        // output.height = win32_framebuffer.height;

        // GameUpdateAndRender(&input, &output);


        SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, 0xFF);
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, &screenRect, &screenRect);
        SDL_RenderPresent(renderer);

        now = os_time();
        #if 1
        f64 remaining_seconds = target_dt - (now - then);

        // NOTE(nick): wait for next frame
        u32 flags = SDL_GetWindowFlags(window);
        b32 window_is_focused = (flags & SDL_WINDOW_INPUT_FOCUS) != 0;
        if (window_is_focused)
        {
            while (now - then < target_dt)
            {
                now = os_time();

                f64 wait_s = target_dt - (now - then);
                if (wait_s > 2.0 / 1000.0)
                {
                    // NOTE(nick): sleep for 80% of the time to account for sleep inaccuracies
                    os_sleep(wait_s * 0.8);
                }
            }
        }
        else
        {
            // NOTE(nick): we're in the background so we don't have to care too much about exact timing.
            now = os_time();
            if (now - then < target_dt) {
                os_sleep(target_dt);
            }
        }
        #endif
    }

    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}