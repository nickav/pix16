#define WIN32_LEAN_AND_MEAN
#define VC_EXTRALEAN
#define NOMINMAX
#include <windows.h>

//
// NOTE(nick): set up base library code
//

#define STB_SPRINTF_IMPLEMENTATION
#include "third_party/stb_sprintf.h"

static char *win32__print_callback(const char *buf, void *user, int len) {
    DWORD bytes_written;
    HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
    WriteFile(handle, buf, len, &bytes_written, 0);
    return (char *)buf;
}

static void win32__print(const char *format, ...) {
    char buffer[1024];

    va_list args;
    va_start(args, format);
    stbsp_vsprintfcb(win32__print_callback, 0, buffer, format, args);
    va_end(args);
}

#define PrintToBuffer stbsp_vsnprintf
#define print win32__print

#define impl
#include "third_party/na.h"
#include "third_party/na_math.h"
#include "game.h"
#include "game.cpp"
#include "pix16_win32_audio.cpp"

//
// NOTE(nick): win32 entry code
//

#pragma comment(lib, "gdi32")
#pragma comment(lib, "shell32")
#pragma comment(lib, "advapi32")

#ifndef WINDOW_CLASS_NAME
    #define WINDOW_CLASS_NAME L"Pix16 Window Class"
#endif

struct Win32_Framebuffer
{
    i32 width;
    i32 height;
    i32 bytes_per_pixel;
    u32 *pixels;
    
    BITMAPINFO bitmap_info;
    HBITMAP bitmap;
};

//
// NOTE(nick): win32 globals
//
global b32 win32_window_is_fullscreen = false;
global Win32_Framebuffer win32_framebuffer = {0};

static b32 should_quit = false;
static i32 game_width = 320;
static i32 game_height = 240;

function void
win32_toggle_fullscreen(HWND hwnd)
{
    static WINDOWPLACEMENT placement = {0};

    b32 is_fullscreen = false;
    {
        MONITORINFO monitor_info = {0};
        monitor_info.cbSize = sizeof(MONITORINFO);
        GetMonitorInfo(MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY), &monitor_info);

        RECT rect;
        GetWindowRect(hwnd, &rect);

        is_fullscreen = (
            rect.left == monitor_info.rcMonitor.left
            && rect.right == monitor_info.rcMonitor.right
            && rect.top == monitor_info.rcMonitor.top
            && rect.bottom == monitor_info.rcMonitor.bottom
        );
    }

    DWORD style = GetWindowLong(hwnd, GWL_STYLE);

    if (!is_fullscreen)
    {
        MONITORINFO monitor_info = {sizeof(monitor_info)};

        if (
            GetWindowPlacement(hwnd, &placement) &&
            GetMonitorInfo(MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY), &monitor_info)
        ) {
            SetWindowLong(hwnd, GWL_STYLE, style & ~WS_OVERLAPPEDWINDOW);

            SetWindowPos(
                hwnd,
                HWND_TOP,
                monitor_info.rcMonitor.left,
                monitor_info.rcMonitor.top,
                monitor_info.rcMonitor.right  - monitor_info.rcMonitor.left,
                monitor_info.rcMonitor.bottom - monitor_info.rcMonitor.top,
                SWP_NOOWNERZORDER | SWP_FRAMECHANGED
            );
        }
    } else {
        SetWindowLong(hwnd, GWL_STYLE, style | WS_OVERLAPPEDWINDOW);
        SetWindowPlacement(hwnd, &placement);
        DWORD flags = SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED;
        SetWindowPos(hwnd, 0, 0, 0, 0, 0, flags);
    }
}

