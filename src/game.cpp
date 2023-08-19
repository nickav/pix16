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

    u32 sample_offset;
};

struct Font_Asset
{
    Asset_Info info;
    Font font;
};

struct Game_State
{
    Image_Asset images[1024];
    Sound_Asset sounds[1024];
    Font_Asset  fonts[1024];

    Random_State rng;
};

static Game_State g_state = {0};

void GameInit()
{
    g_state.rng = {0x6908243098231};
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
    u64 hash = murmur64(path.data, path.count);

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

            String contents = os_read_entire_file(scratch.arena, path);

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

            String contents = os_read_entire_file(scratch.arena, path);

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

Font FontMake(Image image, String alphabet, Vector2i monospaced_letter_size)
{
    Font result = {0};

    Vector2i cursor = {0, 0};

    result.image = image;

    Font_Glyph *null_glyph = &result.glyphs[0];
    null_glyph->character = 0;
    null_glyph->size = v2i(monospaced_letter_size.x, 0);
    result.glyph_count += 1;

    for (i32 index = 0; index < Min(alphabet.count, count_of(result.glyphs)); index += 1)
    {
        u32 character = alphabet.data[index];

        Font_Glyph *glyph = &result.glyphs[result.glyph_count];
        result.glyph_count += 1;

        glyph->character = character;
        glyph->pos = cursor;
        glyph->size = monospaced_letter_size;

        cursor.x += monospaced_letter_size.x;
        if (cursor.x >= image.size.width)
        {
            cursor.x = 0;
            cursor.y += monospaced_letter_size.y;
        }
    }

    return result;
}

//
// Drawing API
//

void DrawSetPixel(Game_Output *out, Vector2 pos, Vector4 color)
{
    i32 in_x = Clamp((i32)pos.x, 0, out->width);
    i32 in_y = Clamp((i32)pos.y, 0, out->height);

    u32 out_color = rgba_u32_from_v4(color);

    assert(in_y * out->width + in_x < (out->width * out->height));

    u32 *at = &out->pixels[in_y * out->width + in_x];
    *at = out_color;
}

u32 DrawGetPixel(Game_Output *out, Vector2 pos)
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

void DrawRect(Game_Output *out, Rectangle2 rect, Vector4 color)
{
    TimeFunction;

    rect = abs_r2(rect);

    i32 in_x0 = Clamp((i32)rect.x0, 0, out->width);
    i32 in_x1 = Clamp((i32)rect.x1, 0, out->width);

    i32 in_y0 = Clamp((i32)rect.y0, 0, out->height);
    i32 in_y1 = Clamp((i32)rect.y1, 0, out->height);

    u32 out_color = rgba_u32_from_v4(color);

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
void DrawRectExt(Game_Output *out, Rectangle2 rect, Vector4 c0, Vector4 c1, Vector4 c2, Vector4 c3)
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

            u32 sample_color = rgba_u32_from_v4(sample);

            *at = sample_color;
            at += 1;
        }

        at += out->width - (in_x1 - in_x0);
    }
}

void DrawCircle(Game_Output *out, Vector2 pos, f32 radius, Vector4 color)
{
    i32 in_x0 = Clamp((i32)pos.x - radius, 0, out->width);
    i32 in_x1 = Clamp((i32)pos.x + radius, 0, out->width);

    i32 in_y0 = Clamp((i32)pos.y - radius, 0, out->height);
    i32 in_y1 = Clamp((i32)pos.y + radius, 0, out->height);

    u32 out_color = rgba_u32_from_v4(color);
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

void DrawTriangle(Game_Output *out, Vector2 p0, Vector2 p1, Vector2 p2, Vector4 color)
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

    u32 out_color = rgba_u32_from_v4(color);

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

void DrawTriangleExt(Game_Output *out, Vector2 p0, Vector4 c0, Vector2 p1, Vector4 c1, Vector2 p2, Vector4 c2)
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
                u32 sample_color = rgba_u32_from_v4(sample);

                *at = sample_color;
            }
            
            at += 1;
        }

        at += out->width - (in_x1 - in_x0);
    }
}

