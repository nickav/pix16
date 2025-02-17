#pragma once

#define STB_IMAGE_IMPLEMENTATION
#include "third_party/stb_image.h"

#define DR_WAV_IMPLEMENTATION
#include "third_party/dr_wav.h"

struct Asset_Info
{
    String name;
    i64 index;
    u64 hash;
};

struct Image_Asset
{
    Asset_Info info;
    Image image;
};

struct Sound_Asset
{
    Asset_Info info;
    Sound sound;
};

struct Font_Asset
{
    Asset_Info info;
    Font font;
};

struct Playing_Sound
{
    Sound sound;
    f32 volume;
    u32 sample_offset;
};

struct Playing_Sound_Array
{
    Playing_Sound *data;
    i64 capacity;
    i64 count;
};

struct Game_State
{
    Image_Asset images[1024];
    Sound_Asset sounds[1024];
    Font_Asset  fonts[1024];

    Random_PCG rng;

    Arena *arena;
    String data_path;

    // Mixer
    Playing_Sound_Array playing_sounds;
    f32 master_volume;
};

static Game_State g_state = {0};
static Game_Input *input = NULL;
static Game_Output *out  = NULL;
static Game_Input *prev_input = NULL;

void GameInit()
{
    Arena *arena = arena_alloc(Gigabytes(1));

    MemoryZero(&g_state, sizeof(Game_State));
    g_state.rng = {0x6908243098231};

    g_state.arena = arena;

    String data_path = os_get_executable_path();
    if (!os_file_exists(path_join(data_path, S("data"))))
    {
        data_path = path_join(path_dirname(data_path), S("data"));
    }
    g_state.data_path = string_push(arena, data_path);

    g_state.playing_sounds.capacity = 256;
    g_state.playing_sounds.count = 0;
    g_state.playing_sounds.data = PushArrayZero(arena, Playing_Sound, g_state.playing_sounds.capacity);

    g_state.master_volume = 1.0;
}

void GameSetState(Game_Input *the_input, Game_Output *the_output, Game_Input *the_prev_input)
{
    input = the_input;
    out = the_output;
    prev_input = the_prev_input;
}

//
// Assets API
//

Asset_Info *FindAssetByHash(void *array, u64 size, u64 count, u64 hash)
{
    Asset_Info *result = NULL;

    if (hash == 0) hash += 1;

    u8 *at = (u8 *)array;
    for (i64 index = 0; index < count; index += 1)
    {
        Asset_Info *it = (Asset_Info *)at;

        if (it->hash == hash)
        {
            result = it;
            result->index = index;
            break;
        }

        at += size;
    }

    return result;
}

Asset_Info *FindFreeAsset(void *array, u64 size, u64 count)
{
    Asset_Info *result = NULL;

    u8 *at = (u8 *)array;
    for (i64 index = 0; index < count; index += 1)
    {
        Asset_Info *it = (Asset_Info *)at;

        if (it->hash == 0)
        {
            result = it;
            result->index = index;
            break;
        }

        at += size;
    }

    return result;
}

Asset_Info *GetAssetByIndex(void *array, u64 size, u64 count, i64 index)
{
    Asset_Info *result = NULL;
    if (index >= 0 && index < count)
    {
        result = (Asset_Info *)((u8 *)(array) + size * index);
    }
    return result;
}

Image LoadImage(String path)
{
    u64 hash = fnv64a(path.data, path.count);

    Image_Asset *result = (Image_Asset *)FindAssetByHash(&g_state.images, sizeof(Image_Asset), count_of(g_state.images), hash);

    if (!result)
    {
        result = (Image_Asset *)FindFreeAsset(&g_state.images, sizeof(Image_Asset), count_of(g_state.images));

        if (!result)
        {
            print("[LoadImage] Used all %d slots available! Failed to load image: %.*s\n", count_of(g_state.images), LIT(path));
        }
    }

    if (result)
    {
        if (result->info.hash == 0)
        {
            M_Temp scratch = GetScratch(0, 0);

            String contents = os_read_entire_file(scratch.arena, path_join(g_state.data_path, path));

            if (contents.count > 0)
            {
                int width, height, channels;
                result->image.pixels      = (u32 *)stbi_load_from_memory(contents.data, contents.count, &width, &height, &channels, 4);
                result->image.size.width  = width;
                result->image.size.height = height;
                result->image.index = result->info.index;
            }
            else
            {
                print("[LoadImage] Image not found: %.*s\n", LIT(path));
            }

            result->info.name = path;
            result->info.hash = hash;

            ReleaseScratch(scratch);
        }

        return result->image;
    }

    return {};
}

