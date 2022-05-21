#pragma once

#include <stdint.h>

struct game_offscreen_buffer
{
    void *Memory;
    int BytesPerPixel;
    int Width;
    int Height;
};

static void GameUpdateAndRender(game_offscreen_buffer *Buffer, int BlueOffset, int GreenOffset);
static void RenderWeirdGradeint(game_offscreen_buffer *Buffer, int XOffset, int YOffset);