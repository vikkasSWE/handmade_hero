#include <stdint.h>
#include <math.h>

#include "types.h"

#include "handmade.h"
#include "handmade.cpp"

#include <windows.h>
#include <xinput.h>
#include <dsound.h>
#include <stdio.h>

struct win32_offscreen_buffer
{
    void *Memory;
    BITMAPINFO Info;
    int BytesPerPixel;
    int Width;
    int Height;
};

struct win32_window_dimension
{
    int Width;
    int Height;
};

// ArrayCount
#define ArrayCount(Array) (sizeof((Array)) / sizeof((Array)[0]))

// XInputGetState
#define XINPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE *pState)
typedef XINPUT_GET_STATE(x_input_get_state);
XINPUT_GET_STATE(XInputGetStateStub)
{
    return ERROR_DEVICE_NOT_CONNECTED;
}
global_variable x_input_get_state *XInputGetState_ = XInputGetStateStub;

// XInputSetState
#define XINPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION *pVibration)
typedef XINPUT_SET_STATE(x_input_set_state);
XINPUT_SET_STATE(XInputSetStateStub)
{
    return ERROR_DEVICE_NOT_CONNECTED;
}
global_variable x_input_set_state *XInputSetState_ = XInputSetStateStub;

#define XInputGetState XInputGetState_
#define XInputSetState XInputSetState_

#define DIRECT_SOUND_CREATE(name) HRESULT WINAPI name(LPCGUID pcGuidDevice, LPDIRECTSOUND *ppDS, LPUNKNOWN pUnkOuter);
typedef DIRECT_SOUND_CREATE(direct_sound_create);

global_variable bool GlobalRunning = true;
global_variable win32_offscreen_buffer GlobalBackBuffer;
global_variable int GlobalXOffset;
global_variable int GlobalYOffset;
global_variable LPDIRECTSOUNDBUFFER GlobalSoundBuffer;

internal void Win32ProcessButtonState(game_button_state *NewButtonState, game_button_state *OldButtonState, DWORD Buttons, DWORD ButtonBit)
{

    NewButtonState->HalfTransitionCount = (OldButtonState->EndedDown == NewButtonState->EndedDown) ? 0 : 1;
    NewButtonState->EndedDown = Buttons & ButtonBit;
}

internal void Win32InitDSound(HWND Window, int32 SamplesPerSecond, int32 BufferSize)
{
    HMODULE Library = LoadLibraryA("dsound.dll");
    if (Library)
    {
        direct_sound_create *DirectSoundCreate = (direct_sound_create *)GetProcAddress(Library, "DirectSoundCreate");

        LPDIRECTSOUND DirectSound;

        if (SUCCEEDED(DirectSoundCreate(0, &DirectSound, 0)))
        {
            if (SUCCEEDED(DirectSound->SetCooperativeLevel(Window, DSSCL_PRIORITY)))
            {
                OutputDebugStringA("Set cooperative level ok\n");
            }
            else
            {
                // TODO: logging
            }

            WAVEFORMATEX WaveFormat = {};
            WaveFormat.wFormatTag = WAVE_FORMAT_PCM;
            WaveFormat.nChannels = 2;
            WaveFormat.nSamplesPerSec = SamplesPerSecond;
            WaveFormat.wBitsPerSample = 16;
            WaveFormat.nBlockAlign = WaveFormat.nChannels * WaveFormat.wBitsPerSample / 8;
            WaveFormat.nAvgBytesPerSec = WaveFormat.nSamplesPerSec * WaveFormat.nBlockAlign;

            {
                DSBUFFERDESC BufferDesc = {};
                BufferDesc.dwSize = sizeof(BufferDesc);
                BufferDesc.dwFlags = DSBCAPS_PRIMARYBUFFER;
                LPDIRECTSOUNDBUFFER PrimaryBuffer;
                if (SUCCEEDED(DirectSound->CreateSoundBuffer(&BufferDesc, &PrimaryBuffer, 0)))
                {
                    OutputDebugStringA("Create primary buffer ok\n");
                    if (SUCCEEDED(PrimaryBuffer->SetFormat(&WaveFormat)))
                    {
                        OutputDebugStringA("Primary buffer set format ok\n");
                    }
                    else
                    {
                        // TODO: logging
                    }
                }
            }

            {
                DSBUFFERDESC BufferDesc = {};
                BufferDesc.dwSize = sizeof(BufferDesc);
                BufferDesc.dwFlags = 0;
                BufferDesc.dwBufferBytes = BufferSize;
                BufferDesc.lpwfxFormat = &WaveFormat;
                if (SUCCEEDED(DirectSound->CreateSoundBuffer(&BufferDesc, &GlobalSoundBuffer, 0)))
                {
                    OutputDebugStringA("Secondary buffer created\n");
                }
                else
                {
                    // TODO: logging
                }
            }
        }
        else
        {
            // TODO: logging
        }
    }
    else
    {
        // TODO: logging
    }
}

