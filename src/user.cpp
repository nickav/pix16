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

void GameUpdateAndRender(Game_Input *input, Game_Output *out)
{
    static b32 initted = false;
    if (!initted)
    {
        GameStart(input, out);
        initted = true;
    }

    Image spr_guy = LoadImage(S("penguin_idle.png"));

    player.velocity += v2(0, 240) * input->dt;

    if (player.velocity.y > 240) player.velocity.y = 240;
    if (player.velocity.y < -240) player.velocity.y = -240;
    if (player.velocity.x > 240) player.velocity.x = 240;
    if (player.velocity.x < -240) player.velocity.x = -240;

    player.position += player.velocity * input->dt;

    if (player.position.y > out->height - 16)
    {
        player.position.y = out->height - 16;
    }

    DrawClear(v4(0.7, 0.6, 0.8, 1.0));

    DrawImage(spr_guy, player.position - v2(16, 16));

    // DrawSprite(spr_guy, player.position);
}