use std::{
    ffi::c_void,
    mem::{size_of, zeroed},
    ptr::null_mut,
};

use windows::{
    core::*,
    Win32::{
        Foundation::{BOOL, ERROR_SUCCESS, HWND, LPARAM, LRESULT, POINT, RECT, WPARAM},
        Graphics::Gdi::{
            BeginPaint, EndPaint, GetDC, ReleaseDC, StretchDIBits, BITMAPINFO, BITMAPINFOHEADER,
            BI_RGB, DIB_RGB_COLORS, HBRUSH, HDC, PAINTSTRUCT, RGBQUAD, SRCCOPY,
        },
        Media::Audio::DirectSound::{
            DirectSoundCreate, IDirectSound, DSBUFFERDESC, DSSCL_PRIORITY,
        },
        System::{
            Diagnostics::Debug::OutputDebugStringA,
            LibraryLoader::GetModuleHandleA,
            Memory::{VirtualAlloc, VirtualFree, MEM_COMMIT, MEM_RELEASE, PAGE_READWRITE},
        },
        UI::{
            Input::{
                KeyboardAndMouse::{
                    VIRTUAL_KEY, VK_DOWN, VK_ESCAPE, VK_LEFT, VK_RIGHT, VK_SPACE, VK_UP,
                },
                XboxController::{
                    XInputGetState, XINPUT_GAMEPAD, XINPUT_GAMEPAD_A, XINPUT_GAMEPAD_B,
                    XINPUT_GAMEPAD_BACK, XINPUT_GAMEPAD_DPAD_DOWN, XINPUT_GAMEPAD_DPAD_LEFT,
                    XINPUT_GAMEPAD_DPAD_RIGHT, XINPUT_GAMEPAD_DPAD_UP,
                    XINPUT_GAMEPAD_LEFT_SHOULDER, XINPUT_GAMEPAD_RIGHT_SHOULDER, XINPUT_GAMEPAD_X,
                    XINPUT_GAMEPAD_Y, XINPUT_STATE, XUSER_MAX_COUNT,
                },
            },
            WindowsAndMessaging::{
                CreateWindowExA, DefWindowProcA, DispatchMessageA, GetClientRect, PeekMessageA,
                RegisterClassA, TranslateMessage, CS_HREDRAW, CS_VREDRAW, CW_USEDEFAULT, HCURSOR,
                HICON, HMENU, MSG, PM_REMOVE, WINDOW_EX_STYLE, WM_ACTIVATEAPP, WM_CLOSE,
                WM_DESTROY, WM_KEYDOWN, WM_KEYUP, WM_PAINT, WM_SIZE, WM_SYSKEYDOWN, WM_SYSKEYUP,
                WNDCLASSA, WS_OVERLAPPEDWINDOW, WS_VISIBLE,
            },
        },
    },
};

struct Win32OffScreenBuffer {
    memory: *mut c_void,
    info: BITMAPINFO,
    bytes_per_pixel: i32,
    width: i32,
    height: i32,
}

struct Win32WindowDimensions {
    width: i32,
    height: i32,
}

static mut RUNNING: bool = true;
static mut GLOBAL_BACKBUFFER: Win32OffScreenBuffer = Win32OffScreenBuffer {
    memory: 0 as *mut c_void,
    info: BITMAPINFO {
        bmiHeader: BITMAPINFOHEADER {
            biSize: 0,
            biWidth: 0,
            biHeight: 0,
            biPlanes: 1,
            biBitCount: 32,
            biCompression: BI_RGB as u32,
            biSizeImage: 0,
            biXPelsPerMeter: 0,
            biYPelsPerMeter: 0,
            biClrUsed: 0,
            biClrImportant: 0,
        },
        bmiColors: [RGBQUAD {
            rgbBlue: 0,
            rgbGreen: 0,
            rgbRed: 0,
            rgbReserved: 0,
        }],
    },
    bytes_per_pixel: 4,
    width: 0,
    height: 0,
};

fn main() {
    unsafe {
        winmain();
    }
}

