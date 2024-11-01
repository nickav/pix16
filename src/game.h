#pragma once

const Vector2 TextAlign_Center      = v2(0.5, 0.5);

const Vector2 TextAlign_TopLeft     = v2(0, 0);
const Vector2 TextAlign_BottomLeft  = v2(0, 1);
const Vector2 TextAlign_CenterLeft  = v2(0, 0.5);

const Vector2 TextAlign_TopRight    = v2(1, 0);
const Vector2 TextAlign_BottomRight = v2(1, 1);
const Vector2 TextAlign_CenterRight = v2(1, 0.5);

typedef u32 Controller_Button;
enum {
    Button_Up = 0,
    Button_Down,
    Button_Left,
    Button_Right,

    Button_A,
    Button_B,
    Button_Start,
    Button_Back,

    Button_COUNT,
};

typedef u32 Mouse_Button;
enum {
    Mouse_Left = 0,
    Mouse_Right = 1,
    Mouse_COUNT,
};

struct Controller
{
    b32 up;
    b32 down;
    b32 left;
    b32 right;

    b32 a;
    b32 b;
    b32 start;
    b32 pause;

    f32 stick_x;
    f32 stick_y;
};

struct Mouse
{
    b32 left;
    b32 right;

    Vector2 position;
};

struct Game_Input
{
    // User Memory
    Arena *arena;

    // Time
    f32 dt;
    f32 time;

    // Inputs
    Mouse mouse;
    Controller controllers[4];
};

struct Game_Output
{
    // Screen Pixels
    i32 width;
    i32 height;
    u32 *pixels;

    // Audio
    i32 samples_per_second;
    i32 sample_count;
    i16 *samples;
    i32 samples_played;
};

struct Image
{
    Vector2i size;
    u32 *pixels;
    i64 index;
};

struct Sound
{
    u16 bits_per_sample; // should always be 32
    u16 num_channels; // should always be 2
    u16 sample_rate; // should always be 44100

    u32 total_samples;
    i16 *samples;
    i64 index;
};

struct Font_Glyph
{
    u32 character;
    Vector2i pos;
    Vector2i size;

    Vector2i line_offset;
    i32 xadvance;
};

struct Font
{
    Image image;

    Font_Glyph *glyphs;
    u32 glyph_count;
};

//
// API
//

void GameInit();
void GameSetState(Game_Input *input, Game_Output *out, Game_Input *prev_input);
void GameUpdateAndRender(Game_Input *input, Game_Output *out);

//
// Controller API
//

b32 ControllerPressed(int controller_index, Controller_Button button);
b32 ControllerDown(int index, Controller_Button button);
b32 ControllerReleased(int index, Controller_Button button);

Vector2 MousePosition();
b32 MousePressed(Mouse_Button button);
b32 MouseReleased(Mouse_Button button);
b32 MouseDown(Mouse_Button button);

//
// Drawing API
//

void DrawSetPixel(Vector2 pos, Vector4 color);
u32 DrawGetPixel(Vector2 pos);

void DrawRect(Rectangle2 rect, Vector4 color);
void DrawRectExt(Rectangle2 rect, Vector4 c0, Vector4 c1, Vector4 c2, Vector4 c3);
void DrawRectOutline(Rectangle2 rect, Vector4 color, int thickness);

void DrawCircle(Vector2 pos, f32 radius, Vector4 color);

void DrawTriangle(Vector2 p0, Vector2 p1, Vector2 p2, Vector4 color);
void DrawTriangleExt(Vector2 p0, Vector4 c0, Vector2 p1, Vector4 c1, Vector2 p2, Vector4 c2);

void DrawLine(Vector2 p0, Vector2 p1, Vector4 color);

void DrawImage(Image image, Vector2 pos);
void DrawImageExt(Image image, Rectangle2 rect, Rectangle2 uv);
void DrawImageMirrored(Image image, Vector2 pos, b32 flip_x, b32 flip_y);

Vector2 MeasureText(Font font, String text);
void DrawText(Font font, String text, Vector2 pos);
void DrawTextAlign(Font font, String text, Vector2 pos, Vector2 anchor);
void DrawTextExt(Font font, String text, Vector2 pos, Vector4 color, Vector2 anchor, f32 scale);

void DrawClear(Vector4 color);

//
// Sound API
//

void PlaySine(f32 tone_hz, f32 volume);
void PlaySquare(f32 tone_hz, f32 volume);
void PlayTriangle(f32 tone_hz, f32 volume);
void PlaySawtooth(f32 tone_hz, f32 volume);
void PlayNoise(f32 volume);

void PlaySoundStream(Sound sound, f32 volume);
void SoundSeek(Sound sound, f32 time_in_seconds);
f32 SoundGetTime(Sound sound);

//
// Assets API
//

Image LoadImage(String path);
Sound LoadSound(String path);
Font LoadFont(String path, String alphabet, Vector2i monospaced_letter_size);
Font LoadFontExt(String path, Font_Glyph *glyphs, u64 glyph_count);
