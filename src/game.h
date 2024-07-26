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

    Font_Glyph glyphs[128];
    u32 glyph_count;
};

//
// API
//

void GameInit();
void GameSetState(Game_Input *input, Game_Output *out);
void GameUpdateAndRender(Game_Input *input, Game_Output *out);

//
// Drawing API
//

void DrawSetPixel(Vector2 pos, Vector4 color);
u32 DrawGetPixel(Vector2 pos);

void DrawRect(Rectangle2 rect, Vector4 color);
void DrawRectExt(Rectangle2 rect, Vector4 c0, Vector4 c1, Vector4 c2, Vector4 c3);

void DrawCircle(Vector2 pos, f32 radius, Vector4 color);

void DrawTriangle(Vector2 p0, Vector2 p1, Vector2 p2, Vector4 color);
void DrawTriangleExt(Vector2 p0, Vector4 c0, Vector2 p1, Vector4 c1, Vector2 p2, Vector4 c2);

void DrawLine(Vector2 p0, Vector2 p1, Vector4 color);

void DrawImage(Image image, Vector2 pos);
void DrawImageExt(Image image, Rectangle2 rect, Rectangle2 uv);

void DrawSpriteExt(Image src, Vector2i src_position, Vector2i src_size, Vector2i dest_position);

void DrawText(Font font, String text, Vector2 pos);
void DrawTextExt(Font font, String text, Vector2 pos, Vector4 color);

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
Font LoadFont(String path);

Font FontMake(Image image, String alphabet, Vector2i monospaced_letter_size);