Sound LoadSound(String path)
{
    u64 hash = murmur64(path.data, path.count);

    Sound_Asset *result = (Sound_Asset *)FindAssetByHash(&g_state.sounds, sizeof(Sound_Asset), count_of(g_state.sounds), hash);

    if (!result)
    {
        result = (Sound_Asset *)FindFreeAsset(&g_state.sounds, sizeof(Sound_Asset), count_of(g_state.sounds));

        if (!result)
        {
            print("[LoadSound] Used all %d slots available! Failed to load sound: %.*s\n", count_of(g_state.sounds), LIT(path));
        }
    }

    if (result)
    {
        if (result->info.hash == 0)
        {
            M_Temp scratch = GetScratch(0, 0);

            String contents = os_read_entire_file(scratch.arena, path_join(g_state.data_path, path));

            if (contents.count > 0)
            {
                unsigned int channels;
                unsigned int sample_rate;
                drwav_uint64 total_pcm_frame_count;
                i16 *samples = drwav_open_memory_and_read_pcm_frames_s16(contents.data, contents.count, &channels, &sample_rate, &total_pcm_frame_count, NULL);

                result->sound.bits_per_sample = 32;
                result->sound.num_channels = channels;
                result->sound.sample_rate = sample_rate;
                result->sound.total_samples = total_pcm_frame_count;
                result->sound.samples = samples;
                result->sound.index = result->info.index;

                assert(channels == 2);
                assert(sample_rate == 44100);
            }
            else
            {
                print("[LoadSound] Sound not found: %.*s\n", LIT(path));
            }

            result->info.name = path;
            result->info.hash = hash;

            ReleaseScratch(scratch);
        }

        return result->sound;
    }

    return {};
}

Font FontMakeFromImageMono(Image image, String alphabet, Vector2i monospaced_letter_size)
{
    Font result = {0};

    Vector2i cursor = {0, 0};

    String32 unicode_alphabet = string32_from_string(temp_arena(), alphabet);
    u32 glyph_count = unicode_alphabet.count;

    result.image  = image;
    result.glyphs = PushArrayZero(g_state.arena, Font_Glyph, glyph_count+1);
    result.glyph_count = 0;

    Font_Glyph *null_glyph = &result.glyphs[0];
    null_glyph->character = 0;
    null_glyph->size = v2i(monospaced_letter_size.x, 0);
    result.glyph_count += 1;

    for (int index = 0; index < glyph_count; index += 1)
    {
        u32 character = unicode_alphabet.data[index];

        Font_Glyph *glyph = &result.glyphs[result.glyph_count];
        result.glyph_count += 1;

        glyph->character = character;
        glyph->pos = cursor;
        glyph->size = monospaced_letter_size;
        glyph->line_offset = v2i(0, 0);
        glyph->xadvance = monospaced_letter_size.x;

        cursor.x += monospaced_letter_size.x;
        if (cursor.x >= image.size.width)
        {
            cursor.x = 0;
            cursor.y += monospaced_letter_size.y;
        }
    }

    return result;
}

Font FontMake(Image image, Font_Glyph *glyphs, u64 glyph_count)
{
    Font result = {0};
    result.image = image;
    result.glyphs = glyphs;
    result.glyph_count = glyph_count;
    return result;
}

Font LoadFont(String path, String alphabet, Vector2i monospaced_letter_size)
{
    Font result = {0};

    u64 hash = murmur64(path.data, path.count);
    Font_Asset *asset = (Font_Asset *)FindAssetByHash(&g_state.fonts, sizeof(Font_Asset), count_of(g_state.fonts), hash);

    if (!asset)
    {
        asset = (Font_Asset *)FindFreeAsset(&g_state.fonts, sizeof(Font_Asset), count_of(g_state.fonts));

        if (!asset)
        {
            print("[LoadFont] Used all %d slots available! Failed to load font: %.*s\n", count_of(g_state.fonts), LIT(path));
        }
    }

    if (asset)
    {
        if (asset->info.hash == 0)
        {
            Image image = LoadImage(path);
            if (image.size.x > 0 && image.size.y > 0)
            {
                asset->font = FontMakeFromImageMono(image, alphabet, monospaced_letter_size);
                asset->info.name = path;
                asset->info.hash = hash;
            }
        }

        result = asset->font;
    }

    return result;
}

Font LoadFontExt(String path, Font_Glyph *glyphs, u64 glyph_count)
{
    Image image = LoadImage(path);
    return FontMake(image, glyphs, glyph_count);
}

//
// Controller API
//

