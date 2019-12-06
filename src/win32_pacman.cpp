// @todo improve input system


#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h> // @note excluded in lean and mean, used for timeBeginPeriod to set scheduler granularity

#include <xinput.h>

#include <stdio.h>

#include "iml_general.h"
#include "iml_types.h"


#define WIDTH  800
#define HEIGHT 600


struct Win32_Offscreen_Buffer {
    BITMAPINFO info;
    void *memory;
    int width;
    int height;
    int pitch;
    int bytes_per_pixel;
};

struct Win32_Window_Dimension {
    int width;
    int height;
};

struct Vector2 {
    int x;
    int y;
};
typedef Vector2 v2;

struct Vector3 {
    union {
        struct {
            u32 x;
            u32 y;
            u32 z;
        };
        struct {
            u32 r;
            u32 g;
            u32 b;
        };
    };
};
typedef Vector3 v3;

internal Vector3
rgb(u32 r, u32 g, u32 b) {
    Vector3 rgb { r, g, b };
    return rgb;
}

struct Game_State {
};

struct Game_Button_State {
    b32 ended_down;
    b32 allow_press;
    int half_transition_count;
};

struct Game_Controller_Input {
    b32 is_connected;
    b32 is_analog;
    f32 stick_average_x;
    f32 stick_average_y;
    
    union {
        Game_Button_State buttons[12];
        
        struct {
            Game_Button_State move_up;
            Game_Button_State move_down;
            Game_Button_State move_left;
            Game_Button_State move_right;
            
            Game_Button_State action_up;
            Game_Button_State action_down;
            Game_Button_State action_left;
            Game_Button_State action_right;
            
            Game_Button_State left_shoulder;
            Game_Button_State right_shoulder;
            
            Game_Button_State start;
            Game_Button_State back;
            
            // @note all buttons must be added to the struct above this line!!!
            Game_Button_State _terminator_;
        };
    };
};

struct Game_Input {
    Game_Controller_Input controllers[5];
};

inline Game_Controller_Input *
get_controller(Game_Input *input, u32 controller_index) {
    assert(controller_index < array_count(input->controllers));
    return &input->controllers[controller_index];
}


global Win32_Offscreen_Buffer global_backbuffer;
global b32 global_running;
global s64 global_performance_count_frequency;
global WINDOWPLACEMENT global_window_position = { sizeof(global_window_position) };


// @note xinput_get_state
#define XINPUT_GET_STATE_SIG(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE *pState)
typedef XINPUT_GET_STATE_SIG(XInput_Get_State_Sig);
XINPUT_GET_STATE_SIG(xinput_get_state_stub) {
    return ERROR_DEVICE_NOT_CONNECTED;
}
global XInput_Get_State_Sig *xinput_get_state_ = xinput_get_state_stub;
#define xinput_get_state xinput_get_state_

// @note xinput_set_state
#define XINPUT_SET_STATE_SIG(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION *pVibration)
typedef XINPUT_SET_STATE_SIG(XInput_Set_State_Sig);
XINPUT_SET_STATE_SIG(xinput_set_state_stub) {
    return ERROR_DEVICE_NOT_CONNECTED;
}
global XInput_Set_State_Sig *xinput_set_state_ = xinput_set_state_stub;
#define xinput_set_state xinput_set_state_

internal void
win32_load_xinput() {
    HMODULE xinput_library = LoadLibraryA("xinput1_4.dll");
    if(!xinput_library) {
        xinput_library = LoadLibraryA("xinput9_1_0.dll");
    }
    if(!xinput_library) {
        xinput_library = LoadLibraryA("xinput1_3.dll");
    }
    
    if (!xinput_library)  return;
    
    xinput_get_state = (XInput_Get_State_Sig *)GetProcAddress(xinput_library, "XInputGetState");
    if (!xinput_get_state) {
        xinput_get_state = xinput_get_state_stub;
    }
    
    xinput_set_state = (XInput_Set_State_Sig *)GetProcAddress(xinput_library, "XInputSetState");
    if (!xinput_set_state)  {
        xinput_set_state = xinput_set_state_stub;
    }
}

