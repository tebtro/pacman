#if !defined(PACMAN_MATH_H)

//
// Vector2
//

union Vector2 {
    struct {
        f32 x;
        f32 y;
    };
    f32 e[2];
};

#define PACMAN_MATH_H
#endif
