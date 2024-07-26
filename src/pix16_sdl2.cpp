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

static b32 should_quit = false;

#define tau 6.2831853
float phase = 0, step = 440*tau/44100;

function void sdl2__audio_callback2(void *user, Uint8 *stream, int len)
{
  short *out = (short*)stream;

  for (int i = 0; i < len/2; i++) 
  {
      out[i] = (short)5000*sinf(phase);
      i += 1;
      out[i] = (short)5000*sinf(phase);

      phase+=step;
      if(phase >= tau){ phase -= tau; }    
  }
}

struct Audio_Output
{
    i64 playback_pos;
    // i64 samples_played;

    u64 user_safe_size;
    u8 *user_samples;
    u8 *read;
    u8 *write;

    b32 wrapped;
};

static Audio_Output audio = {};

function void sdl2__audio_callback(void *user, Uint8 *stream, int len)
{
    // u32 sample_count = len/(2 * sizeof(i16));
    // audio.playback_pos += sample_count;

    if (audio.read + len <= audio.write || audio.wrapped)
    {
        MemoryCopy(stream, audio.read, len);
        audio.read += len;

        if (audio.read > audio.user_samples + audio.user_safe_size) {
            audio.read = audio.user_samples;
            audio.wrapped = false;
        }
    }
    else
    {
        MemoryZero(stream, len);
    }
}

function f32 sdl2__process_controller_value(i16 value, i16 deadzone_threshold)
{
    f32 result = 0;

    if (value < -deadzone_threshold) {
        result = (f32)((value + deadzone_threshold) / (32768.0f - deadzone_threshold));
    } else if (value > deadzone_threshold) {
        result = (f32)((value - deadzone_threshold) / (32767.0f - deadzone_threshold));
    }

    return result;
}

