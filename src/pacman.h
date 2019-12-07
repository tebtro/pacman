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

struct Game_State {
    Memory_Arena game_arena;
    
    u32 active_controller_index;
    u32 offset_x;
    u32 offset_y;
};

#define PACMAN_H
#endif