function b32
win32_resize_framebuffer(Win32_Framebuffer *it, int width, int height)
{
    assert(it);
    assert(width > 0 && height > 0);

    if (it->width == width && it->height == height)
    {
        return false;
    }

    BITMAPINFO *info = &it->bitmap_info;
    info->bmiHeader.biSize        = sizeof(info->bmiHeader);
    info->bmiHeader.biWidth       = width;
    info->bmiHeader.biHeight      = -height; // NOTE(bill): -ve is top-down, +ve is bottom-up
    info->bmiHeader.biPlanes      = 1;
    info->bmiHeader.biBitCount    = 32;
    info->bmiHeader.biCompression = BI_RGB;

    if (it->pixels)
    {
        VirtualFree(it->pixels, 0, MEM_RELEASE);
    }

    it->width = width;
    it->height = height;
    it->bytes_per_pixel = 4;

    i64 size = (it->width * it->height) * it->bytes_per_pixel;
    it->pixels = (u32 *)VirtualAlloc(0, size, MEM_COMMIT, PAGE_READWRITE);

    return true;
}

function LRESULT CALLBACK
win32_window_callback(HWND hwnd, UINT message, WPARAM w_param, LPARAM l_param)
{
    switch (message)
    {
        case WM_CLOSE:
        {
            should_quit = true;
            return DefWindowProcW(hwnd, message, w_param, l_param);
        } break;

        case WM_PAINT:
        {
            //
            // NOTE(nick): this will invalidate any drawing that has happened in the window!
            //
            // We always want the framebuffer to be a fixed size (maybe user configurable?)
            // because it's supposed to look like a pixel art game.
            //
            win32_resize_framebuffer(&win32_framebuffer, game_width, game_height);

            //
            // Re-render here, otherwise because this event blocks while the user
            // is resizing the window, the window will remain black.
            // This is an asethetic choice, so you can disable it if you want to.
            //
            #if 1
            {
                RECT wr = {0};
                GetClientRect(hwnd, &wr);
                int window_width = (int)(wr.right - wr.left);
                int window_height = (int)(wr.bottom - wr.top);

                Win32_Framebuffer *framebuffer = &win32_framebuffer;

                Rectangle2i dest_rect = aspect_ratio_fit(game_width, game_height, window_width, window_height);

                HDC hdc = GetDC(hwnd);

                StretchDIBits(hdc,
                    dest_rect.x0, dest_rect.y0, r2i_width(dest_rect), r2i_height(dest_rect),
                    0, 0, framebuffer->width, framebuffer->height, framebuffer->pixels,
                    &framebuffer->bitmap_info,
                    DIB_RGB_COLORS, SRCCOPY);

                ReleaseDC(hwnd, hdc);
            }
            #endif

            return DefWindowProcW(hwnd, message, w_param, l_param);
        } break;

        case WM_DPICHANGED:
        {
            // Resize windowed mode windows that either permit rescaling or that
            // need it to compensate for non-client area scaling
            RECT *suggested = cast(RECT *)l_param;
            SetWindowPos(hwnd, HWND_TOP,
                            suggested->left,
                            suggested->top,
                            suggested->right - suggested->left,
                            suggested->bottom - suggested->top,
                            SWP_NOACTIVATE | SWP_NOZORDER);
        } break;

        case WM_SYSCOMMAND:
        {
            switch (w_param)
            {
                case SC_SCREENSAVE:
                case SC_MONITORPOWER: {
                    // NOTE(nick): this is where we could disallow screen saver and screen blanking
                    // by not calling the default window proc
                    return DefWindowProcW(hwnd, message, w_param, l_param);
                } break;

                // User trying to access application menu using ALT
                case SC_KEYMENU: {
                    // NOTE(nick): prevent beep sound when pressing alt key combo (e.g. alt + enter)
                    return 0;
                } break;
                
                default:
                {
                    return DefWindowProcW(hwnd, message, w_param, l_param);
                } break;
            }
        } break;

        case WM_SYSKEYDOWN:
        case WM_KEYDOWN:
        {
            b32 was_down = !!(l_param & (1 << 30));
            b32 is_down  =  !(l_param & (1 << 31));

            if (!was_down && is_down)
            {
                if (w_param == VK_F11)
                {
                    win32_toggle_fullscreen(hwnd);
                }

                if ((GetKeyState(VK_MENU) & 0x8000) && w_param == VK_RETURN)
                {
                    win32_toggle_fullscreen(hwnd);
                }
            }

        } break;

        default: {} break;
    }

    return DefWindowProcW(hwnd, message, w_param, l_param);
}

