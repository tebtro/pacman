#include "pacman.h"
#include "pacman_math.h"
#include "pacman_intrinsics.h"


internal b32
is_tile_walkable(Game *game, u32 pos_x, u32 pos_y) {
    b32 is_walkable = false;
    
    u32 tile_value = game->grid[pos_y*game->grid_width + pos_x];
    if (tile_value >= Tile_Type::WALKABLE) {
        is_walkable = true;
    }
    
    return is_walkable;
}

internal b32
move_if_walkable(Game *game, u32 old_pos_x, u32 old_pos_y, enum32(Direction) dir) {
    if (dir == 0)  return false;
    
    u32 new_pos_x = old_pos_x;
    u32 new_pos_y = old_pos_y;
    
    if (dir == UP) {
        --new_pos_y;
    }
    else if (dir == LEFT) {
        --new_pos_x;
    }
    else if (dir == DOWN) {
        ++new_pos_y;
    }
    else if (dir == RIGHT) {
        ++new_pos_x;
    }
    
    b32 is_walkable = is_tile_walkable(game, new_pos_x, new_pos_y);
    if (is_walkable) {
        game->pacman_pos_x = new_pos_x;
        game->pacman_pos_y = new_pos_y;
    }
    
    return is_walkable;
}

internal void
clear_buffer(Game_Offscreen_Buffer *buffer) {
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
render_rectangle(Game_Offscreen_Buffer *buffer,
                 Vector2 v_min, Vector2 v_max,
                 f32 r, f32 g, f32 b) {
    s32 min_x = round_float_to_s32(v_min.x);
    s32 min_y = round_float_to_s32(v_min.y);
    s32 max_x = round_float_to_s32(v_max.x);
    s32 max_y = round_float_to_s32(v_max.y);
    
    if (min_x < 0)  min_x = 0;
    if (min_y < 0)  min_y = 0;
    if (max_x > buffer->width)   max_x = buffer->width;
    if (max_y > buffer->height)  max_y = buffer->height;
    
    u32 color = ((round_float_to_u32(r * 255.0f) << 16) |
                 (round_float_to_u32(g * 255.0f) << 8)  |
                 (round_float_to_u32(b * 255.0f) << 0));
    
    u8 *row = ((u8 *)buffer->memory +
               min_x * buffer->bytes_per_pixel +
               min_y * buffer->pitch);
    for (int y = min_y; y < max_y; ++y) {
        u32 *pixel = (u32 *)row;
        for (int x = min_x; x < max_x; ++x) {
            *pixel++ = color;
        }
        row += buffer->pitch;
    }
}

internal void
render_tile(Game_Offscreen_Buffer *buffer, Game_State *game_state,
            u32 x, u32 y,
            f32 r, f32 g, f32 b) {
    Vector2 min = {
        (f32)(game_state->offset_x + x*game_state->tile_size),
        (f32)(game_state->offset_y + y*game_state->tile_size)
    };
    Vector2 max = {
        min.x+game_state->tile_size,
        min.y+game_state->tile_size
    };
    render_rectangle(buffer, min, max, r, g, b);
}

internal void
render_grid(Game_Offscreen_Buffer *buffer, Game_State *game_state) {
    Game *game = game_state->game;
    for (u32 y = 0; y < game->grid_height; ++y) {
        for (u32 x = 0; x < game->grid_width; ++x) {
            int tile_value = game->grid[y*game->grid_width + x];
            if (tile_value == 1) {
                render_tile(buffer, game_state, x, y, 0.0f, 0.0f, 1.0f);
            }
        }
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
        
        *game_state = {};
        initialize_memory_arena(&game_state->game_arena,
                                memory->permanent_storage_size - sizeof(Game_State),
                                (u8 *)memory->permanent_storage + sizeof(Game_State));
        
        game_state->game = push_struct(&game_state->game_arena, Game);
        Game *game = game_state->game;
        game->grid = push_array(&game_state->game_arena, (game->grid_height * game->grid_width), u32);
        game->grid_width = 21;
        game->grid_height = 27;
        u32 _grid[27][21] = {
            // 0=nothing, 1=wall, 2=walkable, 3=coin, 4=power_coin, 5=player, 6=ghost1, 7=player_start ... ???
            {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
            {1,2,2,2,2,2,2,2,2,2,1,2,2,2,2,2,2,2,2,2,1},
            {1,2,1,1,1,2,1,1,1,2,1,2,1,1,1,2,1,1,1,2,1},
            {1,2,1,2,1,2,1,2,1,2,1,2,1,2,1,2,1,2,1,2,1},
            {1,2,1,1,1,2,1,1,1,2,1,2,1,1,1,2,1,1,1,2,1},
            {1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1},
            {1,2,1,1,1,2,1,2,1,1,1,1,1,2,1,2,1,1,1,2,1},
            {1,2,1,1,1,2,1,2,1,1,1,1,1,2,1,2,1,1,1,2,1},
            {1,2,2,2,2,2,1,2,2,2,1,2,2,2,1,2,2,2,2,2,1},
            {1,1,1,1,1,2,1,1,1,2,1,2,1,1,1,2,1,1,1,1,1},
            {0,0,0,0,1,2,1,2,2,2,2,2,2,2,1,2,1,0,0,0,0},
            {0,0,0,0,1,2,1,2,1,1,0,1,1,2,1,2,1,0,0,0,0},
            {1,1,1,1,1,2,1,2,1,0,0,0,1,2,1,2,1,1,1,1,1},
            {2,2,2,2,2,2,2,2,1,0,0,0,1,2,2,2,2,2,2,2,2},
            {1,1,1,1,1,2,1,2,1,1,1,1,1,2,1,2,1,1,1,1,1},
            {0,0,0,0,1,2,1,2,2,2,2,2,2,2,1,2,1,0,0,0,0},
            {0,0,0,0,1,2,1,2,1,1,1,1,1,2,1,2,1,0,0,0,0},
            {1,1,1,1,1,2,1,2,1,1,1,1,1,2,1,2,1,1,1,1,1},
            {1,2,2,2,2,2,2,2,2,2,1,2,2,2,2,2,2,2,2,2,1},
            {1,2,1,1,1,2,1,1,1,2,1,2,1,1,1,2,1,1,1,2,1},
            {1,2,2,2,1,2,2,2,2,2,2,2,2,2,2,2,1,2,2,2,1},
            {1,1,1,2,1,2,1,2,1,1,1,1,1,2,1,2,1,2,1,1,1},
            {1,1,1,2,1,2,1,2,1,1,1,1,1,2,1,2,1,2,1,1,1},
            {1,2,2,2,2,2,1,2,2,2,1,2,2,2,1,2,2,2,2,2,1},
            {1,2,1,1,1,1,1,1,1,2,1,2,1,1,1,1,1,1,1,2,1},
            {1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1},
            {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
        };
        for (u32 y = 0; y < game->grid_height; ++y) {
            for (u32 x = 0; x < game->grid_width; ++x) {
                game->grid[y*game->grid_width + x] = _grid[y][x];
            }
        }
        game->pacman_pos_x = 10;
        game->pacman_pos_y = 20;
        
        int tile_size;
        f32 offset_x = 0.0f;
        f32 offset_y = 0.0f;
        {
            int tile_width = buffer->width / game->grid_width;
            int tile_height = buffer->height / game->grid_height;
            tile_size = (tile_width > tile_height) ? tile_height : tile_width;
            
            f32 grid_pixel_width = (f32)(tile_size * game->grid_width);
            if (grid_pixel_width < (f32)buffer->width) {
                offset_x = ((f32)buffer->width - grid_pixel_width) / 2;
            }
            f32 grid_pixel_height = (f32)(tile_size * game->grid_height);
            if (grid_pixel_height < (f32)buffer->height) {
                offset_y = ((f32)buffer->height - grid_pixel_height) / 2;
            }
        }
        assert(tile_size);
        game_state->tile_size = tile_size;
        game_state->offset_x = offset_x;
        game_state->offset_y = offset_y;
    }
    Game *game = game_state->game;
    
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
            game->pacman_next_input_direction = Direction::UP;
        }
        else if (controller->move_left.ended_down) {
            game->pacman_next_input_direction = Direction::LEFT;
        }
        else if (controller->move_down.ended_down) {
            game->pacman_next_input_direction = Direction::DOWN;
        }
        else if (controller->move_right.ended_down) {
            game->pacman_next_input_direction = Direction::RIGHT;
        }
        else if (controller->action_right.ended_down || controller->action_down.ended_down) {
        }
    }
    
    //
    // @note simulate
    //
    
    game_state->dt_since_last_move_update += (input->dt * 1000.0f);
    if (game_state->dt_since_last_move_update >= game_state->move_update_frequency_ms) {
        defer {
            game_state->dt_since_last_move_update = 0.0f + (game_state->dt_since_last_move_update - game_state->move_update_frequency_ms);
        };
        
        b32 moved = move_if_walkable(game, game->pacman_pos_x, game->pacman_pos_y, game->pacman_next_input_direction);
        if (moved) {
            game->pacman_move_direction = game->pacman_next_input_direction;
        }
        else {
            moved = move_if_walkable(game, game->pacman_pos_x, game->pacman_pos_y, game->pacman_move_direction);
            if (moved == false)  game->pacman_move_direction = 0;
        }
    }
    
    //
    // @note render
    //
    // render_weird_gradient(buffer, game_state->offset_x, game_state->offset_y);
    clear_buffer(buffer);
    render_grid(buffer, game_state);
    
    render_tile(buffer, game_state,
                game->pacman_pos_x, game->pacman_pos_y,
                1.0f, 1.0f, 0.0f);
}