unsafe extern "system" fn win32_main_window_callback(
    window: HWND,
    message: u32,
    wparam: WPARAM,
    lparam: LPARAM,
) -> LRESULT {
    let mut result = LRESULT(0);
    match message {
        WM_SIZE => OutputDebugStringA(PCSTR(b"WM_ACTIVATEAPP\n".as_ptr())),
        WM_CLOSE => {
            RUNNING = false;
        }

        WM_ACTIVATEAPP => {
            OutputDebugStringA(PCSTR(b"WM_ACTIVATEAPP\n".as_ptr()));
        }
        WM_DESTROY => {
            RUNNING = false;
        }

        WM_PAINT => {
            let mut paint: PAINTSTRUCT = zeroed::<PAINTSTRUCT>();
            let device_context = BeginPaint(window, &mut paint);
            let dimension = win32_get_window_dimensions(window);

            win32_update_window(
                device_context,
                dimension.width,
                dimension.height,
                &GLOBAL_BACKBUFFER,
            );

            EndPaint(window, &paint);
        }
        WM_SYSKEYDOWN | WM_SYSKEYUP | WM_KEYDOWN | WM_KEYUP => {
            let vk_code = wparam.0 as i32;
            let was_down: bool = (lparam.0 & (1 << 30)) != 0;
            let is_down: bool = (lparam.0 & (1 << 31)) == 0;
            if was_down != is_down {
                match vk_code as u8 as char {
                    'W' => {
                        println!("working W");
                    }
                    'A' => {}
                    'S' => {}
                    'D' => {}
                    'Q' => {}
                    'E' => {}
                    _ => match VIRTUAL_KEY(vk_code as u16) {
                        VK_UP => {
                            println!("working up!")
                        }
                        VK_LEFT => {}

                        VK_DOWN => {}
                        VK_RIGHT => {}
                        VK_ESCAPE => {}
                        VK_SPACE => {}
                        _ => {}
                    },
                }
            }
        }
        _ => {
            result = DefWindowProcA(window, message, wparam, lparam);
        }
    }

    return result;
}

pub unsafe extern "system" fn winmain() {
    let instance = GetModuleHandleA(None).unwrap();

    let window_class = WNDCLASSA {
        style: CS_HREDRAW | CS_VREDRAW,
        lpfnWndProc: Some(win32_main_window_callback),
        hInstance: instance,
        lpszClassName: PCSTR(b"HandmadeHeroWindowClass".as_ptr()),
        cbClsExtra: 0,
        cbWndExtra: 0,
        hIcon: HICON(0),
        hCursor: HCURSOR(0),
        hbrBackground: HBRUSH(0),
        lpszMenuName: PCSTR(null_mut()),
    };

    match RegisterClassA(&window_class) {
        _atom => {
            let window = CreateWindowExA(
                WINDOW_EX_STYLE(0),
                PCSTR(b"HandmadeHeroWindowClass".as_ptr()),
                PCSTR(b"Handmade Hero".as_ptr()),
                WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                CW_USEDEFAULT,
                CW_USEDEFAULT,
                CW_USEDEFAULT,
                CW_USEDEFAULT,
                HWND(0),
                HMENU(0),
                instance,
                std::ptr::null(),
            );

            if window != HWND(0) {
                let mut x_offset = 0;
                let mut y_offset = 0;

                win32_resize_dibsection(&mut GLOBAL_BACKBUFFER, 1280, 720);

                while RUNNING {
                    let mut message: MSG = MSG {
                        hwnd: HWND(0),
                        message: 0,
                        wParam: WPARAM(0),
                        lParam: LPARAM(0),
                        time: 0,
                        pt: POINT { x: 0, y: 0 },
                    };

                    while PeekMessageA(&mut message, HWND(0), 0, 0, PM_REMOVE) != BOOL(0) {
                        TranslateMessage(&message);
                        DispatchMessageA(&message);
                    }

                    for controller_index in 0..XUSER_MAX_COUNT {
                        let mut controller_state: XINPUT_STATE = zeroed();
                        if XInputGetState(controller_index, &mut controller_state)
                            == ERROR_SUCCESS.0
                        {
                            let pad: XINPUT_GAMEPAD = controller_state.Gamepad;

                            let _up = pad.wButtons & XINPUT_GAMEPAD_DPAD_UP as u16;
                            let _down = pad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN as u16;
                            let _left = pad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT as u16;
                            let _right = pad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT as u16;

                            let _back = pad.wButtons & XINPUT_GAMEPAD_BACK as u16;
                            let _left_shoulder = pad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER as u16;
                            let _right_shoulder =
                                pad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER as u16;
                            let _a_button = pad.wButtons & XINPUT_GAMEPAD_A as u16;
                            let _b_button = pad.wButtons & XINPUT_GAMEPAD_B as u16;
                            let _x_button = pad.wButtons & XINPUT_GAMEPAD_X as u16;
                            let _y_button = pad.wButtons & XINPUT_GAMEPAD_Y as u16;

                            let _stick_x = pad.sThumbLX;
                            let _stick_y = pad.sThumbLY;
                        }
                    }

                    render_weird_gradient(&GLOBAL_BACKBUFFER, x_offset, y_offset);
                    let device_context: HDC = GetDC(window);
                    let dimensions: Win32WindowDimensions = win32_get_window_dimensions(window);
                    win32_update_window(
                        device_context,
                        dimensions.width,
                        dimensions.height,
                        &GLOBAL_BACKBUFFER,
                    );
                    ReleaseDC(window, device_context);

                    x_offset += 1;
                    y_offset += 2;
                }
            } else {
            }
        }
    }
}