function void
win32_fatal_assert(b32 cond, char *text) {
    if (!cond)
    {
        MessageBoxA(NULL, text, "Pix16: Fatal Error", MB_ICONERROR | MB_OK);
        os_exit(1);
    }
}

//
// Controllers
//
#include <xinput.h>

#define XINPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE *pState)
typedef XINPUT_GET_STATE(XInput_Get_State_t);

#define XINPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION *pVibration)
typedef XINPUT_SET_STATE(XInput_Set_State_t);

#define XINPUT_GET_CAPABILITIES(name) DWORD WINAPI name(DWORD dwUserIndex, DWORD dwFlags, XINPUT_CAPABILITIES *pCapabilities)
typedef XINPUT_GET_CAPABILITIES(XInput_Get_Capabilities_t);

XINPUT_GET_STATE(XInputGetStateStub) { return ERROR_DEVICE_NOT_CONNECTED; }
XINPUT_SET_STATE(XInputSetStateStub) { return ERROR_DEVICE_NOT_CONNECTED; }
XINPUT_GET_CAPABILITIES(XInputGetCapabilitiesStub) { return ERROR_DEVICE_NOT_CONNECTED; }

static XInput_Get_State_t *_XInputGetState = XInputGetStateStub;
static XInput_Set_State_t  *_XInputSetState = XInputSetStateStub;
static XInput_Get_Capabilities_t  *_XInputGetCapabilities = XInputGetCapabilitiesStub;

#define XInputGetState _XInputGetState
#define XInputSetState _XInputSetState
#define XInputGetCapabilities _XInputGetCapabilities

function b32 win32_init_xinput()
{
    HMODULE libxinput         = LoadLibraryA("xinput1_4.dll");
    if (!libxinput) libxinput = LoadLibraryA("xinput9_1_0.dll");
    if (!libxinput) libxinput = LoadLibraryA("xinput1_3.dll");

    if (!libxinput)
    {
        print("[xinput] Failed to load an xinput dll!\n");
        return false;
    }

    XInputGetState = (XInput_Get_State_t *)GetProcAddress(libxinput, "XInputGetState");
    if (!XInputGetState) { XInputGetState = XInputGetStateStub; }

    XInputSetState = (XInput_Set_State_t *)GetProcAddress(libxinput, "XInputSetState");
    if (!XInputSetState) { XInputSetState = XInputSetStateStub; }

    XInputGetCapabilities = (XInput_Get_Capabilities_t *)GetProcAddress(libxinput, "XInputGetCapabilities");
    if (!XInputGetCapabilities) { XInputGetCapabilities = XInputGetCapabilitiesStub; }

    return true;
}

function f32 process_xinput_stick_value(SHORT value, SHORT deadzone_threshold)
{
    f32 result = 0;

    if (value < -deadzone_threshold) {
        result = (f32)((value + deadzone_threshold) / (32768.0f - deadzone_threshold));
    } else if (value > deadzone_threshold) {
        result = (f32)((value - deadzone_threshold) / (32767.0f - deadzone_threshold));
    }

    return result;
}

function b32 process_xinput_button_value(DWORD button_state, DWORD button_bit)
{
    return (button_state & button_bit) == button_bit;
}

