#if !defined(SDL2_PACMAN_H)


struct SDL2_Offscreen_Buffer {
    SDL_Texture *texture;
    void *memory;
    int width;
    int height;
    int pitch;
    int bytes_per_pixel;
};

struct SDL2_Window_Dimension {
    int width;
    int height;
};


#define SDL2_PACMAN_H
#endif