struct win32_sound_output
{
    int32 SamplesPerSecond;
    int ToneHz;
    int16 ToneVolume;
    uint32 RunningSampleIndex;
    int WavePeriod;
    int32 BytesPerSample;
    int32 SecondaryBufferSize;
    real32 tSine;
    int32 LatencySampleCount;
};

internal void Win32ClearBuffer(win32_sound_output *SoundOutput)
{
    VOID *Region1;
    DWORD Region1Size;
    VOID *Region2;
    DWORD Region2Size;

    if (SUCCEEDED(GlobalSoundBuffer->Lock(
            0,
            SoundOutput->SecondaryBufferSize,
            &Region1, &Region1Size,
            &Region2, &Region2Size,
            0)))
    {

        uint8 *DestSample = (uint8 *)Region1;
        for (DWORD ByteIndex = 0; ByteIndex < Region1Size; ByteIndex++)
        {
            *DestSample++ = 0;
        }

        DestSample = (uint8 *)Region2;
        for (DWORD ByteIndex = 0; ByteIndex < Region2Size; ByteIndex++)
        {
            *DestSample++ = 0;
        }

        GlobalSoundBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);
    }
}

internal void
Win32FillSoundBuffer(win32_sound_output *SoundOutput, DWORD BytesToLock, DWORD BytesToWrite, game_sound_output_buffer *SourceBuffer)
{
    VOID *Region1;
    DWORD Region1Size;
    VOID *Region2;
    DWORD Region2Size;

    if (SUCCEEDED(GlobalSoundBuffer->Lock(
            BytesToLock,
            BytesToWrite,
            &Region1, &Region1Size,
            &Region2, &Region2Size,
            0)))
    {
        DWORD Region1SampleCount = Region1Size / SoundOutput->BytesPerSample;
        int16 *DestSample = (int16 *)Region1;
        int16 *SourceSample = SourceBuffer->Samples;
        for (DWORD SampleIndex = 0; SampleIndex < Region1SampleCount; ++SampleIndex)
        {
            *DestSample++ = *SourceSample++;
            *DestSample++ = *SourceSample++;
            ++SoundOutput->RunningSampleIndex;
        }

        DWORD Region2SampleCount = Region2Size / SoundOutput->BytesPerSample;
        DestSample = (int16 *)Region2;
        for (DWORD SampleIndex = 0; SampleIndex < Region2SampleCount; ++SampleIndex)
        {
            *DestSample++ = *SourceSample++;
            *DestSample++ = *SourceSample++;
            ++SoundOutput->RunningSampleIndex;
        }

        GlobalSoundBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);
    }
}

internal void
Win32LoadXInput()
{
    HMODULE Library = LoadLibraryA("xinput1_4.dll");
    if (Library)
    {
        XInputGetState = (x_input_get_state *)GetProcAddress(Library, "XInputGetState");
        XInputSetState = (x_input_set_state *)GetProcAddress(Library, "XInputSetState");
    }
}

internal win32_window_dimension
Win32GetWindowDimension(HWND Window)
{
    win32_window_dimension Result;
    RECT ClientRect;
    GetClientRect(Window, &ClientRect);
    Result.Width = ClientRect.right - ClientRect.left;
    Result.Height = ClientRect.bottom - ClientRect.top;
    return Result;
}

