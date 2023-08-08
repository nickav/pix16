#pragma once

void DrawSetPixel(Game_Output *out, Vector2 pos, Vector4 color)
{
    i32 in_x = Clamp((i32)pos.x, 0, out->width);
    i32 in_y = Clamp((i32)pos.y, 0, out->height);

    u32 out_color = u32_from_v4(color);

    assert(in_y * out->width + in_x < (out->width * out->height));

    u32 *at = &out->pixels[in_y * out->width + in_x];
    *at = out_color;
}

void DrawRect(Game_Output *out, Rectangle2 rect, Vector4 color)
{
    u32 *pixels = (u32 *)out->pixels;

    rect = abs_r2(rect);

    i32 in_x0 = Clamp((i32)rect.x0, 0, out->width);
    i32 in_x1 = Clamp((i32)rect.x1, 0, out->width);

    i32 in_y0 = Clamp((i32)rect.y0, 0, out->height);
    i32 in_y1 = Clamp((i32)rect.y1, 0, out->height);

    u32 out_color = u32_from_v4(color);


    u32 *at = &pixels[in_y0 * out->width + in_x0];

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

void GameUpdateAndRender(Game_Input *input, Game_Output *out)
{
    static Vector2 pos = {0};

    Controller *ctrl0 = &input->controllers[0];
    if (ctrl0->right)
    {
        pos.x += input->dt * 400;
    }
    if (ctrl0->left)
    {
        pos.x -= input->dt * 400;
    }
    if (ctrl0->up)
    {
        pos.y -= input->dt * 400;
    }
    if (ctrl0->down)
    {
        pos.y += input->dt * 400;
    }

    DrawRect(out, r2(v2(0, 0), v2(out->width, out->height)), v4_green);

    DrawRect(out, r2(pos + v2(0, 0), pos + v2(32, 32)), v4_red);

    DrawSetPixel(out, v2(0, 0), v4_yellow);
    DrawSetPixel(out, v2(1, 1), v4_blue);


    if (ctrl0->right)
    {
        PlaySine(out, 440.0f, 3000);
    }

    if (ctrl0->left)
    {
        PlaySine(out, 523.25f, 3000);
        PlaySine(out, 783.99f, 3000);
    }
}