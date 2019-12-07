#if !defined(PACMAN_PLATFORM_H)

#ifdef __cplusplus
extern "C" {
#  if 0
}
#endif
#endif


//
// @note compiler flags:
//
/*
BUILD_INTERNAL:
- 0 Build for public release
- 1 Build for developer only

BUILD_SLOW:
- 0 Not slow code allowed!
- 1 Slow code welcome.
*/

//
// @note compilers
//
#if !defined(COMPILER_MSVC)
#    define COMPILER_MSVC 0
#endif

#if !defined(COMPILER_LLVM)
#    define COMPILER_LLVM 0 // clang/llvm
#endif


#if !COMPILER_MSVC && !COMPILER_LLVM
#    if _MSC_VER
#        undef COMPILER_MSVC
#        define COMPILER_MSVC 1
#    else
#        undef COMPILER_LLVM
#        define COMPILER_LLVM 1
#    endif
#endif


#if COMPILER_MSVC
#include <intrin.h>
#endif

//
// @note types
//
#include "iml_types.h"

//
// @note macros, ...
//

#ifdef assert
#    undef assert
#endif
#if BUILD_SLOW
#define assert(expression) if (!(expression)) { *(int *)(0) = 0; }
#else
#define assert(expression)
#endif

#define kilobytes_to_bytes(value) ((value)*1024LL)
#define megabytes_to_bytes(value) (kilobytes_to_bytes(value)*1024LL)
#define gigabytes_to_bytes(value) (megabytes_to_bytes(value)*1024LL)
#define terabytes_to_bytes(value) (gigabytes_to_bytes(value)*1024LL)

inline u32
safe_truncate_u64_to_u32(u64 value) {
    assert(value <= 0xFFFFFFFF);
    return (u32)value;
}

//
// @note: services that the game provides to the platform layer.
// (this may expand in the future- sound on separate thread, etc.)
//

typedef struct Game_Offscreen_Buffer {
    // @NOTE Pixels are always 32-bits wide, Memory Order 0x xx RR GG BB
    void *memory;
    int width;
    int height;
    int pitch;
    int bytes_per_pixel;
} Game_Offscreen_Buffer;

typedef struct Game_Button_State {
    b32 ended_down;
    b32 allow_press;
    int half_transition_count;
} Game_Button_State;

typedef struct Game_Controller_Input {
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
} Game_Controller_Input;

typedef struct Game_Input {
    Game_Controller_Input controllers[5];
    
    Game_Button_State mouse_buttons[5];
    u32 mouse_x, mouse_y, mouse_z;
    
    f32 dt; // @note(tebtro): dt for frame, how long the current frame will take
} Game_Input;

inline Game_Controller_Input *
get_controller(Game_Input *input, u32 controller_index) {
    assert(controller_index < array_count(input->controllers));
    return &input->controllers[controller_index];
}

typedef struct Game_Memory {
    b32 is_initialized;
    
    u64 permanent_storage_size;
    void *permanent_storage; // @note REQUIRED to be cleared to zero at startup
} Game_Memory;


// @note game_update_and_render
#define GAME_UPDATE_AND_RENDER_SIG(name) void name(Game_Memory *memory, Game_Input *input, Game_Offscreen_Buffer *buffer)
typedef GAME_UPDATE_AND_RENDER_SIG(Game_Update_And_Render_Function);


#ifdef __cplusplus
#  if 0
{
#  endif
}
#endif

#define PACMAN_PLATFORM_H
#endif
