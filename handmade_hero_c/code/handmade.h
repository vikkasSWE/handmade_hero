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

struct game_button_state
{
    int32 HalfTransitionCount;
    bool32 EndedDown;
};

struct game_controller_input
{
    bool32 IsAnalog;

    real32 StartX;
    real32 StartY;

    real32 MinX;
    real32 MinY;

    real32 MaxX;
    real32 MaxY;

    real32 EndX;
    real32 EndY;

    union
    {
        game_button_state Buttons[6];
        struct
        {
            game_button_state Up;
            game_button_state Down;
            game_button_state Left;
            game_button_state Right;
            game_button_state ShoulderLeft;
            game_button_state ShoulderRight;
        };
    };
};

struct game_input
{
    game_controller_input Controllers[4];
};

internal void GameUpdateAndRender(game_input *Input, game_offscreen_buffer *Buffer, game_sound_output_buffer *SoundBuffer);
internal void RenderWeirdGradeint(game_offscreen_buffer *Buffer, int XOffset, int YOffset);