int main()
{
    os_init();

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) < 0)
    {
        printf("[main] could not initialize sdl2: %s\n", SDL_GetError());
        return 1;
    }

    u32 flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN;
    // flags |= SDL_WINDOW_ALLOW_HIGHDPI;
    SDL_Rect screenRect = { 0, 0, game_width, game_height };
    SDL_Window *window = SDL_CreateWindow("pix16", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 1024, flags);
    SDL_SetWindowMinimumSize(window, game_width, game_height);
    SDL_CaptureMouse(SDL_TRUE);

    if (!window)
    {
        printf("[main] Could not create window: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    SDL_Texture *texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, game_width, game_height);

    Arena *permanant_storage = arena_alloc(Megabytes(64));

    SDL_AudioSpec want, have;
    SDL_zero(want);
    want.freq = 44100;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = 512;
    want.callback = sdl2__audio_callback;
    SDL_AudioDeviceID audio_device = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    SDL_PauseAudioDevice(audio_device, 0);

    static i64 audio_samples_queued = 0;
    static u8 *audio_user_samples = (u8 *)os_alloc(Megabytes(4));
    static i64 audio_latency = 0;

    // audio.user_samples = (u8 *)os_alloc(Megabytes(32));
    audio.user_safe_size = Kilobytes(512);
    audio.user_samples = (u8 *)os_alloc(audio.user_safe_size * 2);
    audio.read = audio.user_samples;
    audio.write = audio.user_samples;

    // printf("[SDL] Obtained - frequency: %d format: f %d s %d be %d sz %d channels: %d samples: %d\n", have.freq, SDL_AUDIO_ISFLOAT(have.format), SDL_AUDIO_ISSIGNED(have.format), SDL_AUDIO_ISBIGENDIAN(have.format), SDL_AUDIO_BITSIZE(have.format), have.channels, have.samples);

    GameInit();

    f64 then = os_time();
    f64 accumulator = 0.0;
    f64 average_dt = 0.0;
    f64 min_dt = 0.0;
    f64 max_dt = 0.0;
    i64 frame_index = 0;

    const Uint8 *keys = SDL_GetKeyboardState(NULL);

    SDL_RenderSetVSync(renderer, 1);

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

        // NOTE(nick): reset temporary storage
        arena_reset(temp_arena());

        int window_width;
        int window_height;
        SDL_GetWindowSize(window, &window_width, &window_height);

        u32 window_flags = SDL_GetWindowFlags(window);
        b32 window_is_focused = (window_flags & SDL_WINDOW_INPUT_FOCUS) != 0;

        // int rw = 0, rh = 0;
        // SDL_GetRendererOutputSize(renderer, &rw, &rh);
        // float widthScale = (float)rw / (float) window_width;
        // float heightScale = (float)rh / (float) window_height;
        // Dump2(widthScale, heightScale);

        Rectangle2i dest_rect = aspect_ratio_fit(game_width, game_height, window_width, window_height);
        if (game_pixel_perfect)
        {
            dest_rect = aspect_ratio_fit_pixel_perfect(game_width, game_height, window_width, window_height);
        }

        static Game_Input input = {};
        {
            input.arena = permanant_storage;
            input.dt = dt;
            input.time = os_time();

            //
            // Mouse
            //

            // NOTE(nick): get local mouse state (doesn't respond when window is not focused)
            u32 state = SDL_GetMouseState(0, 0);

            Vector2i point = {0};
            int wx, wy;
            SDL_GetWindowPosition(window, &wx, &wy);
            SDL_GetGlobalMouseState(&point.x, &point.y);
            point.x -= wx;
            point.y -= wy;

            input.mouse.position = v2(
                game_width * ((point.x - dest_rect.x0) / (f32)r2i_width(dest_rect)),
                game_height * ((point.y - dest_rect.y0) / (f32)r2i_height(dest_rect)));

            input.mouse.left = (state & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0;
            input.mouse.right = (state & SDL_BUTTON(SDL_BUTTON_RIGHT)) != 0;

            //
            // Controllers
            //

            MemoryZero(&input.controllers, count_of(input.controllers) * sizeof(Controller));

            Controller *player0 = &input.controllers[0];
            player0->up    |= keys[SDL_SCANCODE_UP];
            player0->down  |= keys[SDL_SCANCODE_DOWN];
            player0->left  |= keys[SDL_SCANCODE_LEFT];
            player0->right |= keys[SDL_SCANCODE_RIGHT];
            player0->a     |= keys[SDL_SCANCODE_X];
            player0->b     |= keys[SDL_SCANCODE_C];

            player0->up    |= keys[SDL_SCANCODE_W];
            player0->down  |= keys[SDL_SCANCODE_S];
            player0->left  |= keys[SDL_SCANCODE_A];
            player0->right |= keys[SDL_SCANCODE_D];
            player0->a     |= keys[SDL_SCANCODE_J];
            player0->b     |= keys[SDL_SCANCODE_K];

            player0->start |= keys[SDL_SCANCODE_ESCAPE];
            player0->pause |= keys[SDL_SCANCODE_P];


            static SDL_GameController *controllers[16];
            for (int i = 0; i < SDL_NumJoysticks(); i++) {
                if (SDL_IsGameController(i)) {
                    if (!controllers[i])
                    {
                        controllers[i] = SDL_GameControllerOpen(i);
                    }
                }
            }

            int controller_index = 0;
            for (int i = 0; i < 16; i += 1)
            {
                if (controllers[i])
                {
                    Controller *player = &input.controllers[controller_index];
                    SDL_GameController *ctrl = controllers[i];

                    player->up |= SDL_GameControllerGetButton(ctrl, SDL_CONTROLLER_BUTTON_DPAD_UP);
                    player->down |= SDL_GameControllerGetButton(ctrl, SDL_CONTROLLER_BUTTON_DPAD_DOWN);
                    player->left |= SDL_GameControllerGetButton(ctrl, SDL_CONTROLLER_BUTTON_DPAD_LEFT);
                    player->right |= SDL_GameControllerGetButton(ctrl, SDL_CONTROLLER_BUTTON_DPAD_RIGHT);

                    player->a |= SDL_GameControllerGetButton(ctrl, SDL_CONTROLLER_BUTTON_A);
                    player->b |= SDL_GameControllerGetButton(ctrl, SDL_CONTROLLER_BUTTON_B);

                    player->start |= SDL_GameControllerGetButton(ctrl, SDL_CONTROLLER_BUTTON_START);
                    player->pause |= SDL_GameControllerGetButton(ctrl, SDL_CONTROLLER_BUTTON_BACK);


                    f32 stick_x = sdl2__process_controller_value(SDL_GameControllerGetAxis(ctrl, SDL_CONTROLLER_AXIS_LEFTX), 0);
                    f32 stick_y = sdl2__process_controller_value(SDL_GameControllerGetAxis(ctrl, SDL_CONTROLLER_AXIS_LEFTY), 0);

                    player->stick_x = stick_x;
                    player->stick_y = stick_y;

                    controller_index += 1;
                }
            }
        }

        static Game_Output output = {};
        
        int pitch;
        u32 *pixels;
        SDL_LockTexture(texture, NULL, (void**)&pixels, &pitch);

        output.pixels = pixels;
        output.width  = game_width;
        output.height = game_height;

        i64 LatencySamples = have.samples * 4;
        u64 SampleSize = 2 * sizeof(i16);

        i64 read_target = (i64)audio.read + LatencySamples * SampleSize;
        i64 write_target = (i64)audio.write;

        u32 UserSampleCount = 0;
        if (read_target >= write_target)
        {
            UserSampleCount = (read_target - write_target)/(SampleSize);
        }

        // NOTE(nick): when wrapping, we would get a garbage value here
        if (UserSampleCount > LatencySamples) UserSampleCount = LatencySamples;

        i16 *UserSamples = (i16 *)audio.write;
        MemoryZero(UserSamples, UserSampleCount * 2 * sizeof(i16));

        output.samples_per_second = 44100;
        output.sample_count = UserSampleCount;
        output.samples = (i16 *)UserSamples;

        GameSetState(&input, &output);
        GameUpdateAndRender(&input, &output);

        SDL_UnlockTexture(texture);

        if (UserSampleCount > 0)
        {
            audio.write += UserSampleCount * 2 * sizeof(i16);
            output.samples_played += UserSampleCount;

            if (audio.write > audio.user_samples + audio.user_safe_size) {
                audio.write = audio.user_samples;
                audio.wrapped = true;
            }
        }


        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        SDL_Rect displayRect = {0};
        displayRect.x = dest_rect.x0;
        displayRect.y = dest_rect.y0;
        displayRect.w = dest_rect.x1 - dest_rect.x0;
        displayRect.h = dest_rect.y1 - dest_rect.y0;
        SDL_RenderCopy(renderer, texture, &screenRect, &displayRect);

        SDL_RenderPresent(renderer);

        now = os_time();
        f64 remaining_seconds = target_dt - (now - then);

        // NOTE(nick): wait for next frame
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
    }

    SDL_CloseAudioDevice(audio_device);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}