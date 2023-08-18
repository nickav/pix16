#pragma once

#define STB_IMAGE_IMPLEMENTATION
#include "third_party/stb_image.h"

#define DR_WAV_IMPLEMENTATION
#include "third_party/dr_wav.h"

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

void PlaySine(Game_Output *out, f32 tone_hz, f32 volume)
{
    int wave_period = out->samples_per_second / tone_hz;
    f32 t_sine = out->samples_played * TAU / (f32)wave_period;
    t_sine = Mod(t_sine, TAU);

    i16 *sample_out = out->samples;
    for (int sample_index = 0; sample_index < out->sample_count; sample_index++)
    {
        f32 sine_value   = sin_f32(t_sine);
        i16 sample_value = (i16)(sine_value * volume);
        *sample_out++ += sample_value;
        *sample_out++ += sample_value;

        t_sine += TAU / (f32)wave_period;
        if (t_sine >= TAU) {
            t_sine -= TAU;
        }
    }
}

Image LoadImage(Game_Input *input, String path)
{
    u64 hash = murmur64(path.data, path.count);

    Image *result = NULL;
    for (i64 index = 0; index < count_of(input->images); index += 1)
    {
        Image *it = &input->images[index];
        if (it->hash == hash)
        {
            result = it;
            break;
        }
    }

    if (!result)
    {
        for (i64 index = 0; index < count_of(input->images); index += 1)
        {
            Image *it = &input->images[index];
            if (it->hash == 0)
            {
                result = it;
                break;
            }
        }

        if (!result)
        {
            print("[LoadImage] Used all %d slots available! Failed to load image: %.*s\n", count_of(input->images), LIT(path));
        }
    }

    if (result)
    {
        if (result->hash == 0)
        {
            M_Temp scratch = GetScratch(0, 0);

            String contents = os_read_entire_file(scratch.arena, path);

            if (contents.count > 0)
            {
                int width, height, channels;
                result->pixels      = (u32 *)stbi_load_from_memory(contents.data, contents.count, &width, &height, &channels, 4);
                result->size.width  = width;
                result->size.height = height;
            }
            else
            {
                print("[LoadImage] Image not found: %.*s\n", LIT(path));
            }

            result->name = path;
            result->hash = hash;

            ReleaseScratch(scratch);
        }

        return *result;
    }

    return {};
}

void FreeImage(Game_Input *input, String path)
{
    u64 hash = murmur64(path.data, path.count);

    Image *result = NULL;
    for (i64 index = 0; index < count_of(input->images); index += 1)
    {
        Image *it = &input->images[index];
        if (it->hash == hash)
        {
            free(it->pixels); 
            it->pixels = 0;

            it->hash = 0;
            MemoryZeroStruct(it);

            break;
        }
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

void DrawImageExt(Game_Output *out, Image image, Rectangle2 rect, Rectangle2 uv)
{
    u32 *pixels = (u32 *)out->pixels;

    rect = abs_r2(rect);

    i32 in_x0 = Clamp((i32)rect.x0, 0, out->width);
    i32 in_x1 = Clamp((i32)rect.x1, 0, out->width);

    i32 in_y0 = Clamp((i32)rect.y0, 0, out->height);
    i32 in_y1 = Clamp((i32)rect.y1, 0, out->height);


    u32 *samples = image.pixels;
    u32 *at = &pixels[in_y0 * out->width + in_x0];

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
                *at = sample_color;
            }

            at += 1;
        }

        at += out->width - (in_x1 - in_x0);
    }
}

Sound *LoadSound(Game_Input *input, String path)
{
    u64 hash = murmur64(path.data, path.count);

    Sound *result = NULL;
    for (i64 index = 0; index < count_of(input->sounds); index += 1)
    {
        Sound *it = &input->sounds[index];
        if (it->hash == hash)
        {
            result = it;
            break;
        }
    }

    if (!result)
    {
        for (i64 index = 0; index < count_of(input->sounds); index += 1)
        {
            Sound *it = &input->sounds[index];
            if (it->hash == 0)
            {
                result = it;
                break;
            }
        }

        if (!result)
        {
            print("[LoadSound] Used all %d slots available! Failed to load sound: %.*s\n", count_of(input->sounds), LIT(path));
        }
    }

    if (result)
    {
        if (result->hash == 0)
        {
            M_Temp scratch = GetScratch(0, 0);

            String contents = os_read_entire_file(scratch.arena, path);

            if (contents.count > 0)
            {
                unsigned int channels;
                unsigned int sample_rate;
                drwav_uint64 total_pcm_frame_count;
                i16 *samples = drwav_open_memory_and_read_pcm_frames_s16(contents.data, contents.count, &channels, &sample_rate, &total_pcm_frame_count, NULL);

                result->bits_per_sample = 32;
                result->num_channels = channels;
                result->sample_rate = sample_rate;
                result->total_samples = total_pcm_frame_count;
                result->samples = samples;
                result->sample_offset = 0;

                assert(channels == 2);
                assert(sample_rate == 44100);
            }
            else
            {
                print("[LoadSound] Sound not found: %.*s\n", LIT(path));
            }

            result->name = path;
            result->hash = hash;

            ReleaseScratch(scratch);
        }
    }

    return result;
}

void FreeSound(Game_Input *input, String path)
{
    u64 hash = murmur64(path.data, path.count);

    Sound *result = NULL;
    for (i64 index = 0; index < count_of(input->sounds); index += 1)
    {
        Sound *it = &input->sounds[index];
        if (it->hash == hash)
        {
            free(it->samples); 
            it->samples = 0;

            it->hash = 0;
            MemoryZeroStruct(it);

            break;
        }
    }
}

void PlaySoundStream(Game_Output *out, Sound *sound)
{
    i16 *samples = out->samples;
    i16 *at = (i16 *)((u8 *)sound->samples + sound->sample_offset * sizeof(i16) * 2);

    u32 samples_remaining = sound->total_samples - sound->sample_offset;

    u32 sample_count = Min(out->sample_count, samples_remaining);

    for (int sample_index = 0; sample_index < sample_count; sample_index++)
    {
        *samples++ += *at;
        at += 1;

        *samples++ += *at;
        at += 1;
    }

    sound->sample_offset += sample_count;
}
