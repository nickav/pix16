#pragma once

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

    Font_Glyph glyphs[256];
    u32 glyph_count;
};

//
// API
//

void GameUpdateAndRender(Game_Input *input, Game_Output *out);

//
// Drawing API
//

void DrawSetPixel(Game_Output *out, Vector2 pos, Vector4 color);
u32 DrawGetPixel(Game_Output *out, Vector2 pos);

void DrawRect(Game_Output *out, Rectangle2 rect, Vector4 color);
void DrawRectExt(Game_Output *out, Rectangle2 rect, Vector4 c0, Vector4 c1, Vector4 c2, Vector4 c3);

void DrawCircle(Game_Output *out, Vector2 pos, f32 radius, Vector4 color);

void DrawTriangle(Game_Output *out, Vector2 p0, Vector2 p1, Vector2 p2, Vector4 color);
void DrawTriangleExt(Game_Output *out, Vector2 p0, Vector4 c0, Vector2 p1, Vector4 c1, Vector2 p2, Vector4 c2);

void DrawLine(Game_Output *out, Vector2 p0, Vector2 p1, Vector4 color);

void DrawImage(Game_Output *out, Image image, Vector2 pos);
void DrawImageExt(Game_Output *out, Image image, Rectangle2 rect, Vector4 color, Rectangle2 uv);

void DrawText(Game_Output *out, Font font, String text, Vector2 pos);
void DrawTextExt(Game_Output *out, Font font, String text, Vector2 pos, Vector4 color);

//
// Sound API
//

void PlaySine(Game_Output *out, f32 tone_hz, f32 volume);
void PlaySquare(Game_Output *out, f32 tone_hz, f32 volume);
void PlayTriangle(Game_Output *out, f32 tone_hz, f32 volume);
void PlaySawtooth(Game_Output *out, f32 tone_hz, f32 volume);
void PlayNoise(Game_Output *out, f32 volume);

void PlaySoundStream(Game_Output *out, Sound sound, f32 volume);
void SoundSeek(Game_Output *out, Sound sound, f32 time_in_seconds);
f32 SoundGetTime(Game_Output *out, Sound sound);

//
// Assets API
//

Image LoadImage(String path);
Sound LoadSound(String path);
Font LoadFont(String path);

Font FontMake(Image image, String alphabet, Vector2i monospaced_letter_size);