function void win32_poll_xinput_controllers(Game_Input *input)
{
    static u32 controller_connected_frame_index[4] = {0};

    for (u32 index = 0; index < 4; index++)
    {
        Controller *it = &input->controllers[index];

        XINPUT_STATE state;
        if (XInputGetState(index, &state) == ERROR_SUCCESS)
        {
            XINPUT_GAMEPAD *pad = &state.Gamepad;

            // NOTE(nick): vibrate when controller is connected
            static u32 vibrate_count = 5;
            if (controller_connected_frame_index[index] <= vibrate_count)
            {
                f32 left_motor = 0;
                f32 right_motor = controller_connected_frame_index[index] < vibrate_count ?  0.5 : 0;

                left_motor = Clamp(left_motor, 0, 1);
                right_motor = Clamp(right_motor, 0, 1);

                XINPUT_VIBRATION vibration = {};
                vibration.wLeftMotorSpeed  = (u16)(left_motor * U16_MAX);
                vibration.wRightMotorSpeed = (u16)(right_motor * U16_MAX);

                b32 success = XInputSetState(index, &vibration) == ERROR_SUCCESS;
            }

            it->up |= process_xinput_button_value(pad->wButtons, XINPUT_GAMEPAD_DPAD_UP);
            it->down |= process_xinput_button_value(pad->wButtons, XINPUT_GAMEPAD_DPAD_DOWN);
            it->left |= process_xinput_button_value(pad->wButtons, XINPUT_GAMEPAD_DPAD_LEFT);
            it->right |= process_xinput_button_value(pad->wButtons, XINPUT_GAMEPAD_DPAD_RIGHT);

            f32 stick_x = process_xinput_stick_value(pad->sThumbLX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
            f32 stick_y = process_xinput_stick_value(pad->sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);

            it->up |= stick_y > 0;
            it->down |= stick_y < 0;
            it->left |= stick_x < 0;
            it->right |= stick_x > 0;

            it->a |= process_xinput_button_value(pad->wButtons, XINPUT_GAMEPAD_A);
            it->b |= process_xinput_button_value(pad->wButtons, XINPUT_GAMEPAD_B);
            it->start |= process_xinput_button_value(pad->wButtons, XINPUT_GAMEPAD_START);
            it->pause |= process_xinput_button_value(pad->wButtons, XINPUT_GAMEPAD_BACK);

            controller_connected_frame_index[index] += 1;
        }
        else
        {
            controller_connected_frame_index[index] = 0;
        }
    }
}

//
// Main
//
int APIENTRY WinMain(HINSTANCE instance, HINSTANCE prev_inst, LPSTR argv, int argc)
{
    os_init();

    // NOTE(nick): Set DPI Awareness
    HMODULE user32 = LoadLibraryA("user32.dll");

    typedef BOOL Win32_SetProcessDpiAwarenessContext(HANDLE);
    typedef BOOL Win32_SetProcessDpiAwareness(int);

    Win32_SetProcessDpiAwarenessContext *SetProcessDpiAwarenessContext =
        (Win32_SetProcessDpiAwarenessContext *) GetProcAddress(user32, "SetProcessDpiAwarenessContext");
    Win32_SetProcessDpiAwareness *SetProcessDpiAwareness =
        (Win32_SetProcessDpiAwareness *) GetProcAddress(user32, "SetProcessDpiAwareness");

    if (SetProcessDpiAwarenessContext) {
        SetProcessDpiAwarenessContext(((HANDLE) -4) /* DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 */);
    } else if (SetProcessDpiAwareness) {
        SetProcessDpiAwareness(1 /* PROCESS_SYSTEM_DPI_AWARE */);
    } else {
        SetProcessDPIAware();
    }

    // NOTE(nick): set up global window class

    WNDCLASSEXW wc = {};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wc.lpfnWndProc   = (WNDPROC)win32_window_callback;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.hInstance     = instance;
    wc.hIcon         = LoadIconA(instance, "APPICON");
    wc.hCursor       = LoadCursorA(0, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszMenuName  = NULL;
    wc.lpszClassName = WINDOW_CLASS_NAME;
    wc.hIconSm       = NULL;

    win32_fatal_assert(RegisterClassExW(&wc), "Failed to register window class.");

    print("Hello, %S!\n", S("World"));

    M_Temp scratch = GetScratch(0, 0);

    u32 scale = 2;
    Vector2i size = v2i(scale * game_width, scale * game_height);
    String16 title16 = string16_from_string(scratch.arena, S("Pix16"));

    HWND hwnd = CreateWindowW(
        WINDOW_CLASS_NAME,
        (WCHAR *)title16.data, WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, size.x, size.y,
        0, 0, instance, 0
    );

    HDC hdc = GetDC(hwnd);

    int window_width = size.x;
    int window_height = size.y;

    // NOTE(nick): scale the window by DPI (if applicable)
    {
        f32 scale = (f32)GetDeviceCaps(hdc, LOGPIXELSX) / (f32)USER_DEFAULT_SCREEN_DPI;
        if (scale > 1)
        {
            RECT wr = {0, 0, (LONG)(size.x * scale), (LONG)(size.y * scale)};
            AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);
            window_width = (i32)(wr.right - wr.left);
            window_height = (i32)(wr.bottom - wr.top);

            SetWindowPos(hwnd, HWND_TOP, 0, 0, window_width, window_height, SWP_NOMOVE | SWP_NOOWNERZORDER);
        }
    }

    // NOTE(nick): center window
    {
        HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY);

        MONITORINFO info = {};
        info.cbSize = sizeof(MONITORINFO);
        GetMonitorInfo(monitor, &info);
        int monitor_width = info.rcMonitor.right - info.rcMonitor.left;
        int monitor_height = info.rcMonitor.bottom - info.rcMonitor.top;

        int center_x = (monitor_width - window_width) / 2;
        int center_y = (monitor_height - window_height) / 2;

        SetWindowPos(hwnd, HWND_TOP, center_x, center_y, window_width, window_height, SWP_NOOWNERZORDER);
    }

    win32_init_xinput();

    win32_audio_init();

    ShowWindow(hwnd, SW_SHOW);

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
            SetWindowTextA(hwnd, title_c);
        }

        // NOTE(nick): clamp large spikes in framerate
        if (dt > 0.25) dt = 0.25;

        // NOTE(nick): poll for events
        for (;;) {
            MSG msg = {};

            if (!PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
                break;
            }

            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        // NOTE(nick): reset temporary storage
        arena_reset(temp_arena());

        static Game_Input input = {};
        {
            static Arena *permanant_storage = NULL;
            if (!permanant_storage)
            {
                permanant_storage = arena_alloc(Megabytes(4));
            }

            input.arena = permanant_storage;
            input.dt = target_dt;
            input.time = os_time();

            //
            // Mouse
            //

            POINT point;
            if (GetCursorPos(&point))
            {
                if (ScreenToClient(hwnd, &point))
                {
                    RECT wr = {};
                    GetClientRect(hwnd, &wr);
                    int window_width = (int)(wr.right - wr.left);
                    int window_height = (int)(wr.bottom - wr.top);

                    Rectangle2i dest_rect = aspect_ratio_fit(game_width, game_height, window_width, window_height);

                    input.mouse.position = v2(
                        game_width * ((point.x - dest_rect.x0) / (f32)r2i_width(dest_rect)),
                        game_height * ((point.y - dest_rect.y0) / (f32)r2i_height(dest_rect)));
                }
            }
            input.mouse.left = ((GetKeyState(VK_LBUTTON) & 0x8000) != 0);
            input.mouse.right = ((GetKeyState(VK_RBUTTON) & 0x8000) != 0);


            //
            // Controllers
            //

            MemoryZero(&input.controllers, count_of(input.controllers) * sizeof(Controller));

            b32 window_is_focused = hwnd == GetForegroundWindow();
            if (window_is_focused)
            {
                win32_poll_xinput_controllers(&input);

                Controller *player0 = &input.controllers[0];

                player0->up    |= (GetKeyState(VK_UP) & (1 << 15)) == (1 << 15);
                player0->down  |= (GetKeyState(VK_DOWN) & (1 << 15)) == (1 << 15);
                player0->left  |= (GetKeyState(VK_LEFT) & (1 << 15)) == (1 << 15);
                player0->right |= (GetKeyState(VK_RIGHT) & (1 << 15)) == (1 << 15);
                player0->a     |= (GetKeyState('X') & (1 << 15)) == (1 << 15);
                player0->b     |= (GetKeyState('C') & (1 << 15)) == (1 << 15);

                player0->up    |= (GetKeyState('W') & (1 << 15)) == (1 << 15);
                player0->down  |= (GetKeyState('S') & (1 << 15)) == (1 << 15);
                player0->left  |= (GetKeyState('A') & (1 << 15)) == (1 << 15);
                player0->right |= (GetKeyState('D') & (1 << 15)) == (1 << 15);
                player0->a     |= (GetKeyState('J') & (1 << 15)) == (1 << 15);
                player0->b     |= (GetKeyState('K') & (1 << 15)) == (1 << 15);

                player0->start |= (GetKeyState(VK_ESCAPE) & (1 << 15)) == (1 << 15);
                player0->pause |= (GetKeyState('P') & (1 << 15)) == (1 << 15);
            }
        }

        static Game_Output output = {};
        output.pixels = win32_framebuffer.pixels;
        output.width  = win32_framebuffer.width;
        output.height = win32_framebuffer.height;

        u32 SamplesPerSecond = win32_audio.samples_per_second;
        u32 SampleCount = win32_audio.sample_count;
        u32 BufferCount = win32_audio.buffer_count;
        u32 BufferSize = win32_audio.buffer_size;
        u32 SamplesPerBuffer = win32_audio.samples_per_buffer;

        u32 CurrentOffset = win32_audio.queued_samples;
        u32 TargetOffset = win32_audio.written_samples + SamplesPerBuffer * BufferCount;

        i32 NumFrames = 0;
        if (CurrentOffset < TargetOffset)
        {
            NumFrames = (((i32)TargetOffset - (i32)CurrentOffset) / SamplesPerBuffer);
            if (NumFrames > 4) NumFrames = 4;
        }

        u32 UserSampleCount = NumFrames * SampleCount;
        i16 *UserSamples = win32_audio.user_samples;
        MemoryZero(UserSamples, UserSampleCount * 2 * sizeof(i16));
        
        output.samples_per_second = SamplesPerSecond;
        output.sample_count = UserSampleCount;
        output.samples = UserSamples;

        GameUpdateAndRender(&input, &output);

        output.samples_played += UserSampleCount;

        u32 UserSampleOffset = 0;
        while (CurrentOffset < TargetOffset)
        {
            u32 SampleOffset = win32_audio.queued_samples % (2 * BufferCount * SamplesPerBuffer);
            i16 *OutputSamples = (i16 *)((u8 *)(win32_audio.samples) + SampleOffset * 2 * sizeof(i16));

            MemoryCopy(OutputSamples, (u8 *)(UserSamples) + UserSampleOffset, SampleCount * 2 * sizeof(i16));
            UserSampleOffset += SampleCount * 2 * sizeof(i16);

            win32_audio.queued_samples += SampleCount;
            CurrentOffset = win32_audio.queued_samples;

            NumFrames -= 1;
            if (NumFrames <= 0) break;
        }

        //
        // NOTE(nick): present framebuffer
        //
        {
            RECT wr = {0};
            GetClientRect(hwnd, &wr);
            int window_width = (int)(wr.right - wr.left);
            int window_height = (int)(wr.bottom - wr.top);

            Win32_Framebuffer *framebuffer = &win32_framebuffer;

            Rectangle2i dest_rect = aspect_ratio_fit(game_width, game_height, window_width, window_height);

            StretchDIBits(hdc,
                dest_rect.x0, dest_rect.y0, r2i_width(dest_rect), r2i_height(dest_rect),
                0, 0, framebuffer->width, framebuffer->height, framebuffer->pixels,
                &framebuffer->bitmap_info,
                DIB_RGB_COLORS, SRCCOPY);
        }

        now = os_time();
        f64 remaining_seconds = target_dt - (now - then);

        // NOTE(nick): wait for next frame
        b32 window_is_focused = (GetForegroundWindow() == hwnd);
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

    os_exit(0);

    return 0;
}
