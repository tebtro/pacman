#if !defined(PACMAN_H)

#include "iml_general.h"

#include "pacman_platform.h"

//
// @note: memory management
//

struct Memory_Arena {
    memory_index size;
    u8 *base;
    memory_index used;
};

internal void
initialize_memory_arena(Memory_Arena *arena,
                        memory_index size,
                        u8 *base) {
    arena->size = size;
    arena->base = base;
    arena->used = 0;
}

#define push_struct(arena, type) (type *)_push_size_(arena, sizeof(type))
#define push_array(arena, count, type) (type *)_push_size_(arena, (count)*sizeof(type))
internal void *
_push_size_(Memory_Arena *arena, memory_index size) {
    assert((arena->used + size) <= arena->size);
    void *memory = arena->base + arena->used;
    arena->used += size;
    
    return memory;
}


//
// @note: game related
//

enum Tile_Type : u32 {
    ZERO = 0,
    WALL = 1,
    
    WALKABLE = 2,
    COIN = 3,
    POWER_COIN = 4,
    
    PLAYER = 5,
    PlAYER_START_POS = 6,
    
    GHOST = 7,
};

enum Direction : u32 {
    UP = 1,
    RIGHT = 2,
    DOWN = 3,
    LEFT = 4
};

struct Entity {
    b32 is_active;
    
    int pos_x;
    int pos_y;
    // f32 pacman_visual_pos;
    
    enum32(Direction) move_direction;
    enum32(Direction) next_input_direction;
    
    f32 move_update_frequency_ms = 400.0f;
    f32 dt_since_last_move_update = 0.0f;
    
    struct Color {
        f32 r, g, b;
    } color;
};

struct Game {
    u32 *grid;
    int grid_width;
    int grid_height;
    
    union {
        struct {
            Entity pacman;
            Entity ghost1;
            Entity ghost2;
            Entity ghost3;
            Entity ghost4;
        };
        Entity entities[5];
    };
    
    int pacman_lifes = 3;
    
    f32 ghost_activation_frequency_ms = 10000.0f;
    f32 dt_since_last_ghost_activation = 0.0f;
};

struct Game_State {
    Memory_Arena game_arena;
    Game *game;
    
    int tile_size;
    f32 offset_x;
    f32 offset_y;
    
    u32 active_controller_index;
};

#define PACMAN_H
#endif