void DrawImage(Game_Output *out, Image image, Vector2 pos)
{
    u32 *pixels = (u32 *)out->pixels;

    Rectangle2 rect = r2(pos, pos + v2_from_v2i(image.size));
    rect = abs_r2(rect);

    i32 in_x0 = Clamp((i32)rect.x0, 0, out->width);
    i32 in_x1 = Clamp((i32)rect.x1, 0, out->width);

    i32 in_y0 = Clamp((i32)rect.y0, 0, out->height);
    i32 in_y1 = Clamp((i32)rect.y1, 0, out->height);

    if (in_x0 == in_x1 || in_y0 == in_y1) return;
    if (image.size.width == 0 || image.size.height == 0) return;

    u32 *samples = image.pixels;
    u32 *at = &pixels[in_y0 * out->width + in_x0];

    for (i32 y = 0; y < image.size.height; y += 1)
    {
        for (i32 x = 0; x < image.size.height; x += 1)
        {
            u32 sample_color = *samples;
            if ((sample_color & 0xff000000) != 0)
            {
                *at = sample_color;
            }

            samples += 1;
            at += 1;
        }

        at += out->width - (in_x1 - in_x0);
    }
}

void DrawImageExt(Game_Output *out, Image image, Rectangle2 rect, Vector4 color, Rectangle2 uv)
{
    u32 *pixels = (u32 *)out->pixels;

    rect = abs_r2(rect);

    i32 in_x0 = Clamp((i32)rect.x0, 0, out->width);
    i32 in_x1 = Clamp((i32)rect.x1, 0, out->width);

    i32 in_y0 = Clamp((i32)rect.y0, 0, out->height);
    i32 in_y1 = Clamp((i32)rect.y1, 0, out->height);

    if (in_x0 == in_x1 || in_y0 == in_y1) return;
    if (image.size.width == 0 || image.size.height == 0) return;

    u32 *samples = image.pixels;
    u32 *at = &pixels[in_y0 * out->width + in_x0];

    b32 color_is_white = color.r == 1 && color.g == 1 && color.b == 1 && color.a == 1;

    for (i32 y = in_y0; y < in_y1; y += 1)
    {
        for (i32 x = in_x0; x < in_x1; x += 1)
        {
            f32 u = (x - in_x0) / (f32)(in_x1 - in_x0);
            f32 v = (y - in_y0) / (f32)(in_y1 - in_y0);

            // remap to input uv range
            u = uv.x0 + u * (uv.x1 - uv.x0);
            v = uv.y0 + v * (uv.y1 - uv.y0);

            i32 sample_x = (i32)(u * image.size.width) % image.size.width;
            i32 sample_y = (i32)(v * image.size.height) % image.size.height;
            u32 sample_color = image.pixels[sample_y * image.size.width + sample_x];

            if ((sample_color & 0xff000000) != 0)
            {
                if (color_is_white)
                {
                    *at = sample_color;
                }
                else
                {
                    *at = rgba_u32_from_v4(rgba_v4_from_u32(sample_color) * color);
                }
            }

            at += 1;
        }

        at += out->width - (in_x1 - in_x0);
    }
}