internal void
win32_resize_dib_section(Win32_Offscreen_Buffer *buffer, int width, int height) {
    if (buffer->memory) {
        VirtualFree(buffer->memory, 0, MEM_RELEASE);
    }
    
    buffer->width  = width;
    buffer->height = height;
    
    buffer->info.bmiHeader.biSize = sizeof(buffer->info.bmiHeader);
    buffer->info.bmiHeader.biWidth = buffer->width;
    buffer->info.bmiHeader.biHeight = -buffer->height;
    buffer->info.bmiHeader.biPlanes = 1;
    buffer->info.bmiHeader.biBitCount = 32;
    buffer->info.bmiHeader.biCompression = BI_RGB;
    
    int bytes_per_pixel = 4;
    int bitmap_memory_size = (buffer->width * buffer->height) * bytes_per_pixel;
    buffer->memory = VirtualAlloc(0, bitmap_memory_size, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
    buffer->pitch = buffer->width * bytes_per_pixel;
    buffer->bytes_per_pixel = bytes_per_pixel;
}

internal Win32_Window_Dimension
win32_get_window_dimension(HWND window) {
    Win32_Window_Dimension result;
    
    RECT client_rect;
    GetClientRect(window, &client_rect);
    result.width = client_rect.right - client_rect.left;
    result.height = client_rect.bottom - client_rect.top;
    
    return result;
}

internal void
win32_display_buffer_in_window(Win32_Offscreen_Buffer *buffer, HDC device_context, int window_width, int window_height) {
    // @todo better scaling, centering, black bars, ...
    
    int buffer_width = WIDTH;
    int buffer_height = HEIGHT;
    
    f32 height_scale = (f32)window_height / (f32)buffer_height;
    int new_width = (f32)buffer_width * height_scale;
    int offset_x = (window_width - new_width) / 2;
    if (new_width <= buffer_width)  {
        offset_x = 0;
        new_width = window_width;
    }
    else {
        PatBlt(device_context, 0, 0, offset_x-2, window_height, BLACKNESS);
        PatBlt(device_context, offset_x-2, 0, offset_x, window_height,  WHITENESS);
        PatBlt(device_context, offset_x+new_width, 0, offset_x+buffer_width+2, window_height, WHITENESS);
        PatBlt(device_context, offset_x+new_width+2, 0, window_width, window_height, BLACKNESS);
    }
    
    StretchDIBits(device_context,
                  offset_x, 0, new_width, window_height,
                  0, 0, buffer->width, buffer->height,
                  buffer->memory, &buffer->info,
                  DIB_RGB_COLORS, SRCCOPY);
}

internal void
toggle_fullscreen(HWND window) {
    // @note: This follows Raymond Chen's prescription for fullscreen toggling, see:
    //        https://devblogs.microsoft.com/oldnewthing/20100412-00/?p=14353
    
    DWORD style = GetWindowLong(window, GWL_STYLE);
    if (style & WS_OVERLAPPEDWINDOW) {
        MONITORINFO monitor_info = { sizeof(monitor_info) };
        if (GetWindowPlacement(window, &global_window_position) &&
            GetMonitorInfo(MonitorFromWindow(window, MONITOR_DEFAULTTOPRIMARY), &monitor_info)) {
            SetWindowLong(window, GWL_STYLE, style & ~WS_OVERLAPPEDWINDOW);
            SetWindowPos(window, HWND_TOP,
                         monitor_info.rcMonitor.left, monitor_info.rcMonitor.top,
                         monitor_info.rcMonitor.right - monitor_info.rcMonitor.left,
                         monitor_info.rcMonitor.bottom - monitor_info.rcMonitor.top,
                         SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        }
    }
    else {
        SetWindowLong(window, GWL_STYLE, style | WS_OVERLAPPEDWINDOW);
        SetWindowPlacement(window, &global_window_position);
        SetWindowPos(window, NULL, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    }
}

LRESULT CALLBACK
win32_main_window_callback(HWND window,
                           UINT message,
                           WPARAM wparam,
                           LPARAM lparam) {
    LRESULT result = 0;
    
    switch (message) {
        case WM_ACTIVATEAPP: {
            OutputDebugStringA("WM_ACTIVATEAPP\n");
        } break;
        
        case WM_DESTROY:
        case WM_CLOSE: {
            global_running = false;
        } break;
        
        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
        case WM_KEYDOWN:
        case WM_KEYUP: {
            assert(!"Keyboard input came in through a non-dispatch message!");
        } break;
        
        case WM_PAINT: {
            PAINTSTRUCT paint;
            HDC device_context = BeginPaint(window, &paint);
            int x = paint.rcPaint.left;
            int y = paint.rcPaint.top;
            int width  = paint.rcPaint.right  - paint.rcPaint.left;
            int height = paint.rcPaint.bottom - paint.rcPaint.top;
            Win32_Window_Dimension dimension = win32_get_window_dimension(window);
            win32_display_buffer_in_window(&global_backbuffer, device_context, dimension.width, dimension.height);
            EndPaint(window, &paint);
        } break;
        
        default: {
            result = DefWindowProcA(window, message, wparam, lparam);
        } break;
    }
    
    return result;
}

internal void
win32_clear_buffer(Win32_Offscreen_Buffer *buffer, Game_State *game_state) {
    u8 *row = (u8 *)buffer->memory;
    for (int y = 0; y < buffer->height; ++y) {
        u32 *pixel = (u32 *)row;
        for (int x = 0; x < buffer->width; ++x) {
            *pixel++ = 0;
        }
        
        row += buffer->pitch;
    }
}

internal void
win32_process_keyboard_message(Game_Button_State *new_state, b32 is_down) {
    if (new_state->ended_down == is_down)  return;
    new_state->ended_down = is_down;
    ++new_state->half_transition_count;
}

internal void
win32_process_xinput_digital_button(DWORD xinput_button_state,
                                    Game_Button_State *old_state, DWORD button_bit,
                                    Game_Button_State *new_state) {
    b32 is_down = ((xinput_button_state & button_bit) == button_bit);
    new_state->ended_down = false;
    if (!is_down)  {
        new_state->allow_press = true;
        return;
    }
    else {
        new_state->allow_press = false;
        
        if (old_state->allow_press)  {
            new_state->ended_down = is_down;
            new_state->half_transition_count = (old_state->ended_down != new_state->ended_down) ? 1 : 0;
            return;
        }
    }
}

internal void
win32_process_pending_messages(Game_Controller_Input *keyboard_controller) {
    MSG message;
    while (PeekMessage(&message, 0, 0, 0, PM_REMOVE)) {
        switch (message.message) {
            case WM_QUIT: {
                global_running = false;
            } break;
            case WM_SYSKEYDOWN:
            case WM_SYSKEYUP:
            case WM_KEYDOWN:
            case WM_KEYUP: {
                u32 vk_code = (u32)message.wParam;
                
                // @note Since we are comparing was_down to is_down,
                // we MUST use == and != to convert these bit tests to actual 0 or 1 values.
                b32 was_down = ((message.lParam & (1 << 30)) != 0);
                b32 is_down = ((message.lParam & (1 << 31)) == 0);
                if (was_down != is_down) {
                    if (vk_code == 'W') {
                        win32_process_keyboard_message(&keyboard_controller->move_up, is_down);
                    }
                    else if (vk_code == 'A') {
                        win32_process_keyboard_message(&keyboard_controller->move_left, is_down);
                    }
                    else if (vk_code == 'S') {
                        win32_process_keyboard_message(&keyboard_controller->move_down, is_down);
                    }
                    else if (vk_code == 'D') {
                        win32_process_keyboard_message(&keyboard_controller->move_right, is_down);
                    }
                    else if (vk_code == 'Q') {
                        win32_process_keyboard_message(&keyboard_controller->left_shoulder, is_down);
                    }
                    else if (vk_code == 'E') {
                        win32_process_keyboard_message(&keyboard_controller->right_shoulder, is_down);
                    }
                    else if (vk_code == VK_UP) {
                        win32_process_keyboard_message(&keyboard_controller->action_up, is_down);
                    }
                    else if (vk_code == VK_LEFT) {
                        win32_process_keyboard_message(&keyboard_controller->action_left, is_down);
                    }
                    else if ((vk_code == VK_DOWN) || (vk_code == 'K')) {
                        win32_process_keyboard_message(&keyboard_controller->action_down, is_down);
                    }
                    else if ((vk_code == VK_RIGHT) || (vk_code == 'J')) {
                        win32_process_keyboard_message(&keyboard_controller->action_right, is_down);
                    }
                    else if (vk_code == VK_RETURN) {
                        win32_process_keyboard_message(&keyboard_controller->start, is_down);
                    }
                    else if (vk_code == VK_BACK) {
                        win32_process_keyboard_message(&keyboard_controller->back, is_down);
                    }
                    if (vk_code == VK_ESCAPE) {
                        global_running = false;
                    }
                    
                    if (is_down) {
                        b32 alt_key_was_down = (message.lParam & (1 << 29));
                        if (alt_key_was_down)  {
                            if (vk_code == VK_F4) {
                                global_running = false;
                            }
                            if (vk_code == VK_RETURN)  {
                                if (message.hwnd) {
                                    toggle_fullscreen(message.hwnd);
                                }
                            }
                        }
                    }
                }
            } break;
            
            default: {
                TranslateMessage(&message);
                DispatchMessageA(&message);
            } break;
        }
    }
}

inline LARGE_INTEGER
win32_get_wall_clock() {
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return counter;
}

inline f32
win32_get_seconds_elapsed(LARGE_INTEGER start, LARGE_INTEGER end) {
    f32 result = ((f32)(end.QuadPart - start.QuadPart) / (f32)global_performance_count_frequency);
    return result;
}

int CALLBACK
WinMain(HINSTANCE instance,
        HINSTANCE prev_instance,
        LPSTR     cmd_line,
        int       show_cmd) {
    LARGE_INTEGER performance_count_frequency_result;
    QueryPerformanceFrequency(&performance_count_frequency_result);
    global_performance_count_frequency = performance_count_frequency_result.QuadPart;
    
    UINT desired_scheduler_ms = 1;
    b32 sleep_is_granular = (timeBeginPeriod(desired_scheduler_ms) == TIMERR_NOERROR);
    
    win32_load_xinput();
    
    win32_resize_dib_section(&global_backbuffer, WIDTH, HEIGHT);
    
    WNDCLASSA window_class = {};
    window_class.style = CS_HREDRAW|CS_VREDRAW|CS_OWNDC;
    window_class.lpfnWndProc = win32_main_window_callback;
    window_class.hInstance = instance;
    window_class.lpszClassName = "TetrisWindowClass";
    
    if (!RegisterClassA(&window_class))  {
        // @todo
    }
    HWND window = CreateWindowExA(0, window_class.lpszClassName,
                                  "Tetris",
                                  WS_OVERLAPPEDWINDOW|WS_VISIBLE,
                                  CW_USEDEFAULT, CW_USEDEFAULT,
                                  WIDTH, HEIGHT,
                                  0, 0, instance, 0);
    if (!window)  {
        // @todo
    }
    
    HDC device_context = GetDC(window);
    
    int monitor_refresh_hz = 60;
    int win32_refresh_rate = GetDeviceCaps(device_context, VREFRESH);
    if (win32_refresh_rate > 1)  {
        monitor_refresh_hz = win32_refresh_rate;
    }
    f32 game_update_hz = (monitor_refresh_hz / 2.0f);
    f32 target_seconds_per_frame =  1.0f / (f32)game_update_hz;
    f32 dt = target_seconds_per_frame;
    
    f32 move_update_frequency_ms = 200.0f;
    LARGE_INTEGER last_move_counter = win32_get_wall_clock();
    
    LARGE_INTEGER perf_count_frequency_result;
    QueryPerformanceFrequency(&perf_count_frequency_result);
    s64 perf_count_frequency = perf_count_frequency_result.QuadPart;
    
    Game_Input input[2] = {};
    Game_Input *new_input = &input[0];
    Game_Input *old_input = &input[1];
    
    int active_controller_index = 0;
    
    Game_State game_state = {};
    
    global_running = true;
    
    LARGE_INTEGER last_counter = win32_get_wall_clock();
    QueryPerformanceCounter(&last_counter);
    u64 last_cycle_count = __rdtsc();
    while (global_running) {
        //
        // @note handle input
        //
        
        Game_Controller_Input *old_keyboard_controller = get_controller(old_input, 0);
        Game_Controller_Input *new_keyboard_controller = get_controller(new_input, 0);
        *new_keyboard_controller = {};
        new_keyboard_controller->is_connected = true;
        
#if 0
        for (int button_index = 0; button_index < array_count(new_keyboard_controller->buttons); ++button_index) {
            new_keyboard_controller->buttons[button_index].ended_down = old_keyboard_controller->buttons[button_index].ended_down;
        }
#endif
        
        win32_process_pending_messages(new_keyboard_controller);
        
        DWORD max_controller_count = XUSER_MAX_COUNT;
        DWORD input_controller_count = array_count(input->controllers) - 1;
        if (max_controller_count > input_controller_count) {
            max_controller_count = input_controller_count;
        }
        for (DWORD controller_index = 0; controller_index < max_controller_count; ++controller_index) {
            DWORD our_controller_index = controller_index + 1;
            Game_Controller_Input *old_controller = get_controller(old_input, our_controller_index);
            Game_Controller_Input *new_controller = get_controller(new_input, our_controller_index);
            *new_controller = {};
            
            XINPUT_STATE controller_state;
            ZeroMemory(&controller_state, sizeof(XINPUT_STATE));
            if (xinput_get_state(controller_index, &controller_state) != ERROR_SUCCESS) {
                new_controller->is_connected = false;
                continue;
            }
            
            new_controller->is_connected = true;
            new_controller->is_analog = old_controller->is_analog;
            XINPUT_GAMEPAD *pad = &controller_state.Gamepad;
            
            // XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE 7689
            // XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE 8689
            auto process_xinput_stick_value = [](SHORT value, SHORT deadzone_threshold) {
                f32 result = 0;
                if (value < -deadzone_threshold) {
                    result = (f32)((value + deadzone_threshold) / (32768.0f - deadzone_threshold));
                }
                else if (value > deadzone_threshold){
                    result = (f32)((value - deadzone_threshold) / (32767.0f - deadzone_threshold));
                }
                return result;
            };
            
            new_controller->stick_average_x = process_xinput_stick_value(pad->sThumbLX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
            new_controller->stick_average_y = process_xinput_stick_value(pad->sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
            if ((new_controller->stick_average_x != 0.0f) ||
                (new_controller->stick_average_y != 0.0f)) {
                new_controller->is_analog = true;
            }
            
            if (pad->wButtons & XINPUT_GAMEPAD_DPAD_UP) {
                new_controller->stick_average_y = 1.0f;
                new_controller->is_analog = false;
            }
            if (pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN) {
                new_controller->stick_average_y = -1.0f;
                new_controller->is_analog = false;
            }
            if (pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT) {
                new_controller->stick_average_x = -1.0f;
                new_controller->is_analog = false;
            }
            if (pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) {
                new_controller->stick_average_x = 1.0f;
                new_controller->is_analog = false;
            }
            
            f32 threshold = 0.5f;
            win32_process_xinput_digital_button((new_controller->stick_average_x < -threshold) ? 1 : 0,
                                                &old_controller->move_left, 1,
                                                &new_controller->move_left);
            win32_process_xinput_digital_button((new_controller->stick_average_x > threshold) ? 1 : 0,
                                                &old_controller->move_right, 1,
                                                &new_controller->move_right);
            win32_process_xinput_digital_button((new_controller->stick_average_y < -threshold) ? 1 : 0,
                                                &old_controller->move_down, 1,
                                                &new_controller->move_down);
            win32_process_xinput_digital_button((new_controller->stick_average_y > threshold) ? 1 : 0,
                                                &old_controller->move_up, 1,
                                                &new_controller->move_up);
            
            win32_process_xinput_digital_button(pad->wButtons,
                                                &old_controller->action_down, XINPUT_GAMEPAD_A,
                                                &new_controller->action_down);
            win32_process_xinput_digital_button(pad->wButtons,
                                                &old_controller->action_right, XINPUT_GAMEPAD_B,
                                                &new_controller->action_right);
            win32_process_xinput_digital_button(pad->wButtons,
                                                &old_controller->action_left, XINPUT_GAMEPAD_X,
                                                &new_controller->action_left);
            win32_process_xinput_digital_button(pad->wButtons,
                                                &old_controller->action_up, XINPUT_GAMEPAD_Y,
                                                &new_controller->action_up);
            
            win32_process_xinput_digital_button(pad->wButtons,
                                                &old_controller->left_shoulder, XINPUT_GAMEPAD_LEFT_SHOULDER,
                                                &new_controller->left_shoulder);
            win32_process_xinput_digital_button(pad->wButtons,
                                                &old_controller->right_shoulder, XINPUT_GAMEPAD_RIGHT_SHOULDER,
                                                &new_controller->right_shoulder);
            
            win32_process_xinput_digital_button(pad->wButtons,
                                                &old_controller->start, XINPUT_GAMEPAD_START,
                                                &new_controller->start);
            win32_process_xinput_digital_button(pad->wButtons,
                                                &old_controller->back, XINPUT_GAMEPAD_BACK,
                                                &new_controller->back);
        }
        
        //
        // @note do input
        //
        
        for (DWORD controller_index = 0; controller_index < input_controller_count; ++controller_index) {
            Game_Controller_Input *controller = get_controller(old_input, controller_index);
            if (controller->start.ended_down)  {
                active_controller_index = controller_index;
            }
        }
        
        {
            Game_Controller_Input *controller = get_controller(new_input, active_controller_index);
            
            if (controller->move_up.ended_down) {
            }
            else if (controller->move_left.ended_down) {
            }
            else if (controller->move_down.ended_down) {
            }
            else if (controller->move_right.ended_down) {
            }
            else if (controller->action_right.ended_down || controller->action_down.ended_down) {
            }
        }
        
        //
        // @note simulate
        //
        
        
        
        //
        // @note render
        //
        win32_clear_buffer(&global_backbuffer, &game_state);
        
        
        
        Win32_Window_Dimension dimension = win32_get_window_dimension(window);
        win32_display_buffer_in_window(&global_backbuffer, device_context, dimension.width, dimension.height);
        
        //
        // @note frame rate
        //
        LARGE_INTEGER work_counter = win32_get_wall_clock();
        f32 seconds_elapsed_for_work = win32_get_seconds_elapsed(last_counter, work_counter);
        
        f32 seconds_elapsed_for_frame = seconds_elapsed_for_work;
        if (seconds_elapsed_for_frame < target_seconds_per_frame) {
            DWORD sleep_ms;
            if (sleep_is_granular)  {
                sleep_ms = (DWORD)(1000 * (DWORD)(target_seconds_per_frame - seconds_elapsed_for_frame));
                if (sleep_ms > 0)  {
                    Sleep(sleep_ms);
                }
            }
            
            f32 test_seconds_elapsed_for_frame = win32_get_seconds_elapsed(last_counter, win32_get_wall_clock());
            if (test_seconds_elapsed_for_frame > target_seconds_per_frame)  {
                // @todo missed sleep here
            }
            
            while (seconds_elapsed_for_frame < target_seconds_per_frame) {
                seconds_elapsed_for_frame = win32_get_seconds_elapsed(last_counter, win32_get_wall_clock());
            }
        }
        else {
            // @todo missed frame rate!
        }
        
        LARGE_INTEGER end_counter = win32_get_wall_clock();
        f64 ms_per_frame = 1000.0f * win32_get_seconds_elapsed(last_counter, end_counter);
        last_counter = end_counter;
        
        Game_Input *temp_input = new_input;
        new_input = old_input;
        old_input = temp_input;
        
        u64 end_cycle_count = __rdtsc();
        u64 cycles_elapsed = end_cycle_count - last_cycle_count;
        last_cycle_count = end_cycle_count;
        
        f64 fps = 0; // @note not a relevant measurement (f64)global_performance_count_frequency / (f64)counter_elapsed;
        f64 mcpf = (f64)cycles_elapsed / (1000.0f * 1000.0f);
        
        char fps_buffer[256];
        _snprintf_s(fps_buffer, sizeof(fps_buffer), "%.02fms/work, %.02fms/f, %.02ffps, %.02fmc/f\n", seconds_elapsed_for_work*1000, ms_per_frame, fps, mcpf);
        OutputDebugStringA(fps_buffer);
    }
    
    return 0;
}