internal void
Win32ResizeDIBSection(win32_offscreen_buffer *Buffer, int Width, int Height)
{
    if (Buffer->Memory)
    {
        VirtualFree(Buffer->Memory, 0, MEM_RELEASE);
    }

    Buffer->Width = Width;
    Buffer->Height = Height;
    Buffer->BytesPerPixel = 4;

    Buffer->Info.bmiHeader.biSize = sizeof(Buffer->Info.bmiHeader);
    Buffer->Info.bmiHeader.biWidth = Buffer->Width;
    Buffer->Info.bmiHeader.biHeight = -Buffer->Height;
    Buffer->Info.bmiHeader.biPlanes = 1;
    Buffer->Info.bmiHeader.biBitCount = Buffer->BytesPerPixel * 8;
    Buffer->Info.bmiHeader.biCompression = BI_RGB;

    int BitmapSize = Buffer->Width * Buffer->Height * Buffer->BytesPerPixel;

    Buffer->Memory = VirtualAlloc(0, BitmapSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
}

internal void
Win32DisplayBufferInWindow(
    win32_offscreen_buffer *Buffer,
    HDC DeviceContext,
    int WindowWidth,
    int WindowHeight)
{
    StretchDIBits(
        DeviceContext,
        0, 0, WindowWidth, WindowHeight,
        0, 0, Buffer->Width, Buffer->Height,
        Buffer->Memory,
        &Buffer->Info,
        DIB_RGB_COLORS,
        SRCCOPY);
}

internal LRESULT CALLBACK
Win32MainWindowCallback(
    HWND Window,
    UINT Message,
    WPARAM WParam,
    LPARAM LParam)
{
    LRESULT Result = 0;
    switch (Message)
    {
    case WM_ACTIVATEAPP:
    {
        OutputDebugStringA("WM_ACTIVATEAPP\n");
    }
    break;

    case WM_CLOSE:
    {
        GlobalRunning = false;
        OutputDebugStringA("WM_CLOSE\n");
    }
    break;

    case WM_DESTROY:
    {
        OutputDebugStringA("WM_DESTROY\n");
    }
    break;

    case WM_SYSKEYUP:
    case WM_SYSKEYDOWN:
    case WM_KEYUP:
    case WM_KEYDOWN:
    {
        uint64 VKCode = WParam;
        bool32 WasDown = LParam & (1 << 30);
        bool32 IsDown = LParam & (1 << 31);

        int diff = 20;

        switch (VKCode)
        {
        case VK_F4:
        {
            bool32 IsAltDown = LParam & (1 << 29);
            if (IsAltDown)
            {
                GlobalRunning = false;
            }
        }
        break;

        case VK_UP:
        {
            GlobalYOffset += diff;
        }
        break;

        case VK_DOWN:
        {
            GlobalYOffset -= diff;
        }
        break;

        case VK_LEFT:
        {
            GlobalXOffset -= diff;
        }
        break;

        case VK_RIGHT:
        {
            GlobalXOffset += diff;
        }
        break;
        }
    }
    break;

    case WM_PAINT:
    {
        PAINTSTRUCT PaintStruct = {};
        HDC DeviceContext = BeginPaint(Window, &PaintStruct);
        win32_window_dimension Dimension = Win32GetWindowDimension(Window);
        Win32DisplayBufferInWindow(
            &GlobalBackBuffer,
            DeviceContext,
            Dimension.Width,
            Dimension.Height);
        EndPaint(Window, &PaintStruct);
    }
    break;

    case WM_SIZE:
    {
        OutputDebugStringA("WM_SIZE\n");
    }
    break;

    default:
    {
        Result = DefWindowProcA(Window, Message, WParam, LParam);
    }
    break;
    }

    return Result;
}

int CALLBACK
WinMain(
    HINSTANCE Instance,
    HINSTANCE PrevInstance,
    LPSTR CmdLine,
    int ShowCmd)
{
    LARGE_INTEGER CounterPerSecond;
    QueryPerformanceFrequency(&CounterPerSecond);

    Win32LoadXInput();

    WNDCLASS WindowClass = {};

    Win32ResizeDIBSection(&GlobalBackBuffer, 1280, 720);

    WindowClass.style = CS_HREDRAW | CS_VREDRAW;
    WindowClass.lpfnWndProc = Win32MainWindowCallback;
    WindowClass.hInstance = Instance;
    // WindowClass.hIcon = ;
    WindowClass.lpszClassName = "HandmadeHeroWindowClass";

    if (RegisterClassA(&WindowClass))
    {
        HWND Window = CreateWindowExA(
            0,
            "HandmadeHeroWindowClass",
            "Handmade Hero",
            WS_OVERLAPPEDWINDOW | WS_VISIBLE,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            0,
            0,
            Instance,
            0);

        if (Window)
        {
            HDC DeviceContext = GetDC(Window);

            int32 XOffset = 0;
            int32 YOffset = 0;

            win32_sound_output SoundOutput = {};

            SoundOutput.SamplesPerSecond = 48000;
            SoundOutput.ToneHz = 256;
            SoundOutput.ToneVolume = 3000;
            SoundOutput.WavePeriod = SoundOutput.SamplesPerSecond / SoundOutput.ToneHz;
            SoundOutput.BytesPerSample = sizeof(int16) * 2;
            SoundOutput.SecondaryBufferSize = SoundOutput.SamplesPerSecond * SoundOutput.BytesPerSample;
            SoundOutput.LatencySampleCount = SoundOutput.SamplesPerSecond / 15;
            Win32InitDSound(Window, SoundOutput.SamplesPerSecond, SoundOutput.SecondaryBufferSize);
            Win32ClearBuffer(&SoundOutput);
            GlobalSoundBuffer->Play(0, 0, DSBPLAY_LOOPING);

            GlobalRunning = true;

            int16 *Samples = (int16 *)VirtualAlloc(0, SoundOutput.SecondaryBufferSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

            game_input Input[2] = {};
            game_input *OldInput = &Input[0];
            game_input *NewInput = &Input[1];

            LARGE_INTEGER LastCounter;
            QueryPerformanceCounter(&LastCounter);
            uint64 LastCycleCounter = __rdtsc();
            while (GlobalRunning)
            {
                MSG Message = {};

                while (PeekMessage(&Message, 0, 0, 0, PM_REMOVE))
                {
                    if (Message.message == WM_QUIT)
                    {
                        GlobalRunning = false;
                    }

                    TranslateMessage(&Message);
                    DispatchMessage(&Message);
                }

                int32 ControllerCount = XUSER_MAX_COUNT;
                if (ControllerCount > ArrayCount(NewInput->Controllers))
                {
                    ControllerCount = ArrayCount(NewInput->Controllers);
                }

                for (DWORD ControllerIndex = 0; ControllerIndex < ControllerCount; ControllerIndex++)
                {
                    XINPUT_STATE ControllerState;

                    game_controller_input *OldController = &OldInput->Controllers[ControllerIndex];
                    game_controller_input *NewController = &NewInput->Controllers[ControllerIndex];

                    ZeroMemory(&ControllerState, sizeof(XINPUT_STATE));
                    if (XInputGetState(ControllerIndex, &ControllerState) == ERROR_SUCCESS)
                    {
                        XINPUT_GAMEPAD *Pad = &ControllerState.Gamepad;

                        NewController->IsAnalog = true;
                        NewController->StartX = OldController->EndX;
                        NewController->StartY = OldController->EndY;

                        real32 X;
                        if (Pad->sThumbLX < 0)
                        {
                            X = (real32)Pad->sThumbLX / 32767.0f;
                        }
                        else
                        {
                            X = (real32)Pad->sThumbLX / 32767.0f;
                        }
                        NewController->MinX = NewController->MaxX = NewController->EndX = X;

                        real32 Y;
                        if (Pad->sThumbLY < 0)
                        {
                            Y = (real32)Pad->sThumbLY / 32767.0f;
                        }
                        else
                        {
                            Y = (real32)Pad->sThumbLY / 32767.0f;
                        }

                        NewController->MinY = NewController->MaxY = NewController->EndY = Y;

                        Win32ProcessButtonState(&NewController->Up, &OldController->Up, Pad->wButtons, XINPUT_GAMEPAD_A);
                        Win32ProcessButtonState(&NewController->Down, &OldController->Up, Pad->wButtons, XINPUT_GAMEPAD_B);
                        Win32ProcessButtonState(&NewController->Left, &OldController->Up, Pad->wButtons, XINPUT_GAMEPAD_X);
                        Win32ProcessButtonState(&NewController->Right, &OldController->Up, Pad->wButtons, XINPUT_GAMEPAD_Y);
                        Win32ProcessButtonState(&NewController->ShoulderLeft, &OldController->Up, Pad->wButtons, XINPUT_GAMEPAD_LEFT_SHOULDER);
                        Win32ProcessButtonState(&NewController->ShoulderRight, &OldController->Up, Pad->wButtons, XINPUT_GAMEPAD_RIGHT_SHOULDER);

                        int16 StickX = Pad->sThumbLX;
                        int16 StickY = Pad->sThumbLY;
                    }
                    else
                    {
                        // Controller is not connected
                    }
                }

                DWORD BytesToLock;
                DWORD TargetCursor;
                DWORD BytesToWrite;
                DWORD PlayCursor;
                DWORD WriteCursor;
                bool32 SoundIsValid = false;
                if (SUCCEEDED(GlobalSoundBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor)))
                {
                    BytesToLock = (SoundOutput.RunningSampleIndex * SoundOutput.BytesPerSample) % SoundOutput.SecondaryBufferSize;

                    TargetCursor = (PlayCursor + (SoundOutput.LatencySampleCount * SoundOutput.BytesPerSample)) % SoundOutput.SecondaryBufferSize;

                    if (BytesToLock > TargetCursor)
                    {
                        BytesToWrite = SoundOutput.SecondaryBufferSize - BytesToLock;
                        BytesToWrite += TargetCursor;
                    }
                    else
                    {
                        BytesToWrite = TargetCursor - BytesToLock;
                    }

                    SoundIsValid = true;
                }

                int16 Samples[48000 * 2];
                game_sound_output_buffer SoundBuffer = {};
                SoundBuffer.SamplesPerSecond = SoundOutput.SamplesPerSecond;
                SoundBuffer.SampleCount = BytesToWrite / SoundOutput.BytesPerSample;
                SoundBuffer.Samples = Samples;

                game_offscreen_buffer Buffer = {};
                Buffer.Memory = GlobalBackBuffer.Memory;
                Buffer.BytesPerPixel = GlobalBackBuffer.BytesPerPixel;
                Buffer.Width = GlobalBackBuffer.Width;
                Buffer.Height = GlobalBackBuffer.Height;
                GameUpdateAndRender(NewInput, &Buffer, &SoundBuffer);

                if (SoundIsValid)
                {
                    Win32FillSoundBuffer(&SoundOutput, BytesToLock, BytesToWrite, &SoundBuffer);
                }

                win32_window_dimension Dimension = Win32GetWindowDimension(Window);
                Win32DisplayBufferInWindow(&GlobalBackBuffer, DeviceContext, Dimension.Width, Dimension.Height);

                game_input *temp = OldInput;
                OldInput = NewInput;
                NewInput = temp;

                uint64 EndCycleCounter = __rdtsc();
                LARGE_INTEGER EndCounter;
                QueryPerformanceCounter(&EndCounter);

                uint64 CycleElapsed = EndCycleCounter - LastCycleCounter;
                int64 CounterElapsed = EndCounter.QuadPart - LastCounter.QuadPart;

                real32 MSPerFrame = 1000.0f * (real32)CounterElapsed / (real32)CounterPerSecond.QuadPart;
                real32 FPS = (real32)CounterPerSecond.QuadPart / (real32)CounterElapsed;
                real32 MCPF = (real32)CycleElapsed / (1000.0f * 1000.0f);
#if 0
                char OutputBuffer[256];
                sprintf(OutputBuffer, "ms/f: %.2f,  fps: %.2f,  mc/f: %.2f\n", MSPerFrame, FPS, MCPF);
                OutputDebugStringA(OutputBuffer);
#endif

                LastCounter = EndCounter;
                LastCycleCounter = EndCycleCounter;
            }
        }
        else
        {
            // TODO: Logging
        }
    }
    else
    {
        // TODO: Logging
    }

    return 0;
}