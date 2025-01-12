#ifndef SHARED_H
#define SHARED_H

#define MAX_WORLD_WIDTH  100
#define MAX_WORLD_HEIGHT 100
#define MAX_SNAKE_LENGTH 200
#define MAX_OBSTACLES    50

// Enum pre smer pohybu hadíka
typedef enum {
    DIRECTION_UP,
    DIRECTION_DOWN,
    DIRECTION_LEFT,
    DIRECTION_RIGHT
} Direction;

// Enum pre typ sveta
typedef enum {
    WORLD_TYPE_EMPTY,
    WORLD_TYPE_OBSTACLES
} WorldType;

// Štruktúra pre pozíciu v hernom svete
typedef struct {
    int x;
    int y;
} Position;

// Štruktúra pre herný stav
typedef struct {
    int width;
    int height;
    Position snake[MAX_SNAKE_LENGTH];
    int snake_length;
    Position fruit;
    Position obstacles[MAX_OBSTACLES];
    int num_obstacles;
    int score;
    WorldType world_type;
    Direction direction;
    int timed_mode;       // 1 pre časový režim, 0 pre štandardný režim
    int time_limit;       // Časový limit v sekundách pre časový režim
    int elapsed_time;     // Uplynulý čas od začiatku hry
	int pause_flag;
    int game_over_flag;
    int new_game_flag;
} GameState;

#endif // SHARED_H
