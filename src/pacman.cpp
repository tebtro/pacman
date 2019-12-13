// @todo game_reset also deletes the bitmaps, so they don't get rendered anymore!

// @todo Load ttf fonts and generate glyph data.

// @todo ghost should collide with other ghosts
// @todo Screen flashes sometimes when in fullscreen ???


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

internal void
draw_rectangle(Game_Offscreen_Buffer *buffer,
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

// @todo premultiplied alpha
// @note https://docs.microsoft.com/en-us/windows/win32/api/wingdi/ns-wingdi-bitmapfileheader
#pragma pack(push, 1)
// @note BITMAPINFO structure
struct Bitmap_Header {
    // @note BITMAPFILEHEADER structure
    u16 file_type;
    u32 file_size;
    u16 reserved_1;
    u16 reserved_2;
    u32 bitmap_offset;
    
    // @note BITMAPINFOHEADER structure
    u32 size;
    s32 width;
    s32 height;
    u16 planes;
    u16 bit_per_pixel;
    
    u32 compression_method;
    u32 size_of_bitmap;
    s32 horizontal_resolution;
    s32 vertical_resolution;
    u32 colors_used;
    u32 colors_important;
    
    // @note RGBQUAD structure
    u32 red_mask;
    u32 green_mask;
    u32 blue_mask;
};
#pragma pack(pop)

internal Loaded_Bitmap
load_bitmap(Platform_Read_Entire_File_Function *platform_read_entire_file, char *filename) {
    Loaded_Bitmap result = {};
    
    Read_File_Result read_result = platform_read_entire_file(filename);
    if (read_result.memory_size == 0)  return result;
    
    Bitmap_Header *header = (Bitmap_Header *)read_result.memory;
    // @todo handle compression
    assert(header->compression_method == 3);
    // @todo handle top-down or bottom-up order. For top-down the height will be negative.
    u32 *pixels = (u32 *)((u8 *)read_result.memory + header->bitmap_offset);
    
    // 0x AA RR GG BB
    
    u32 red_mask   = header->red_mask;
    u32 green_mask = header->green_mask;
    u32 blue_mask  = header->blue_mask;
    u32 alpha_mask = ~(red_mask | green_mask | blue_mask);
    
    Bit_Scan_Result red_scan = find_least_significant_set_bit(red_mask);
    Bit_Scan_Result green_scan = find_least_significant_set_bit(green_mask);
    Bit_Scan_Result blue_scan = find_least_significant_set_bit(blue_mask);
    Bit_Scan_Result alpha_scan = find_least_significant_set_bit(alpha_mask);
    
    assert(red_scan.found);
    assert(green_scan.found);
    assert(blue_scan.found);
    assert(alpha_scan.found);
    
    s32 alpha_shift = 24 - (s32)alpha_scan.index;
    s32 red_shift   = 16 - (s32)red_scan.index;
    s32 green_shift =  8 - (s32)green_scan.index;
    s32 blue_shift  =  0 - (s32)blue_scan.index;
    
    u32 *pixel = pixels;
    for (s32 y = 0; y < header->height; ++y) {
        for (s32 x = 0; x < header->width; ++x) {
            u32 color = *pixel;
#if 1
            *pixel = (rotate_left(color & alpha_mask, alpha_shift) |
                      rotate_left(color & red_mask,   red_shift)   |
                      rotate_left(color & green_mask, green_shift) |
                      rotate_left(color & blue_mask,  blue_shift));
#else
            *pixel = ((((color >> alpha_scan.index) & 0xFF) << 24) |
                      (((color >> red_scan.index)   & 0xFF) << 16) |
                      (((color >> green_scan.index) & 0xFF) <<  8) |
                      (((color >> blue_scan.index)  & 0xFF) <<  0));
#endif
            
            ++pixel;
        }
    }
    
    result.width  = header->width;
    result.height = header->height;
    result.pixels  = pixels;
    
    return result;
}

internal void
draw_bitmap(Game_Offscreen_Buffer *buffer, Loaded_Bitmap *bitmap,
            f32 float_x, f32 float_y, s32 align_x = 0, s32 align_y = 0) {
    float_x -= (f32)align_x;
    float_y -= (f32)align_y;
    
    s32 min_x = round_float_to_s32(float_x);
    s32 min_y = round_float_to_s32(float_y);
    s32 max_x = round_float_to_s32(float_x + (f32)bitmap->width);
    s32 max_y = round_float_to_s32(float_y + (f32)bitmap->height);
    
    s32 source_offset_x = 0;
    if (min_x < 0) {
        source_offset_x = -min_x;
        min_x = 0;
    }
    s32 source_offset_y = 0;
    if (min_y < 0) {
        source_offset_y = -min_y;
        min_y = 0;
    }
    if (max_x > buffer->width)   max_x = buffer->width;
    if (max_y > buffer->height)  max_y = buffer->height;
    
    u32 *source_row = bitmap->pixels + bitmap->width * (bitmap->height - 1);
    source_row += bitmap->width * -source_offset_y + source_offset_x;
    u8 *dest_row = ((u8 *)buffer->memory +
                    min_x * buffer->bytes_per_pixel +
                    min_y * buffer->pitch);
    for (s32 y = min_y; y < max_y; ++y) {
        u32 *source = source_row;
        u32 *dest = (u32 *)dest_row;
        
        for (s32 x = min_x; x < max_x; ++x) {
            f32 alpha = (f32)((u8)(*source >> 24) & 0xFF) / 255.0f;
            
            f32 source_red    = (f32)((u8)(*source >> 16) & 0xFF);
            f32 source_green  = (f32)((u8)(*source >>  8) & 0xFF);
            f32 source_blue   = (f32)((u8)(*source >>  0) & 0xFF);
            
            f32 dest_alpha = (f32)((u8)(*dest >> 24) & 0xFF);
            f32 dest_red   = (f32)((u8)(*dest >> 16) & 0xFF);
            f32 dest_green = (f32)((u8)(*dest >>  8) & 0xFF);
            f32 dest_blue  = (f32)((u8)(*dest >>  0) & 0xFF);
            
            // @note linear blending
            f32 red   = (1.0f - alpha) * dest_red   + alpha * source_red;
            f32 green = (1.0f - alpha) * dest_green + alpha * source_green;
            f32 blue  = (1.0f - alpha) * dest_blue  + alpha * source_blue;
            
            *dest = (((u32)(dest_alpha)   << 24) |
                     ((u32)(red   + 0.5f) << 16) |
                     ((u32)(green + 0.5f) << 8)  |
                     ((u32)(blue  + 0.5f) << 0));
            
            ++dest;
            ++source;
        }
        
        source_row -= bitmap->width;
        dest_row += buffer->pitch;
    }
}

// @cleanup @copynpaste
internal void
render_char(Game_Offscreen_Buffer *buffer, Game_State *game_state, f32 float_x, f32 float_y, char _char) {
    Loaded_Bitmap *bitmap = &game_state->bmp_font;
    int char_value = _char;
    int char_index = 0;
    if (char_value == 32) {
        Vector2 v_min = {
            float_x,
            float_y
        };
        Vector2 v_max = {
            v_min.x + 11.0f,
            v_min.y + 17.0f
        };
        draw_rectangle(buffer, v_min, v_max, 1.0f, 1.0f, 1.0f);
        return;
    }
    else if (char_value >= 48 && char_value <= 57) {
        char_index = char_value - 48;
    }
    else if (char_value >= 65 && char_value <= 90) {
        char_index = char_value - 65 + (36);
    }
    else if (char_value >= 97 && char_value <= 122) {
        char_index = char_value - 97 + (10);
    }
    
    
    s32 min_x = round_float_to_s32(float_x);
    s32 min_y = round_float_to_s32(float_y);
    s32 max_x = round_float_to_s32(float_x + (f32)11);
    s32 max_y = round_float_to_s32(float_y + (f32)17);
    
    s32 source_offset_x = 0;
    if (min_x < 0) {
        source_offset_x = -min_x;
        min_x = 0;
    }
    s32 source_offset_y = 0;
    if (min_y < 0) {
        source_offset_y = -min_y;
        min_y = 0;
    }
    if (max_x > buffer->width)   max_x = buffer->width;
    if (max_y > buffer->height)  max_y = buffer->height;
    
    u32 *source_row = bitmap->pixels + bitmap->width * (bitmap->height - 1);
    source_row += bitmap->width * -source_offset_y + source_offset_x;
    source_row += bitmap->width * -0 + 11 * char_index;
    u8 *dest_row = ((u8 *)buffer->memory +
                    min_x * buffer->bytes_per_pixel +
                    min_y * buffer->pitch);
    for (s32 y = min_y; y < max_y; ++y) {
        u32 *source = source_row;
        u32 *dest = (u32 *)dest_row;
        
        for (s32 x = min_x; x < max_x; ++x) {
            f32 alpha = (f32)((u8)(*source >> 24) & 0xFF) / 255.0f;
            
            f32 source_red    = (f32)((u8)(*source >> 16) & 0xFF);
            f32 source_green  = (f32)((u8)(*source >>  8) & 0xFF);
            f32 source_blue   = (f32)((u8)(*source >>  0) & 0xFF);
            
            f32 dest_alpha = (f32)((u8)(*dest >> 24) & 0xFF);
            f32 dest_red   = (f32)((u8)(*dest >> 16) & 0xFF);
            f32 dest_green = (f32)((u8)(*dest >>  8) & 0xFF);
            f32 dest_blue  = (f32)((u8)(*dest >>  0) & 0xFF);
            
            // @note linear blending
            f32 red   = (1.0f - alpha) * dest_red   + alpha * source_red;
            f32 green = (1.0f - alpha) * dest_green + alpha * source_green;
            f32 blue  = (1.0f - alpha) * dest_blue  + alpha * source_blue;
            
            *dest = (((u32)(dest_alpha)   << 24) |
                     ((u32)(red   + 0.5f) << 16) |
                     ((u32)(green + 0.5f) << 8)  |
                     ((u32)(blue  + 0.5f) << 0));
            
            ++dest;
            ++source;
        }
        
        source_row -= bitmap->width;
        dest_row += buffer->pitch;
    }
}

internal void
render_string(Game_Offscreen_Buffer *buffer, Game_State *game_state, f32 float_x, f32 float_y) {
    // @todo How are we going to handle strings? String library?
    char *str = "Hello Sailor";
    int count = 12;
    
    for (int i = 0; i < count; ++i) {
        render_char(buffer, game_state, float_x + (f32)(i * 11), float_y, str[i]);
    }
}

internal b32
is_tile_walkable(Grid *grid, int pos_x, int pos_y) {
    if (pos_x < 0 || pos_y < 0)  return false;
    b32 is_walkable = false;
    
    u32 tile_value = grid->tiles[pos_y*grid->width + pos_x];
    if (tile_value >= Tile_Type::WALKABLE) {
        is_walkable = true;
    }
    
    return is_walkable;
}

internal b32
move_if_walkable(Grid *grid, Entity *entity, enum32(Direction) dir) {
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
    
    if (new_pos_x < 0)  new_pos_x = grid->width - 1;
    if (new_pos_y < 0)  new_pos_y = grid->height - 1;
    if (new_pos_x >= grid->width)  new_pos_x = 0;
    if (new_pos_y >= grid->height) new_pos_y = 0;
    
    b32 is_walkable = is_tile_walkable(grid, new_pos_x, new_pos_y);
    if (is_walkable) {
        entity->pos_x = new_pos_x;
        entity->pos_y = new_pos_y;
    }
    
    return is_walkable;
}

internal void
reset_game(Game *game) {
    {
        Map current_map = game->current_map;
        *game = {};
        game->current_map = current_map;
    }
    Grid *grid = &game->current_map.grid;
    
    u32 _grid[27][21] = {
        // 0=nothing, 1=wall, 2=walkable, 3=coin, 4=power_coin, 5=player, 6=ghost1, 7=player_start ... ???
        {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
        {1,3,3,3,3,3,3,3,3,3,1,3,3,3,3,3,3,3,3,3,1},
        {1,3,1,1,1,3,1,1,1,3,1,3,1,1,1,3,1,1,1,3,1},
        {1,4,1,0,1,3,1,0,1,3,1,3,1,0,1,3,1,0,1,4,1},
        {1,3,1,1,1,3,1,1,1,3,1,3,1,1,1,3,1,1,1,3,1},
        {1,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,1},
        {1,3,1,1,1,3,1,3,1,1,1,1,1,3,1,3,1,1,1,3,1},
        {1,3,1,1,1,3,1,3,1,1,1,1,1,3,1,3,1,1,1,3,1},
        {1,3,3,3,3,3,1,3,3,3,1,3,3,3,1,3,3,3,3,3,1},
        {1,1,1,1,1,3,1,1,1,2,1,2,1,1,1,3,1,1,1,1,1},
        {0,0,0,0,1,3,1,2,2,2,2,2,2,2,1,3,1,0,0,0,0},
        {0,0,0,0,1,3,1,2,1,1,0,1,1,2,1,3,1,0,0,0,0},
        {1,1,1,1,1,3,1,2,1,0,0,0,1,2,1,3,1,1,1,1,1},
        {2,2,2,2,2,3,2,2,1,0,0,0,1,2,2,3,2,2,2,2,2},
        {1,1,1,1,1,3,1,2,1,1,1,1,1,2,1,3,1,1,1,1,1},
        {0,0,0,0,1,3,1,2,2,2,2,2,2,2,1,3,1,0,0,0,0},
        {0,0,0,0,1,3,1,2,1,1,1,1,1,2,1,3,1,0,0,0,0},
        {1,1,1,1,1,3,1,2,1,1,1,1,1,2,1,3,1,1,1,1,1},
        {1,3,3,3,3,3,3,3,3,3,1,3,3,3,3,3,3,3,3,3,1},
        {1,3,1,1,1,3,1,1,1,3,1,3,1,1,1,3,1,1,1,3,1},
        {1,4,3,3,1,3,3,3,3,3,3,3,3,3,3,3,1,3,3,4,1},
        {1,1,1,3,1,3,1,3,1,1,1,1,1,3,1,3,1,3,1,1,1},
        {1,1,1,3,1,3,1,3,1,1,1,1,1,3,1,3,1,3,1,1,1},
        {1,3,3,3,3,3,1,3,3,3,1,3,3,3,1,3,3,3,3,3,1},
        {1,3,1,1,1,1,1,1,1,3,1,3,1,1,1,1,1,1,1,3,1},
        {1,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,1},
        {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
    };
    for (int y = 0; y < grid->height; ++y) {
        for (int x = 0; x < grid->width; ++x) {
            grid->tiles[y*grid->width + x] = _grid[y][x];
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
    
    game->current_map.pacman_start_pos_x = 10;
    game->current_map.pacman_start_pos_y = 20;
    
    game->current_map.ghost_start_pos_x = 10;
    game->current_map.ghost_start_pos_y = 10;
    
    set_entity_pos(&game->pacman, game->current_map.pacman_start_pos_x, game->current_map.pacman_start_pos_y);
    set_entity_pos(&game->blinky, 9, 12);
    set_entity_pos(&game->clyde, 9, 13);
    set_entity_pos(&game->inky, 11, 12);
    set_entity_pos(&game->pinky, 11, 13);
    
    set_entity_color(&game->pacman, 1.0f, 1.0f, 0.0f);
    set_entity_color(&game->blinky, 1.0f, 0.0f, 0.0f);
    set_entity_color(&game->clyde, 0.0f, 1.0f, 0.0f);
    set_entity_color(&game->inky, 0.0f, 1.0f, 1.0f);
    set_entity_color(&game->pinky, 1.0f, 0.0f, 1.0f);
    
    game->pacman.state = Entity_State::ACTIVE;
    
    game->ghost_activation_frequency_ms = 10000.0f;
    game->dt_since_last_ghost_activation = 0.0f;
}

// :collision_detection
internal void
check_collision(Game *game, Entity *colliding_entity) {
    Entity *pacman = &game->pacman;
    
    if (pacman->pos_x != colliding_entity->pos_x ||
        pacman->pos_y != colliding_entity->pos_y)  return;
    
    if (pacman->state == Entity_State::POWER_UP) {
        colliding_entity->state = 0;
        colliding_entity->pos_x = game->current_map.ghost_start_pos_x;
        colliding_entity->pos_y = game->current_map.ghost_start_pos_y+2;
        
        // @todo increase score
        return;
    }
    
    if (game->pacman_lifes <= 1) {
        reset_game(game);
    }
    else if (game->pacman_lifes > 1) {
        --game->pacman_lifes;
        
        pacman->state = Entity_State::NEED_NEW_POSITION;
        pacman->pos_x = -1;
        pacman->pos_y = -1;
        pacman->move_direction = 0;
        pacman->next_input_direction = 0;
    }
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
render_coin(Game_Offscreen_Buffer *buffer, Game_State *game_state,
            u32 x, u32 y,
            f32 scale) {
    f32 tile_size = (f32)game_state->tile_size * scale;
    f32 offset_center = ((f32)game_state->tile_size - tile_size) / 2.0f;
    
#if 0
    Vector2 min = {
        (f32)(game_state->offset_x + x*game_state->tile_size + offset_center),
        (f32)(game_state->offset_y + y*game_state->tile_size + offset_center)
    };
    Vector2 max = {
        min.x + tile_size,
        min.y + tile_size
    };
    render_rectangle(buffer, min, max, 1.0f, 0.8f, 0.0f);
#endif
    u32 color = ((round_float_to_u32(1.0f * 255.0f) << 16) |
                 (round_float_to_u32(1.0f * 255.0f) << 8)  |
                 (round_float_to_u32(0.0f * 255.0f) << 0));
    
    f32 center_x = (game_state->offset_x + x*game_state->tile_size + (f32)game_state->tile_size/2.0f);
    f32 center_y = (game_state->offset_y + y*game_state->tile_size + (f32)game_state->tile_size/2.0f);
    
    int int_radius = (int)tile_size;
    for (int r = 0; r <= int_radius; ++r) {
        f32 current_x = center_x + r;
        f32 current_y = center_y;
        for (int angle = 0; angle <= 360; ++angle) {
            f32 rotated_x = ((current_x - center_x) * (f32)cos(angle)) - ((center_y - current_y) * (f32)sin(angle)) + center_x;
            f32 rotated_y = ((center_y - current_y) * (f32)cos(angle)) - ((current_x - center_x) * (f32)sin(angle)) + center_y;
            
            s32 int_x = round_float_to_s32(rotated_x);
            s32 int_y = round_float_to_s32(rotated_y);
            
            u32 *pixel = (u32 *)((u8 *)buffer->memory +
                                 int_x * buffer->bytes_per_pixel +
                                 int_y * buffer->pitch);
            
            *pixel = color;
        }
    }
}

internal void
render_grid(Game_Offscreen_Buffer *buffer, Game_State *game_state) {
    Grid *grid = &game_state->game->current_map.grid;
    for (int  y = 0; y < grid->height; ++y) {
        for (int x = 0; x < grid->width; ++x) {
            int tile_value = grid->tiles[y*grid->width + x];
            if (tile_value == Tile_Type::WALL) {
                render_tile(buffer, game_state, x, y, 0.0f, 0.0f, 0.6f);
            }
            else if (tile_value == Tile_Type::COIN) {
                render_coin(buffer, game_state, x, y, 0.1f);
            }
            else if (tile_value == Tile_Type::POWER_COIN) {
                render_coin(buffer, game_state, x, y, 0.4f);
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
        *game = {};
        Grid *grid = &game->current_map.grid;
        grid->width = 21;
        grid->height = 27;
        grid->tiles = push_array(&game_state->game_arena, (grid->height * grid->width), u32);
        reset_game(game);
        
        int tile_size;
        f32 offset_x = 0.0f;
        f32 offset_y = 0.0f;
        {
            int tile_width = buffer->width / grid->width;
            int tile_height = buffer->height / grid->height;
            tile_size = (tile_width > tile_height) ? tile_height : tile_width;
            tile_size = 32;
            
            f32 grid_pixel_width = (f32)(tile_size * grid->width);
            if (grid_pixel_width < (f32)buffer->width) {
                offset_x = ((f32)buffer->width - grid_pixel_width) / 2;
            }
            f32 grid_pixel_height = (f32)(tile_size * grid->height);
            if (grid_pixel_height < (f32)buffer->height) {
                offset_y = ((f32)buffer->height - grid_pixel_height) / 2;
            }
        }
        assert(tile_size);
        game_state->tile_size = tile_size;
        game_state->offset_x = offset_x;
        game_state->offset_y = offset_y;
        
        // @note load bitmaps
        game_state->bmp_ghost_blue = load_bitmap(memory->platform_read_entire_file, "../run_tree/data/sprites/ghost_blue.bmp");
        game->pacman.bmp = load_bitmap(memory->platform_read_entire_file, "../run_tree/data/sprites/pacman_closed.bmp");
        game->blinky.bmp = load_bitmap(memory->platform_read_entire_file, "../run_tree/data/sprites/ghost_blinky.bmp");
        game->clyde.bmp = load_bitmap(memory->platform_read_entire_file, "../run_tree/data/sprites/ghost_clyde.bmp");
        game->inky.bmp = load_bitmap(memory->platform_read_entire_file, "../run_tree/data/sprites/ghost_inky.bmp");
        game->pinky.bmp = load_bitmap(memory->platform_read_entire_file, "../run_tree/data/sprites/ghost_pinky.bmp");
        
        game_state->bmp_font = load_bitmap(memory->platform_read_entire_file, "../run_tree/data/fonts/bmp_font.bmp");
    }
    Game *game = game_state->game;
    Grid *grid = &game->current_map.grid;
    
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
            if (entity->state != 0)  continue;
            
            entity->state = NEED_NEW_POSITION;
            entity->pos_x = -1;
            entity->pos_y = -1;
            
            break;
        }
    }
    
    for (int i = 1; i < array_count(game->entities); ++i) {
        Entity *entity = &game->entities[i];
        if (entity->state != NEED_NEW_POSITION)  continue;
        
        if (is_tile_walkable(grid, game->current_map.ghost_start_pos_x, game->current_map.ghost_start_pos_y)) {
            entity->pos_x = game->current_map.ghost_start_pos_x;
            entity->pos_y = game->current_map.ghost_start_pos_y;
            entity->state = ACTIVE;
        }
    }
    
    // @note update ghost direcitons
    for (int i = 1; i < array_count(game->entities); ++i) {
        Entity *entity = &game->entities[i];
        if (entity->state != ACTIVE) continue;
        
        int count_walkables = 0;
        int dir_arr[4];
        if (is_tile_walkable(grid, entity->pos_x, entity->pos_y-1) &&
            entity->move_direction != Direction::DOWN)  dir_arr[count_walkables++] = Direction::UP;
        if (is_tile_walkable(grid, entity->pos_x, entity->pos_y+1) &&
            entity->move_direction != Direction::UP)  dir_arr[count_walkables++] = Direction::DOWN;
        if (is_tile_walkable(grid, entity->pos_x+1, entity->pos_y) &&
            entity->move_direction != Direction::LEFT)  dir_arr[count_walkables++] = Direction::RIGHT;
        if (is_tile_walkable(grid, entity->pos_x-1, entity->pos_y) &&
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
        if (entity->state != ACTIVE && entity->state != POWER_UP)  continue;
        
        // :collision_detection
        if (i > 0) {
            check_collision(game, entity);
        }
        
        entity->dt_since_last_move_update += (input->dt * 1000.0f);
        if (entity->dt_since_last_move_update >= entity->move_update_frequency_ms) {
            defer {
                entity->dt_since_last_move_update = 0.0f + (entity->dt_since_last_move_update - entity->move_update_frequency_ms);
            };
            
            b32 moved = move_if_walkable(grid, entity, entity->next_input_direction);
            if (moved) {
                entity->move_direction = entity->next_input_direction;
            }
            else {
                moved = move_if_walkable(grid, entity, entity->move_direction);
                if (moved == false)  entity->move_direction = 0;
            }
        }
        
        // :collision_detection
        if (i > 0) {
            check_collision(game, entity);
        }
    }
    
    // :collision_detection @note check player ghost collision
    if (colliding) {
        if (game->pacman_lifes <= 1) {
            reset_game(game);
        }
        else if (game->pacman_lifes > 1) {
            --game->pacman_lifes;
            
            pacman->state = Entity_State::NEED_NEW_POSITION;
            pacman->pos_x = -1;
            pacman->pos_y = -1;
            pacman->move_direction = 0;
            pacman->next_input_direction = 0;
        }
    }
    
    if (pacman->state == Entity_State::NEED_NEW_POSITION) {
        b32 is_walkable = true;
        for (int i = 1; i < array_count(game->entities); ++i) {
            Entity *entity = &game->entities[i];
            if (entity->pos_x == game->current_map.pacman_start_pos_x &&
                entity->pos_y == game->current_map.pacman_start_pos_y) {
                is_walkable = false;
            }
        }
        if (is_walkable && is_tile_walkable(grid, game->current_map.pacman_start_pos_x,
                                            game->current_map.pacman_start_pos_y)) {
            pacman->pos_x = game->current_map.pacman_start_pos_x;
            pacman->pos_y = game->current_map.pacman_start_pos_y;
            pacman->state = ACTIVE;
        }
    }
    
    // @note coin and power up
    if (pacman->state == Entity_State::POWER_UP) {
        game->pacman_power_up_duration_left -= input->dt * 1000.0f;
        if (game->pacman_power_up_duration_left <= 0.0f) {
            pacman->state = ACTIVE;
            pacman->color = {1.0f, 1.0f, 0.0f};
            pacman->move_update_frequency_ms = 400.0f;
            game-> pacman_power_up_duration_left = 0.0f;
        }
    }
    enum32(Tile_Type) tile_value = grid->tiles[pacman->pos_y*grid->width + pacman->pos_x];
    if (tile_value == Tile_Type::COIN ||
        tile_value == Tile_Type::POWER_COIN) {
        grid->tiles[pacman->pos_y*grid->width + pacman->pos_x] = Tile_Type::WALKABLE;
        
        // @todo increase score
        
        if (tile_value == Tile_Type::POWER_COIN) {
            pacman->state = POWER_UP;
            pacman->color = {0.0f, 0.0f, 1.0f};
            pacman->move_update_frequency_ms = 200.0f;
            game->pacman_power_up_duration_left += 10000.0f;
        }
    }
    
    b32 coins_cleared = true;
    for (int y = 0; y < grid->height; ++y) {
        for (int x = 0; x < grid->width; ++x) {
            tile_value = grid->tiles[y*grid->width + x];
            if (tile_value == Tile_Type::COIN ||
                tile_value == Tile_Type::POWER_COIN) {
                coins_cleared = false;
                goto COINS_NOT_CLEARED;
            }
        }
    }
    COINS_NOT_CLEARED:
    if (coins_cleared) {
        // @todo won, next level
        reset_game(game);
    }
    
    //
    // @note render
    //
    // render_weird_gradient(buffer, game_state->offset_x, game_state->offset_y);
    clear_buffer(buffer);
    render_grid(buffer, game_state);
    
    for (int i = 0; i < array_count(game->entities); ++i) {
        Entity *entity = &game->entities[i];
        Loaded_Bitmap *bmp = &entity->bmp;
        if (game->pacman.state == Entity_State::POWER_UP && i != 0) {
            bmp = &game_state->bmp_ghost_blue;
        }
        
        draw_bitmap(buffer, bmp,
                    (f32)entity->pos_x * game_state->tile_size + game_state->offset_x,
                    (f32)entity->pos_y * game_state->tile_size + game_state->offset_y);
        
#if 0
        render_tile(buffer, game_state,
                    entity->pos_x, entity->pos_y,
                    entity->color.r, entity->color.g, entity->color.b);
#endif
    }
    
    // ui
    
    render_string(buffer, game_state, 0, 0);
}