fn win32_update_window(
    device_context: HDC,
    window_width: i32,
    window_height: i32,
    buffer: &Win32OffScreenBuffer,
) {
    unsafe {
        StretchDIBits(
            device_context,
            0,
            0,
            window_width,
            window_height,
            0,
            0,
            buffer.width,
            buffer.height,
            buffer.memory,
            &buffer.info,
            DIB_RGB_COLORS,
            SRCCOPY,
        );
    }
}

fn win32_get_window_dimensions(window: HWND) -> Win32WindowDimensions {
    unsafe {
        let mut client_rect = zeroed::<RECT>();
        GetClientRect(window, &mut client_rect);

        Win32WindowDimensions {
            width: client_rect.right - client_rect.left,
            height: client_rect.bottom - client_rect.top,
        }
    }
}

fn render_weird_gradient(buffer: &Win32OffScreenBuffer, x_offset: i32, y_offset: i32) {
    unsafe {
        let mut row = buffer.memory as *mut u8;
        for y in 0..buffer.height {
            let mut pixel = row as *mut [u8; 4]; //array of 4, u8s
            for x in 0..buffer.width {
                *pixel = [(x + x_offset) as u8, (y + y_offset) as u8, 0, 0];
                pixel = pixel.offset(1); // adds sizeof(pixel), 4
            }
            row = row.offset((buffer.width * buffer.bytes_per_pixel) as isize);
        }
    }
}

fn win32_init_dsound(window: HWND) {
    unsafe {
        let mut direct_sound = Some(zeroed::<IDirectSound>());

        if DirectSoundCreate(zeroed(), &mut direct_sound, test).is_ok() {
            if direct_sound
                .unwrap()
                .SetCooperativeLevel(window, DSSCL_PRIORITY)
                .is_ok()
            {}
        }
    }
}

fn win32_resize_dibsection(buffer: &mut Win32OffScreenBuffer, width: i32, height: i32) {
    if !buffer.memory.is_null() {
        unsafe {
            VirtualFree(buffer.memory, 0, MEM_RELEASE);
        }
    }

    buffer.width = width;
    buffer.height = height;
    buffer.bytes_per_pixel = 4;

    buffer.info.bmiHeader.biSize = size_of::<BITMAPINFOHEADER>() as u32;
    buffer.info.bmiHeader.biWidth = buffer.width;
    buffer.info.bmiHeader.biHeight = -buffer.height;
    buffer.info.bmiHeader.biPlanes = 1;
    buffer.info.bmiHeader.biBitCount = (buffer.bytes_per_pixel * 8) as u16;
    buffer.info.bmiHeader.biCompression = BI_RGB as u32;

    let bitmap_size = buffer.width * buffer.height * buffer.bytes_per_pixel;
    buffer.memory = unsafe {
        VirtualAlloc(
            0 as *mut c_void,
            bitmap_size as usize,
            MEM_COMMIT,
            PAGE_READWRITE,
        )
    };
}
