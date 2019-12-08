// @todo ghost should collide with other ghosts

#include "pacman.h"
#include "pacman_math.h"
#include "pacman_intrinsics.h"


#include <stdlib.h>
#include <time.h>

internal u32
get_random_number_in_range(int min, int max) {
    srand((unsigned int)time(null));
    u32 random = min + (rand() % ((max + 1) - min));
    return random;
}

internal b32
is_tile_walkable(Game *game, int pos_x, int pos_y) {
    if (pos_x < 0 || pos_y < 0)  return false;
    b32 is_walkable = false;
    
    u32 tile_value = game->grid[pos_y*game->grid_width + pos_x];
    if (tile_value >= Tile_Type::WALKABLE) {
        is_walkable = true;
    }
    
    return is_walkable;
}

internal b32
move_if_walkable(Game *game, Entity *entity, enum32(Direction) dir) {
    if (dir == 0)  return false;
    
    int old_pos_x = entity->pos_x;
    int old_pos_y = entity->pos_y;
    int new_pos_x = old_pos_x;
    int new_pos_y = old_pos_y;
    
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
    
    if (new_pos_x < 0)  new_pos_x = game->grid_width - 1;
    if (new_pos_y < 0)  new_pos_y = game->grid_height - 1;
    if (new_pos_x >= game->grid_width)  new_pos_x = 0;
    if (new_pos_y >= game->grid_height) new_pos_y = 0;
    
    b32 is_walkable = is_tile_walkable(game, new_pos_x, new_pos_y);
    if (is_walkable) {
        entity->pos_x = new_pos_x;
        entity->pos_y = new_pos_y;
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
    for (int  y = 0; y < game->grid_height; ++y) {
        for (int x = 0; x < game->grid_width; ++x) {
            int tile_value = game->grid[y*game->grid_width + x];
            if (tile_value == 1) {
                render_tile(buffer, game_state, x, y, 0.0f, 0.0f, 1.0f);
            }
        }
    }
}

internal void
reset_game(Game *game) {
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
    for (int y = 0; y < game->grid_height; ++y) {
        for (int x = 0; x < game->grid_width; ++x) {
            game->grid[y*game->grid_width + x] = _grid[y][x];
        }
    }
    for (int i = 0; i < array_count(game->entities); ++i) {
        game->entities[i] = {};
    }
    auto set_entity_pos = [](Entity *entity, int pos_x, int pos_y) {
        entity->pos_x = pos_x;
        entity->pos_y = pos_y;
    };
    auto set_entity_color = [](Entity *entity, f32 r, f32 g, f32 b) {
        entity->color.r = r;
        entity->color.g = g;
        entity->color.b = b;
    };
    
    set_entity_pos(&game->pacman, 10, 20);
    set_entity_pos(&game->ghost1, 9, 12);
    set_entity_pos(&game->ghost2, 9, 13);
    set_entity_pos(&game->ghost3, 11, 12);
    set_entity_pos(&game->ghost4, 11, 13);
    
    set_entity_color(&game->pacman, 1.0f, 1.0f, 0.0f);
    set_entity_color(&game->ghost1, 1.0f, 0.0f, 0.0f);
    set_entity_color(&game->ghost2, 0.0f, 1.0f, 0.0f);
    set_entity_color(&game->ghost3, 0.0f, 1.0f, 1.0f);
    set_entity_color(&game->ghost4, 1.0f, 0.0f, 1.0f);
    
    game->pacman.is_active = true;
#if 0
    game->ghost1.is_active = true;
    set_entity_pos(&game->ghost1, 10, 10);
#endif
    
    game->ghost_activation_frequency_ms = 10000.0f;
    game->dt_since_last_ghost_activation = 0.0f;
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
        *game = {};
        game->grid = push_array(&game_state->game_arena, (game->grid_height * game->grid_width), u32);
        game->grid_width = 21;
        game->grid_height = 27;
        reset_game(game);
        
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
            game->pacman.next_input_direction = Direction::UP;
        }
        else if (controller->move_left.ended_down) {
            game->pacman.next_input_direction = Direction::LEFT;
        }
        else if (controller->move_down.ended_down) {
            game->pacman.next_input_direction = Direction::DOWN;
        }
        else if (controller->move_right.ended_down) {
            game->pacman.next_input_direction = Direction::RIGHT;
        }
        else if (controller->action_right.ended_down || controller->action_down.ended_down) {
        }
    }
    
    //
    // @note simulate
    //
    
    // @note ghost activation
    game->dt_since_last_ghost_activation += (input->dt * 1000.0f);
    if (game->dt_since_last_ghost_activation >= game->ghost_activation_frequency_ms) {
        defer {
            game->dt_since_last_ghost_activation = 0.0f + (game->dt_since_last_ghost_activation - game->ghost_activation_frequency_ms);
        };
        
        for (int i = 1; i < array_count(game->entities); ++i) {
            Entity *entity = &game->entities[i];
            if (entity->is_active)  continue;
            
            // @todo entity state enum, is_active, activating, not_moving, moving, ... ???
            entity->is_active = true;
            entity->pos_x = -1;
            entity->pos_y = -1;
            
            break;
        }
    }
    
    for (int i = 1; i < array_count(game->entities); ++i) {
        Entity *entity = &game->entities[i];
        if (!entity->is_active)  continue;
        if (entity->pos_x != -1 && entity->pos_y != -1)  continue;
        
        if (is_tile_walkable(game, 10, 10)) {
            entity->pos_x = 10;
            entity->pos_y = 10;
        }
    }
    
    // @note update ghost direcitons
    for (int i = 1; i < array_count(game->entities); ++i) {
        Entity *entity = &game->entities[i];
        if (!entity->is_active) continue;
        
        int count_walkables = 0;
        int dir_arr[4];
        if (is_tile_walkable(game, entity->pos_x, entity->pos_y-1) &&
            entity->move_direction != Direction::DOWN)  dir_arr[count_walkables++] = Direction::UP;
        if (is_tile_walkable(game, entity->pos_x, entity->pos_y+1) &&
            entity->move_direction != Direction::UP)  dir_arr[count_walkables++] = Direction::DOWN;
        if (is_tile_walkable(game, entity->pos_x+1, entity->pos_y) &&
            entity->move_direction != Direction::LEFT)  dir_arr[count_walkables++] = Direction::RIGHT;
        if (is_tile_walkable(game, entity->pos_x-1, entity->pos_y) &&
            entity->move_direction != Direction::RIGHT)  dir_arr[count_walkables++] = Direction::LEFT;
        
        if (count_walkables == 0)  continue;
        enum32(Direction) dir = dir_arr[0];
        if (count_walkables > 1) {
            dir = dir_arr[get_random_number_in_range(0, count_walkables-1)];
        }
        
        entity->next_input_direction = dir;
    }
    
    // @note update entity positions
    b32 colliding = false;
    Entity *pacman = &game->entities[0];
    
    for (int i = 0; i < array_count(game->entities); ++i) {
        Entity *entity = &game->entities[i];
        if (!entity->is_active)  continue;
        
        // :collision_detection
        if (i > 0) {
            if (pacman->pos_x == entity->pos_x &&
                pacman->pos_y == entity->pos_y) {
                colliding = true;
            }
        }
        
        entity->dt_since_last_move_update += (input->dt * 1000.0f);
        if (entity->dt_since_last_move_update >= entity->move_update_frequency_ms) {
            defer {
                entity->dt_since_last_move_update = 0.0f + (entity->dt_since_last_move_update - entity->move_update_frequency_ms);
            };
            
            b32 moved = move_if_walkable(game, entity, entity->next_input_direction);
            if (moved) {
                entity->move_direction = entity->next_input_direction;
            }
            else {
                moved = move_if_walkable(game, entity, entity->move_direction);
                if (moved == false)  entity->move_direction = 0;
            }
        }
        
        // :collision_detection
        if (i > 0) {
            if (pacman->pos_x == entity->pos_x &&
                pacman->pos_y == entity->pos_y) {
                colliding = true;
            }
        }
    }
    
    // :collision_detection @note check player ghost collision
    if (colliding) {
        if (game->pacman_lifes <= 1) {
            reset_game(game);
        }
        else if (game->pacman_lifes > 1) {
            --game->pacman_lifes;
            
            pacman->pos_x = 10;
            pacman->pos_y = 20;
            pacman->move_direction = 0;
            pacman->next_input_direction = 0;
        }
    }
    
    //
    // @note render
    //
    // render_weird_gradient(buffer, game_state->offset_x, game_state->offset_y);
    clear_buffer(buffer);
    render_grid(buffer, game_state);
    
    for (int i = 0; i < array_count(game->entities); ++i) {
        Entity *entity = &game->entities[i];
        
        render_tile(buffer, game_state,
                    entity->pos_x, entity->pos_y,
                    entity->color.r, entity->color.g, entity->color.b);
    }
}