b32 ControllerPressed(int index, Controller_Button button)
{
    b32 result = false;
    if (index >= 0 && index < count_of(input->controllers))
    {
        if (button >= 0 && button < Button_COUNT)
        {
            b32 *prev_state = (b32 *)&prev_input->controllers[index];
            b32 *state = (b32 *)&input->controllers[index];

            result = !prev_state[button] && state[button];
        }
    }
    return result;
}

b32 ControllerDown(int index, Controller_Button button)
{
    b32 result = false;
    if (index >= 0 && index < count_of(input->controllers))
    {
        if (button >= 0 && button < Button_COUNT)
        {
            b32 *prev_state = (b32 *)&prev_input->controllers[index];
            b32 *state = (b32 *)&input->controllers[index];

            result = state[button];
        }
    }
    return result;
}

b32 ControllerReleased(int index, Controller_Button button)
{
    b32 result = false;
    if (index >= 0 && index < count_of(input->controllers))
    {
        if (button >= 0 && button < Button_COUNT)
        {
            b32 *prev_state = (b32 *)&prev_input->controllers[index];
            b32 *state = (b32 *)&input->controllers[index];

            result = prev_state[button] && !state[button];
        }
    }
    return result;
}

Vector2 MousePosition()
{
    return input->mouse.position;
}

b32 MousePressed(Mouse_Button button)
{
    b32 result = false;
    if (button >= 0 && button < Mouse_COUNT)
    {
        b32 *prev_state = (b32 *)&prev_input->mouse;
        b32 *state = (b32 *)&input->mouse;
        result = !prev_state[button] && state[button];
    }
    return result;
}

b32 MouseDown(Mouse_Button button)
{
    b32 result = false;
    if (button >= 0 && button < Mouse_COUNT)
    {
        b32 *prev_state = (b32 *)&prev_input->mouse;
        b32 *state = (b32 *)&input->mouse;
        result = state[button];
    }
    return result;
}

b32 MouseReleased(Mouse_Button button)
{
    b32 result = false;
    if (button >= 0 && button < Mouse_COUNT)
    {
        b32 *prev_state = (b32 *)&prev_input->mouse;
        b32 *state = (b32 *)&input->mouse;
        result = prev_state[button] && !state[button];
    }
    return result;
}

//
// Drawing API
//

void DrawSetPixel(Vector2 pos, Vector4 color)
{
    i32 x = (i32)pos.x;
    i32 y = (i32)pos.y;

    if (x >= 0 && x < out->width && y >= 0 && y < out->height)
    {
        u32 out_color = u32_rgba_from_v4(color);
        u32 *at = &out->pixels[y * out->width + x];
        *at = out_color;
    }
}

u32 DrawGetPixel(Vector2 pos)
{
    u32 result = 0;

    i32 x = (i32)pos.x;
    i32 y = (i32)pos.y;

    if (x >= 0 && x < out->width && y >= 0 && y < out->height)
    {
        u32 *at = &out->pixels[y * out->width + x];
        result = *at;
    }

    return result;
}

typedef u32 OutCode;
enum
{
    OutCode_LEFT   = 1,
    OutCode_RIGHT  = 2,
    OutCode_BOTTOM = 4,
    OutCode_TOP    = 8,
};

OutCode computeOutCode(i32 x, i32 y, i32 max_x, i32 max_y)
{
    int code = 0;
    if (x < 0) code |= OutCode_LEFT;
    else if (x > max_x) code |= OutCode_RIGHT;
    if (y < 0) code |= OutCode_BOTTOM;
    else if (y > max_y) code |= OutCode_TOP;
    return code;
}

b32 cohenSutherlandClip(i32 *in_x0, i32 *in_y0, i32 *in_x1, i32 *in_y1, i32 max_x, i32 max_y) {
    i32 x0 = *in_x0;
    i32 y0 = *in_y0;
    i32 x1 = *in_x1;
    i32 y1 = *in_y1;

    int outcode0 = computeOutCode(x0, y0, max_x, max_y);
    int outcode1 = computeOutCode(x1, y1, max_x, max_y);
    b32 accept = 0;

    while (1) {
        if (!(outcode0 | outcode1)) {
            // Both endpoints are inside the rectangle
            accept = 1;
            break;
        } else if (outcode0 & outcode1) {
            // Both endpoints share an outside zone (logical AND is not 0)
            break;
        } else {
            // At least one endpoint is outside the rectangle
            int outcodeOut = outcode0 ? outcode0 : outcode1;
            int x, y;

            if (outcodeOut & OutCode_TOP) {           // Point is above the rectangle
                x = x0 + (x1 - x0) * (max_y - y0) / (y1 - y0);
                y = max_y;
            } else if (outcodeOut & OutCode_BOTTOM) { // Point is below the rectangle
                x = x0 + (x1 - x0) * (0 - y0) / (y1 - y0);
                y = 0;
            } else if (outcodeOut & OutCode_RIGHT) {  // Point is to the right of the rectangle
                y = y0 + (y1 - y0) * (max_x - x0) / (x1 - x0);
                x = max_x;
            } else if (outcodeOut & OutCode_LEFT) {   // Point is to the left of the rectangle
                y = y0 + (y1 - y0) * (0 - x0) / (x1 - x0);
                x = 0;
            }

            if (outcodeOut == outcode0) {
                x0 = x;
                y0 = y;
                outcode0 = computeOutCode(x0, y0, max_x, max_y);
            } else {
                x1 = x;
                y1 = y;
                outcode1 = computeOutCode(x1, y1, max_x, max_y);
            }
        }
    }

    *in_x0 = x0;
    *in_y0 = y0;
    *in_x1 = x1;
    *in_y1 = y1;

    return accept;
}

