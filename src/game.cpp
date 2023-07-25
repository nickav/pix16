#pragma once

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

    DrawRect(out, r2(v2(0, 0), v2(1, 1)), v4_green);
}