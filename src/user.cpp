struct Entity
{
    u64 id;
    u64 type;

    Vector2 position;
    Vector2 velocity;

    Vector2 size;
    Vector2 anchor;
};

Entity player = {};

void GameStart(Game_Input *input, Game_Output *out)
{
    player.position = v2(out->width * 0.5, 0);
    player.size = v2(32, 32);
    player.anchor = v2(0.5, 0.5);
}

void GameUpdate(Game_Input *input)
{
    f32 dt = input->dt;

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

    Controller *ctrl0 = &input->controllers[0];
    if (ctrl0->right)
    {
        player.position.x += 160 * dt;
    }
    if (ctrl0->left)
    {
        player.position.x -= 160 * dt;
    }

    if (ctrl0->a)
    {
        player.velocity.y = -160;
    }
}

void GameRender(Game_Output *out)
{
    Image spr_guy = LoadImage(S("penguin_idle.png"));

    DrawClear(v4(0.7, 0.6, 0.8, 1.0));
    DrawImage(spr_guy, player.position - v2(16, 16));
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
    GameRender(out);

    // DrawSprite(spr_guy, player.position);
}