void DrawRect(Rectangle2 rect, Vector4 color)
{
    // TimeFunction;

    rect = abs_r2(rect);

    i32 in_x0 = Clamp((i32)rect.x0, 0, out->width);
    i32 in_x1 = Clamp((i32)rect.x1, 0, out->width);

    i32 in_y0 = Clamp((i32)rect.y0, 0, out->height);
    i32 in_y1 = Clamp((i32)rect.y1, 0, out->height);

    u32 out_color = u32_rgba_from_v4(color);

    u32 *at = &out->pixels[in_y0 * out->width + in_x0];

    for (i32 y = in_y0; y < in_y1; y += 1)
    {
        for (i32 x = in_x0; x < in_x1; x += 1)
        {
            //pixels[y * out->width + x] = out_color;
            *at = out_color;
            at += 1;
        }

        at += out->width - (in_x1 - in_x0);
    }
}

/*
c0 ----- c1
|        |
|        |
|        |
c2 ----- c3
*/
void DrawRectExt(Rectangle2 rect, Vector4 c0, Vector4 c1, Vector4 c2, Vector4 c3)
{
    rect = abs_r2(rect);

    i32 in_x0 = Clamp((i32)rect.x0, 0, out->width);
    i32 in_x1 = Clamp((i32)rect.x1, 0, out->width);

    i32 in_y0 = Clamp((i32)rect.y0, 0, out->height);
    i32 in_y1 = Clamp((i32)rect.y1, 0, out->height);

    u32 *at = &out->pixels[in_y0 * out->width + in_x0];

    for (i32 y = in_y0; y < in_y1; y += 1)
    {
        for (i32 x = in_x0; x < in_x1; x += 1)
        {
            f32 u = (x - in_x0) / (f32)(in_x1 - in_x0);
            f32 v = (y - in_y0) / (f32)(in_y1 - in_y0);

            Vector4 sample = lerp_v4(lerp_v4(c0, c2, v), lerp_v4(c1, c3, v), u);

            u32 sample_color = u32_rgba_from_v4(sample);

            *at = sample_color;
            at += 1;
        }

        at += out->width - (in_x1 - in_x0);
    }
}

void DrawRectOutline(Rectangle2 rect, Vector4 color, int thickness)
{
    i32 in_x0 = Clamp((i32)rect.x0, 0, out->width);
    i32 in_y0 = Clamp((i32)rect.y0, 0, out->height);
    i32 in_x1 = Clamp((i32)rect.x1, 0, out->width);
    i32 in_y1 = Clamp((i32)rect.y1, 0, out->height);

    // top
    DrawRect(r2_from_f32(in_x0, in_y0, in_x1, in_y0+thickness), color);

    // bottom
    DrawRect(r2_from_f32(in_x0, in_y1, in_x1, in_y1-thickness), color);

    // left
    DrawRect(r2_from_f32(in_x0, in_y0, in_x0+thickness, in_y1), color);

    // right
    DrawRect(r2_from_f32(in_x1, in_y0, in_x1-thickness, in_y1), color);
}

