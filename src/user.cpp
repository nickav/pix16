struct Entity
{
    u64 id;
    u64 type;

    Vector2 position;
    Vector2 velocity;
};

Entity player = {};

void GameUpdateAndRender(Game_Input *input, Game_Output *out)
{
    Image spr_guy = LoadImage(S("penguin_idle.png"));

    player.velocity += v2(0, 80) * input->dt;
    player.position += player.velocity * input->dt;

    DrawClear(out, v4(0.7, 0.6, 0.8, 1.0));

    DrawImage(out, spr_guy, player.position);
}