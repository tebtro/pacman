#include "pacman.h"


internal void
render_weird_gradient(Game_Offscreen_Buffer *buffer, int blue_offset = 1, int green_offset = 1) {
    u8 *row = (u8 *)buffer->memory;
    for (int y = 0; y < buffer->height; ++y) {
        u32 *pixel = (u32 *)row;
        for (int x = 0; x < buffer->width; ++x) {
            /*                  0  1  2  3
                                pixel in memory: 00 00 00 00
                                LITTLE ENDIAN ARCHITECTURE
                                pixel in memory:      BB GG RR xx
                                pixel in Register: 0x xx RR GG BB
            */
            u8 red = 0;
            u8 green = (u8)(y + green_offset);
            u8 blue = (u8)(x + blue_offset);
            
            *pixel++ = ((red << 24) | ((green << 16) | blue));
        }
        
        row += buffer->pitch;
    }
}

extern "C" GAME_UPDATE_AND_RENDER_SIG(game_update_and_render) {
    assert((&input->controllers[0]._terminator_ - &input->controllers[0].buttons[0]) == array_count(input->controllers[0].buttons));
    assert(sizeof(Game_State) <= memory->permanent_storage_size);
    
    Game_State *game_state = (Game_State *)memory->permanent_storage;
    if (!memory->is_initialized) {
        defer {
            memory->is_initialized = true;
        };
        initialize_memory_arena(&game_state->game_arena,
                                memory->permanent_storage_size - sizeof(Game_State),
                                (u8 *)memory->permanent_storage + sizeof(Game_State));
    }
    
    //
    // @note do input
    //
    
    for (u32 controller_index = 0; controller_index < array_count(input->controllers); ++controller_index) {
        Game_Controller_Input *controller = get_controller(input, controller_index);
        if (controller->start.ended_down)  {
            game_state->active_controller_index = controller_index;
        }
    }
    
    {
        Game_Controller_Input *controller = get_controller(input, game_state->active_controller_index);
        
        if (controller->move_up.ended_down) {
            --game_state->offset_y;
        }
        else if (controller->move_left.ended_down) {
            --game_state->offset_x;
        }
        else if (controller->move_down.ended_down) {
            ++game_state->offset_y;
        }
        else if (controller->move_right.ended_down) {
            ++game_state->offset_x;
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
    render_weird_gradient(buffer, game_state->offset_x, game_state->offset_y);
}