void DrawCircle(Vector2 pos, f32 radius, Vector4 color)
{
    i32 in_x0 = Clamp((i32)pos.x - radius, 0, out->width);
    i32 in_x1 = Clamp((i32)pos.x + radius, 0, out->width);

    i32 in_y0 = Clamp((i32)pos.y - radius, 0, out->height);
    i32 in_y1 = Clamp((i32)pos.y + radius, 0, out->height);

    u32 out_color = u32_rgba_from_v4(color);
    u32 *at = &out->pixels[in_y0 * out->width + in_x0];

    for (i32 y = in_y0; y < in_y1; y += 1)
    {
        for (i32 x = in_x0; x < in_x1; x += 1)
        {
            f32 cx = 2 * ((x - in_x0) / (f32)(in_x1 - in_x0)) - 1;
            f32 cy = 2 * ((y - in_y0) / (f32)(in_y1 - in_y0)) - 1;

            if (cx * cx + cy * cy < 1)
            {
                *at = out_color;
            }

            //pixels[y * out->width + x] = out_color;
            at += 1;
        }

        at += out->width - (in_x1 - in_x0);
    }
}

void DrawTriangle(Vector2 p0, Vector2 p1, Vector2 p2, Vector4 color)
{
    f32 min_x = min_f32(p0.x, min_f32(p1.x, p2.x));
    f32 max_x = max_f32(p0.x, max_f32(p1.x, p2.x));

    f32 min_y = min_f32(p0.y, min_f32(p1.y, p2.y));
    f32 max_y = max_f32(p0.y, max_f32(p1.y, p2.y));

    Rectangle2 rect = r2(v2(min_x, min_y), v2(max_x, max_y));

    i32 in_x0 = Clamp((i32)rect.x0, 0, out->width);
    i32 in_x1 = Clamp((i32)rect.x1, 0, out->width);

    i32 in_y0 = Clamp((i32)rect.y0, 0, out->height);
    i32 in_y1 = Clamp((i32)rect.y1, 0, out->height);

    u32 out_color = u32_rgba_from_v4(color);

    u32 *at = &out->pixels[in_y0 * out->width + in_x0];

    for (i32 y = in_y0; y < in_y1; y += 1)
    {
        for (i32 x = in_x0; x < in_x1; x += 1)
        {
            if (point_in_triangle(v2(x, y), p0, p1, p2))
            {
                *at = out_color;
            }
            //pixels[y * out->width + x] = out_color;
            at += 1;
        }

        at += out->width - (in_x1 - in_x0);
    }
}

void DrawTriangleExt(Vector2 p0, Vector4 c0, Vector2 p1, Vector4 c1, Vector2 p2, Vector4 c2)
{
    f32 min_x = min_f32(p0.x, min_f32(p1.x, p2.x));
    f32 max_x = max_f32(p0.x, max_f32(p1.x, p2.x));

    f32 min_y = min_f32(p0.y, min_f32(p1.y, p2.y));
    f32 max_y = max_f32(p0.y, max_f32(p1.y, p2.y));

    Rectangle2 rect = r2(v2(min_x, min_y), v2(max_x, max_y));

    i32 in_x0 = Clamp((i32)rect.x0, 0, out->width);
    i32 in_x1 = Clamp((i32)rect.x1, 0, out->width);

    i32 in_y0 = Clamp((i32)rect.y0, 0, out->height);
    i32 in_y1 = Clamp((i32)rect.y1, 0, out->height);

    Vector2 center = r2_center(rect);

    u32 *at = &out->pixels[in_y0 * out->width + in_x0];

    for (i32 y = in_y0; y < in_y1; y += 1)
    {
        for (i32 x = in_x0; x < in_x1; x += 1)
        {
            Vector2 point = v2(x, y);
            if (point_in_triangle(point, p0, p1, p2))
            {
                // barycentric coordinates
                Vector2 v0 = p1 - p0;
                Vector2 v1 = p2 - p0;
                Vector2 v2 = point - p0;
                f32 d00 = v2_dot(v0, v0);
                f32 d01 = v2_dot(v0, v1);
                f32 d11 = v2_dot(v1, v1);
                f32 d20 = v2_dot(v2, v0);
                f32 d21 = v2_dot(v2, v1);
                f32 denom = d00 * d11 - d01 * d01;

                f32 v = (d11 * d20 - d01 * d21) / denom;
                f32 w = (d00 * d21 - d01 * d20) / denom;
                f32 u = 1.0f - v - w;

                Vector4 sample = c0 * u + c1 * v + c2 * w;
                u32 sample_color = u32_rgba_from_v4(sample);

                *at = sample_color;
            }
            
            at += 1;
        }

        at += out->width - (in_x1 - in_x0);
    }
}

