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

struct Image
{
    String name;
    Vector2i size;
    u32 *pixels;
};

struct Game_Input
{
    f32 dt;
    f32 time;

    Controller controllers[4];

    Image images[256];
};

struct Game_Output
{
    i32 width;
    i32 height;
    u32 *pixels;

    i32 samples_played;
    i32 samples_per_second;
    i32 sample_count;
    i16 *samples;
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

struct Sound
{
    u16 bits_per_sample; // should always be 32
    u16 num_channels; // should always be 2
    u16 sample_rate; // should always be 48000
    u32 bytes_per_second; // computed: sample_rate * (bits_per_sample / 8) * num_channels

    u32 total_samples;
    i16 *samples;

    u32 sample_offset; // samples played so far
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

void DrawCircle(Game_Output *out, Rectangle2 r, Vector4 color);

void DrawLine(Game_Output *out, Vector2 p0, Vector2 p1, Vector4 color);

void DrawTriangle(Game_Output *out, Vector2 p0, Vector2 p1, Vector2 p2, Vector4 color);

void DrawImage(Game_Output *out, Rectangle2 r, Image image);
void DrawImageExt(Game_Output *out, Rectangle2 r, Image image, Rectangle2 uv);

void DrawText(Game_Output *out, Font *font, String text, Vector2 pos, Vector4 color);

//
// Sound API
//

void PlaySine(Game_Output *out, f32 tone_hz, f32 volume);
void PlayTriangle(Game_Output *out, f32 tone_hz, f32 volume);
void PlaySquare(Game_Output *out, f32 tone_hz, f32 volume);
void PlayNoise(Game_Output *out, f32 tone_hz, f32 volume);

void PlaySound(Game_Output *out, Sound *sound);