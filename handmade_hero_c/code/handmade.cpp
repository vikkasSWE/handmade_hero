#include "handmade.h"
#include <math.h>

internal void GameOutputSound(game_sound_output_buffer *SoundBuffer, int32 ToneHz)
{
    local_persist real32 tSine;
    int16 ToneVolume = 3000;
    int16 WavePeriod = SoundBuffer->SamplesPerSecond / ToneHz;

    int16 *SampleOut = SoundBuffer->Samples;
    for (int32 SampleIndex = 0; SampleIndex < SoundBuffer->SampleCount; SampleIndex++)
    {
        real32 SineValue = sinf(tSine);
        int16 SampleValue = (int16)(SineValue * ToneVolume);
        *SampleOut++ = SampleValue;
        *SampleOut++ = SampleValue;

        tSine += 2.0f * Pi32 * 1.0f / (real32)WavePeriod;
    }
}

internal void RenderWeirdGradient(game_offscreen_buffer *Buffer, int XOffset, int YOffset)
{
    uint8 *Row = (uint8 *)Buffer->Memory;

    for (int32 Y = 0; Y < Buffer->Height; Y++)
    {
        uint32 *Pixel = (uint32 *)Row;

        for (int32 X = 0; X < Buffer->Width; X++)
        {
            uint8 Blue = X + XOffset;
            uint8 Green = Y + YOffset;
            // 0xXXRRGGBB
            *Pixel++ = (Green << 8) | Blue;
        }

        Row = Row + Buffer->Width * Buffer->BytesPerPixel;
    }
}

internal void GameUpdateAndRender(game_input *Input, game_offscreen_buffer *Buffer, game_sound_output_buffer *SoundBuffer)
{
    local_persist int32 BlueOffset = 0;
    local_persist int32 GreenOffset = 0;
    local_persist int32 ToneHz = 256;

    game_controller_input *Input0 = &Input->Controllers[0];
    if (Input0->IsAnalog)
    {
        BlueOffset += (int32)4.0f * (Input0->EndX);
        ToneHz = 256 + (int32)(128.0f * (Input0->EndY));
    }
    else
    {
    }

    if (Input0->Down.EndedDown)
    {
        GreenOffset += 1;
    }

    // TODO: Allow sample offsets here for more robust platform options
    GameOutputSound(SoundBuffer, ToneHz);
    RenderWeirdGradient(Buffer, BlueOffset, GreenOffset);
}
