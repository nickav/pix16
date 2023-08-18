void GameUpdateAndRender(Game_Input *input, Game_Output *out)
{
    static i32 slide_index = 0;

    static b32 did_advance_slides = false;

    Controller *ctrl0 = &input->controllers[0];
    if (ctrl0->b)
    {
        if (!did_advance_slides)
        {
            DrawRect(out, r2(v2(0, 0), v2(out->width, out->height)), v4_black);
            slide_index += 1;
            did_advance_slides = true;
        }
    }
    else if (ctrl0->a)
    {
        if (!did_advance_slides)
        {
            DrawRect(out, r2(v2(0, 0), v2(out->width, out->height)), v4_black);
            slide_index -= 1;
            did_advance_slides = true;
        }
    }
    else
    {
        did_advance_slides = false;
    }

    u32 num_slides = 0;

    if (slide_index == num_slides)
    {
        DrawRect(out, r2(v2(0, 0), v2(out->width, out->height)), v4_black);

        DrawTriangleExt(out,
            v2(out->width * 0.5, 0), v4_red,
            input->mouse.position, v4_green,
            v2(out->width, out->height), v4_blue);
    }
    num_slides += 1;

    if (slide_index == num_slides)
    {
        DrawRect(out, r2(v2(0, 0), v2(out->width, out->height)), v4_black);
        DrawRectExt(out, r2(v2(0, 0), v2(out->width, out->height)), v4_red, v4_green, v4_white, v4_blue);
    }
    num_slides += 1;

    if (slide_index == num_slides)
    {
        DrawRect(out, r2(v2(0, 0), v2(out->width, out->height)), v4_black);

        static Vector2 pos = {0};

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

        DrawTriangle(out, v2(16, 16), v2(128, 0), v2(200, 200), v4_magenta);

        DrawRect(out, r2(pos + v2(0, 0), pos + v2(32, 32)), v4_red);

        DrawSetPixel(out, v2(0, 0), v4_yellow);
        DrawSetPixel(out, v2(1, 1), v4_blue);

        Image image = LoadImage(input, S("../data/guy.png"));
        DrawImage(out, image, v2(64, 64));

        DrawImageExt(out, image, r2(v2(128, 128), v2(256, 256)), r2(v2(0, 0), v2(1, 1)));

        DrawCircle(out, v2(200, 32), 32, v4_blue);
    }
    num_slides += 1;

    if (slide_index == num_slides)
    {
        DrawRect(out, r2(v2(0, 0), v2(out->width, out->height)), v4_black);

        Image image = LoadImage(input, S("../data/guy.png"));
        DrawImageExt(out, image, r2(v2(0, 0), v2(out->width, out->height)), r2(v2(0, 0), v2(2, 2)));
    }
    num_slides += 1;

    Sound *sound = LoadSound(input, S("../data/snd_boot_sequence.wav"));
    static b32 did_reset = false;
    if (ctrl0->down)
    {
        if (!did_reset)
        {
            sound->sample_offset = 0;
            did_reset = true;
        }
    }
    else
    {
        did_reset = false;
    }

    PlaySoundStream(out, sound);

    if (slide_index < 0) slide_index += num_slides;
    if (slide_index >= num_slides) slide_index -= num_slides;

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