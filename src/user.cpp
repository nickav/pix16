struct Entity
{
    u64 id;
    u64 type;

    Vector2 position;
    Vector2 velocity;

    Vector2 size;
    Vector2 anchor;

    i32 facing;

    i32 sprite_index;
    f32 animation_time;
};

Entity player = {};

void GameStart(Game_Input *input, Game_Output *out)
{
    player.position = v2(out->width * 0.5, 0);
    player.size = v2(32, 32);
    player.anchor = v2(0.5, 0.5);
    player.facing = 1;
}

void GameUpdate(Game_Input *input)
{
    f32 dt = input->dt;

    f32 max_speed = 80.0;

    Controller *ctrl0 = &input->controllers[0];
    if (ctrl0->right)
    {
        player.velocity.x = move_f32(player.velocity.x, max_speed, 1000 * dt);
        player.facing = 1;
    }
    else if (ctrl0->left)
    {
        player.velocity.x = move_f32(player.velocity.x, -max_speed, 1000 * dt);
        player.facing = -1;
    }
    else
    {
        player.velocity.x = move_f32(player.velocity.x, 0, 2000 * dt);
    }

    player.velocity += v2(0, 240) * dt;

    if (player.velocity.y > 240) player.velocity.y = 240;
    if (player.velocity.y < -240) player.velocity.y = -240;
    if (player.velocity.x > 240) player.velocity.x = 240;
    if (player.velocity.x < -240) player.velocity.x = -240;

    player.position += player.velocity * dt;

    if (player.position.y > out->height - 16)
    {
        player.position.y = out->height - 16;
    }


    if (ControllerPressed(0, Button_A) || MousePressed(Mouse_Left))
    {
        player.velocity.y = -120;
    }
}

void GameRender(Game_Input *input, Game_Output *out)
{
    Image spr_guy = LoadImage(S("penguin_idle.png"));
    Image spr_guyWalk = LoadImage(S("penguin_walk.png"));

    DrawClear(v4(0.7, 0.6, 0.8, 1.0));

    if (abs_f32(player.velocity.x) > 0)
    {
        i32 num_frames = 4;
        i32 sprite_speed = 8;
        player.animation_time += input->dt;

        player.sprite_index = (i32)(player.animation_time * sprite_speed) % num_frames;

        f32 fxw = 1.0 / (f32)num_frames;
        Rectangle2 uv = r2(v2(player.facing * fxw * player.sprite_index, 0), v2(player.facing * fxw * (player.sprite_index + 1), 1));

        DrawImageExt(spr_guyWalk, r2(player.position - v2(16, 16), player.position + v2(16, 16)), v4_white, uv);
    }
    else
    {
        Rectangle2 uv = r2(v2(0, 0), v2(player.facing * 1, 1));
        DrawImageExt(spr_guy, r2(player.position - v2(16, 16), player.position + v2(16, 16)), v4_white, uv);
    }

    DrawLine(v2(out->width * 0.5, out->height * 0.5), input->mouse.position, v4_white);


    // TODO(nick): investigate strin32 decoding bug with cents symbol: ￠
    String font_chars = S(" ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789$ €£¥¤+-*/÷=%‰\"'#@&_(),.;:¿?¡!\\|{}<>[]§¶µ`^~©®™");
    Font font_hellomyoldfriend = LoadFont(S("spr_font_hellomyoldfriend_12x12_by_lotovik_strip110.png"), font_chars, v2i(12, 12));

    DrawText(font_hellomyoldfriend, S("Hello, Sailor!"), v2(0, 0));
}

void GameUpdateAndRender(Game_Input *input, Game_Output *out)
{
    static b32 initted = false;
    if (!initted)
    {
        GameStart(input, out);
        initted = true;
    }

    GameUpdate(input);
    GameRender(input, out);

    // DrawSprite(spr_guy, player.position);
}