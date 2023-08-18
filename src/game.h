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

struct Image
{
    Vector2i size;
    u32 *pixels;
    
    String name;
    u64 hash;
};

struct Sound
{
    u16 bits_per_sample; // should always be 32
    u16 num_channels; // should always be 2
    u16 sample_rate; // should always be 48000

    u32 total_samples;
    i16 *samples;

    u32 sample_offset; // samples played so far

    String name;
    u64 hash;
};

struct Game_Input
{
    Arena *arena;
    f32 dt;
    f32 time;

    Mouse mouse;
    Controller controllers[4];

    Image images[1024];
    Sound sounds[1024];
};

struct Game_Output
{
    i32 width;
    i32 height;
    u32 *pixels;

    i32 samples_per_second;
    i32 sample_count;
    i16 *samples;
    i32 samples_played;
};

struct Font_Glyph
{
    u32 character;
    Vector2i pos;
    Vector2i size;
};

struct Font
{
    Image image;
    Font_Glyph glyphs[256];
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

void DrawRect(Game_Output *out, Rectangle2 r, Vector4 color);
void DrawRectExt(Game_Output *out, Rectangle2 r, Vector4 c0, Vector4 c1, Vector4 c2, Vector4 c3);

void DrawCircle(Game_Output *out, Vector2 pos, f32 radius, Vector4 color);

void DrawTriangle(Game_Output *out, Vector2 p0, Vector2 p1, Vector2 p2, Vector4 color);
void DrawTriangleExt(Game_Output *out, Vector2 p0, Vector4 c0, Vector2 p1, Vector4 c1, Vector2 p2, Vector4 c2);

void DrawImage(Game_Output *out, Image image, Vector2 pos);
void DrawImageExt(Game_Output *out, Image image, Rectangle2 r, Rectangle2 uv);

void DrawLine(Game_Output *out, Vector2 p0, Vector2 p1, Vector4 color);

void DrawText(Game_Output *out, Font *font, String text, Vector2 pos, Vector4 color);

//
// Sound API
//

void PlaySine(Game_Output *out, f32 tone_hz, f32 volume);
void PlayTriangle(Game_Output *out, f32 tone_hz, f32 volume);
void PlaySquare(Game_Output *out, f32 tone_hz, f32 volume);
void PlayNoise(Game_Output *out, f32 tone_hz, f32 volume);

void PlaySoundStream(Game_Output *out, Sound *sound);

//
// Assets API
//

Image LoadImage(Game_Input *input, String path);
void FreeImage(Game_Input *input, String path);

Sound *LoadSound(Game_Input *input, String path);
void FreeSound(Game_Input *input, String path);