Font_Glyph FontGetGlyph(Font font, u32 character)
{
    Font_Glyph result = font.glyphs[0];

    for (i32 index = 1; index < font.glyph_count; index += 1)
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

void DrawTextExt(Game_Output *out, Font font, String text, Vector2 pos, Vector4 color)
{
    Vector2 cursor = pos;

    for (i32 i = 0; i < text.count; i += 1)
    {
        u32 character = (u32)text.data[i];
        Font_Glyph glyph = FontGetGlyph(font, character);

        Rectangle2 uv = r2(
            (v2_from_v2i(glyph.pos)) / (v2_from_v2i(font.image.size)),
            (v2_from_v2i(glyph.pos) + v2_from_v2i(glyph.size)) / (v2_from_v2i(font.image.size))
        );

        if (color.a > 0)
        {
            DrawImageExt(out, font.image, r2(cursor, cursor + v2_from_v2i(glyph.size)), color, uv);
        }

        cursor.x += glyph.size.width;
    }
}

void DrawText(Game_Output *out, Font font, String text, Vector2 pos)
{
    DrawTextExt(out, font, text, pos, v4_white);
}

//
// Sound API
//

#define MAX_CONCURRENT_SOUNDS ((f32)8)
#define MAX_SOUND_SIZE (I16_MAX * (1.0 / MAX_CONCURRENT_SOUNDS))

void PlaySine(Game_Output *out, f32 tone_hz, f32 volume)
{
    volume = clamp_f32(volume, 0, 2);

    int wave_period = out->samples_per_second / tone_hz;
    f32 t_sine = out->samples_played * TAU / (f32)wave_period;
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

void PlaySquare(Game_Output *out, f32 tone_hz, f32 volume)
{
    volume = clamp_f32(volume, 0, 2);

    f32 wave_period = (f32)out->samples_per_second / tone_hz;
    f32 t_sine = out->samples_played * TAU / wave_period;
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

void PlayTriangle(Game_Output *out, f32 tone_hz, f32 volume)
{
    volume = clamp_f32(volume, 0, 2);

    f32 wave_period = (f32)out->samples_per_second / tone_hz;
    f32 t_sine = out->samples_played * TAU / (f32)wave_period;
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

void PlaySawtooth(Game_Output *out, f32 tone_hz, f32 volume)
{
    volume = clamp_f32(volume, 0, 2);

    f32 wave_period = (f32)out->samples_per_second / tone_hz;
    f32 t_sine = out->samples_played * TAU / (f32)wave_period;
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

void PlayNoise(Game_Output *out, f32 volume)
{
    volume = clamp_f32(volume, 0, 2);

    Random_State *rng = &g_state.rng;

    i16 *sample_out = out->samples;
    for (int sample_index = 0; sample_index < out->sample_count; sample_index++)
    {
        f32 random_value   = random_f32_between(rng, -1.0, 1.0);
        i16 sample_value = (i16)(random_value * volume * MAX_SOUND_SIZE);
        *sample_out++ += sample_value;
        *sample_out++ += sample_value;
    }
}

void PlaySoundStream(Game_Output *out, Sound sound, f32 volume)
{
    Sound_Asset *asset = (Sound_Asset *)GetAssetByIndex(&g_state.sounds, sizeof(Sound_Asset), count_of(g_state.sounds), sound.index); 
    if (!asset) return;

    volume = clamp_f32(volume, 0, 2);

    i16 *samples = out->samples;

    i16 *at = (i16 *)((u8 *)sound.samples + asset->sample_offset * sizeof(i16) * 2);

    u32 samples_remaining = sound.total_samples - asset->sample_offset;

    u32 sample_count = Min(out->sample_count, samples_remaining);

    for (int sample_index = 0; sample_index < sample_count; sample_index++)
    {
        *samples++ += *at * (volume / MAX_CONCURRENT_SOUNDS);
        at += 1;

        *samples++ += *at * (volume / MAX_CONCURRENT_SOUNDS);
        at += 1;
    }

    asset->sample_offset += sample_count;
}

f32 SoundGetTime(Game_Output *out, Sound sound)
{
    Sound_Asset *asset = (Sound_Asset *)GetAssetByIndex(&g_state.sounds, sizeof(Sound_Asset), count_of(g_state.sounds), sound.index); 
    if (!asset) return 0;

    assert(out->samples_per_second > 0);

    u32 sample_offset = clamp_u32(asset->sample_offset, 0, sound.total_samples);
    f32 result = sample_offset / (f32)out->samples_per_second;
    return result;
}

void SoundSeek(Game_Output *out, Sound sound, f32 time_in_seconds)
{
    Sound_Asset *asset = (Sound_Asset *)GetAssetByIndex(&g_state.sounds, sizeof(Sound_Asset), count_of(g_state.sounds), sound.index); 
    if (!asset) return;

    u32 sample_offset = out->samples_per_second * Max(time_in_seconds, 0);
    asset->sample_offset = clamp_u32(sample_offset, 0, sound.total_samples);
}
