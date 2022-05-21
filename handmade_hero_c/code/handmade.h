#pragma once

#include <stdint.h>

#include "types.h"

struct game_offscreen_buffer
{
    void *Memory;
    int BytesPerPixel;
    int Width;
    int Height;
};

struct game_sound_output_buffer
{
    int SamplesPerSecond;
    int SampleCount;
    int16 *Samples;
};

internal void GameUpdateAndRender(game_offscreen_buffer *Buffer, int BlueOffset, int GreenOffset);
internal void RenderWeirdGradeint(game_offscreen_buffer *Buffer, int XOffset, int YOffset);