void DrawLine(Vector2 p0, Vector2 p1, Vector4 color)
{
    i32 x0 = (i32)p0.x;
    i32 y0 = (i32)p0.y;
    i32 x1 = (i32)p1.x;
    i32 y1 = (i32)p1.y;
    cohenSutherlandClip(&x0, &y0, &x1, &y1, out->width, out->height);

    int dx = abs(x1 - x0);
    int dy = abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;

    while (1)
    {
        DrawSetPixel(v2(x0, y0), color);

        if (x0 == x1 && y0 == y1) break;

        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void DrawImage(Image image, Vector2 pos)
{
    u32 *pixels = (u32 *)out->pixels;

    Rectangle2 rect = r2(pos, pos + v2_from_v2i(image.size));
    rect = abs_r2(rect);

    i32 in_x0 = Clamp((i32)rect.x0, 0, out->width);
    i32 in_y0 = Clamp((i32)rect.y0, 0, out->height);

    i32 in_x1 = Clamp((i32)rect.x1, 0, out->width);
    i32 in_y1 = Clamp((i32)rect.y1, 0, out->height);

    if (in_x0 == in_x1 || in_y0 == in_y1) return;
    if (image.size.width == 0 || image.size.height == 0) return;

    u32 *samples = image.pixels;
    u32 *at = &pixels[in_y0 * out->width + in_x0];

    i32 height = in_y1 - in_y0;
    i32 width = in_x1 - in_x0;

    i32 src_pos_x = in_x0 - (i32)rect.x0;
    i32 src_pos_y = in_y0 - (i32)rect.y0;

    u8 *in_data = (u8 *)image.pixels;
    u32 in_pitch = sizeof(u32) * image.size.width;
    u8 *in_line = in_data + (src_pos_y * in_pitch) + (sizeof(u32) * src_pos_x);

    u8 *out_data = (u8 *)out->pixels;
    u32 out_pitch = sizeof(u32) * out->width;
    u8 *out_line = out_data + (in_y0 * out_pitch) + (sizeof(u32) * in_x0);

    u32 in_copy_size = sizeof(u32) * width;

    for (i32 y = 0; y < height; y += 1)
    {
        u32 *in_pixel = (u32 *)in_line;
        u32 *out_pixel = (u32 *)out_line;

        for (int x = 0; x < width; x += 1)
        {
            u32 sample_color = *in_pixel;
            if ((sample_color & 0xff000000) != 0)
            {
                *out_pixel = sample_color;
            }

            in_pixel += 1;
            out_pixel += 1;
        }

        in_line += in_pitch;
        out_line += out_pitch;
    }
}

void DrawImageExt(Image image, Rectangle2 rect, Vector4 color, Rectangle2 uv)
{
    u32 *pixels = (u32 *)out->pixels;

    rect = abs_r2(rect);

    i32 width = (i32)r2_width(rect);
    i32 height = (i32)r2_height(rect);

    i32 in_x0 = Clamp((i32)rect.x0, 0, out->width);
    i32 in_y0 = Clamp((i32)rect.y0, 0, out->height);

    i32 in_x1 = Clamp((i32)rect.x1, 0, out->width);
    i32 in_y1 = Clamp((i32)rect.y1, 0, out->height);

    if (in_x0 == in_x1 || in_y0 == in_y1) return;
    if (image.size.width == 0 || image.size.height == 0) return;

    i32 src_pos_x = in_x0 - (i32)rect.x0;
    i32 src_pos_y = in_y0 - (i32)rect.y0;
    
    u32 *samples = image.pixels;
    u32 *at = &pixels[in_y0 * out->width + in_x0];

    b32 color_is_white = color.r == 1 && color.g == 1 && color.b == 1 && color.a == 1;

    for (i32 y = in_y0; y < in_y1; y += 1)
    {
        for (i32 x = in_x0; x < in_x1; x += 1)
        {
            f32 u = ((x - in_x0 + src_pos_x) + 0.5) / (f32)(width);
            f32 v = ((y - in_y0 + src_pos_y) + 0.5) / (f32)(height);

            // remap to input uv range
            u = uv.x0 + u * (uv.x1 - uv.x0);
            v = uv.y0 + v * (uv.y1 - uv.y0);

            if (u < 0) u += 1;
            if (v < 0) v += 1;
            if (u > 1) u -= 1;
            if (v > 1) v -= 1;

            i32 sample_x = floor_i32(u * (image.size.width)) % image.size.width;
            i32 sample_y = floor_i32(v * (image.size.height)) % image.size.height;
            sample_x = Max(sample_x, 0);
            sample_y = Max(sample_y, 0);

            assert((sample_x >= 0 && sample_x < image.size.width) && (sample_y >= 0 && sample_y < image.size.height));

            u32 sample_color = image.pixels[sample_y * image.size.width + sample_x];

            if ((sample_color & 0xff000000) != 0)
            {
                if (color_is_white)
                {
                    *at = sample_color;
                }
                else
                {
                    *at = u32_rgba_from_v4(v4_rgba_from_u32(sample_color) * color);
                }
            }

            at += 1;
        }

        at += out->width - (in_x1 - in_x0);
    }
}

void DrawImageMirrored(Image image, Vector2 pos, b32 flip_x, b32 flip_y)
{
    Rectangle2 dest = r2(pos, pos + v2_from_v2i(image.size));
    Rectangle2 uv = r2_from_f32(0, 0, 1, 1);
    if (flip_x) { uv.x0 = 1; uv.x1 = 0; }
    if (flip_y) { uv.y0 = 1; uv.y1 = 0; }
    DrawImageExt(image, dest, v4_white, uv);
}

Font_Glyph FontGetGlyph(Font font, u32 character)
{
    Font_Glyph result = font.glyphs[0];

    for (int index = 1; index < font.glyph_count; index += 1)
    {
        Font_Glyph *it = &font.glyphs[index];
        if (it->character == character)
        {
            result = *it;
            break;
        }
    }

    return result;
}


Vector2 MeasureText(Font font, String text)
{
    Vector2 result = {0};

    f32 line_height = 0;

    String32 unicode_text = string32_from_string(temp_arena(), text);
    for (i32 i = 0; i < unicode_text.count; i += 1)
    {
        u32 character = unicode_text.data[i];
        Font_Glyph glyph = FontGetGlyph(font, character);

        result.x += glyph.xadvance;
        result.y = Max(result.y, glyph.size.y);
    }

    return result;
}

void DrawTextExt(Font font, String text, Vector2 pos, Vector4 color, Vector2 anchor, f32 scale)
{
    if (scale <= 0) scale = 1.0;
    
    Vector2 cursor = pos;
    if (!v2_is_zero(anchor))
    {
        Vector2 size = MeasureText(font, text);
        cursor -= size * anchor * scale;
    }

    String32 unicode_text = string32_from_string(temp_arena(), text);
    for (i32 i = 0; i < unicode_text.count; i += 1)
    {
        u32 character = unicode_text.data[i];
        Font_Glyph glyph = FontGetGlyph(font, character);

        Rectangle2 uv = r2(
            (v2_from_v2i(glyph.pos)) / (v2_from_v2i(font.image.size)),
            (v2_from_v2i(glyph.pos) + v2_from_v2i(glyph.size)) / (v2_from_v2i(font.image.size))
        );

        if (color.a > 0)
        {
            Vector2 pos = cursor;
            pos += v2_from_v2i(glyph.line_offset) * scale;
            DrawImageExt(font.image, r2(pos, pos + v2_from_v2i(glyph.size) * scale), color, uv);
        }

        cursor.x += glyph.xadvance * scale;
    }
}

void DrawText(Font font, String text, Vector2 pos)
{
    DrawTextExt(font, text, pos, v4_white, v2_zero, 1);
}

void DrawTextAlign(Font font, String text, Vector2 pos, Vector2 anchor)
{
    DrawTextExt(font, text, pos, v4_white, anchor, 1);
}

void DrawClear(Vector4 color)
{
    DrawRect(r2(v2(0, 0), v2(out->width, out->height)), color);
}

//
// Sound API
//

#define MAX_CONCURRENT_SOUNDS ((f32)8)
#define MAX_SOUND_SIZE (I16_MAX * (1.0 / MAX_CONCURRENT_SOUNDS))

void PlaySine(f32 tone_hz, u32 sample_offset, f32 volume)
{
    volume = clamp_f32(volume, 0, 2);

    int wave_period = out->samples_per_second / tone_hz;
    f32 t_sine = sample_offset * TAU / (f32)wave_period;
    t_sine = Mod(t_sine, TAU);

    i16 *sample_out = out->samples;
    for (int sample_index = 0; sample_index < out->sample_count; sample_index++)
    {
        f32 sine_value   = sin_f32(t_sine);
        i16 sample_value = (i16)(sine_value * volume * MAX_SOUND_SIZE);
        *sample_out++ += sample_value;
        *sample_out++ += sample_value;

        t_sine += TAU / (f32)wave_period;
        if (t_sine >= TAU) {
            t_sine -= TAU;
        }
    }
}

void PlaySquare(f32 tone_hz, u32 sample_offset, f32 volume)
{
    volume = clamp_f32(volume, 0, 2);

    f32 wave_period = (f32)out->samples_per_second / tone_hz;
    f32 t_sine = sample_offset * TAU / wave_period;
    t_sine = Mod(t_sine, TAU);

    i16 *sample_out = out->samples;
    for (int sample_index = 0; sample_index < out->sample_count; sample_index++)
    {
        f32 sine_value   = t_sine <= PI ? 1.0 : -1.0;
        i16 sample_value = (i16)(sine_value * volume * MAX_SOUND_SIZE);
        *sample_out++ += sample_value;
        *sample_out++ += sample_value;

        t_sine += TAU / (f32)wave_period;
        if (t_sine >= TAU) {
            t_sine -= TAU;
        }
    }
}

void PlayTriangle(f32 tone_hz, u32 sample_offset, f32 volume)
{
    volume = clamp_f32(volume, 0, 2);

    f32 wave_period = (f32)out->samples_per_second / tone_hz;
    f32 t_sine = sample_offset * TAU / (f32)wave_period;
    t_sine = Mod(t_sine, TAU);

    i16 *sample_out = out->samples;
    for (i32 sample_index = 0; sample_index < out->sample_count; sample_index++)
    {
        f32 triangle_value   = 1.0 - 2.0 * abs_f32(t_sine / PI - 1.0);

        i16 sample_value = (i16)(triangle_value * volume * MAX_SOUND_SIZE);
        *sample_out++ += sample_value;
        *sample_out++ += sample_value;

        t_sine += TAU / (f32)wave_period;
        if (t_sine >= TAU) {
            t_sine -= TAU;
        }
    }
}

void PlaySawtooth(f32 tone_hz, u32 sample_offset, f32 volume)
{
    volume = clamp_f32(volume, 0, 2);

    f32 wave_period = (f32)out->samples_per_second / tone_hz;
    f32 t_sine = sample_offset * TAU / (f32)wave_period;
    t_sine = Mod(t_sine, TAU);

    i16 *sample_out = out->samples;
    for (int sample_index = 0; sample_index < out->sample_count; sample_index++)
    {
        f32 sawtooth_value   = t_sine / PI - 1.0;
        i16 sample_value = (i16)(sawtooth_value * volume * MAX_SOUND_SIZE);
        *sample_out++ += sample_value;
        *sample_out++ += sample_value;

        t_sine += TAU / (f32)wave_period;
        if (t_sine >= TAU) {
            t_sine -= TAU;
        }
    }
}

void PlayNoise(f32 volume)
{
    volume = clamp_f32(volume, 0, 2);

    Random_PCG *rng = &g_state.rng;

    i16 *sample_out = out->samples;
    for (int sample_index = 0; sample_index < out->sample_count; sample_index++)
    {
        f32 random_value = random_pcg_between_f32(rng, -1.0, 1.0);
        i16 sample_value = (i16)(random_value * volume * MAX_SOUND_SIZE);
        *sample_out++ += sample_value;
        *sample_out++ += sample_value;
    }
}

u32 PlaySoundStream(Sound sound, u32 sample_offset, f32 volume)
{
    Sound_Asset *asset = (Sound_Asset *)GetAssetByIndex(&g_state.sounds, sizeof(Sound_Asset), count_of(g_state.sounds), sound.index); 
    if (!asset) return 0;

    volume = clamp_f32(volume, 0, 2);

    i16 *samples = out->samples;

    i16 *at = (i16 *)((u8 *)sound.samples + sample_offset * sizeof(i16) * 2);

    u32 samples_remaining = sound.total_samples - sample_offset;

    u32 sample_count = Min(out->sample_count, samples_remaining);

    for (int sample_index = 0; sample_index < sample_count; sample_index++)
    {
        *samples++ += *at * (volume / MAX_CONCURRENT_SOUNDS);
        at += 1;

        *samples++ += *at * (volume / MAX_CONCURRENT_SOUNDS);
        at += 1;
    }

    return sample_count;
}

void MixerPlaySound(Sound sound, f32 volume)
{
    Playing_Sound play = {0};
    play.sound = sound;
    play.volume = volume;

    if (g_state.playing_sounds.count < g_state.playing_sounds.capacity)
    {
        array_add(&g_state.playing_sounds, play);
    }
}

void MixerSetMasterVolume(f32 master_volume)
{
    g_state.master_volume = Clamp(master_volume, 0.0, 1.0);
}

void MixerOutputPlayingSounds()
{
    Playing_Sound_Array *sounds = &g_state.playing_sounds;

    f32 master_volume = g_state.master_volume;

    for (i64 index = sounds->count - 1; index >= 0; index -= 1)
    {
        Playing_Sound *sound = &sounds->data[index];
        sound->sample_offset += PlaySoundStream(sound->sound, sound->sample_offset, master_volume * sound->volume);

        if (sound->sample_offset >= sound->sound.total_samples)
        {
            array_remove_ordered(sounds, index);
        }
    }
}
