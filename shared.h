#ifndef SHARED_H
#define SHARED_H

#define MAX_WORLD_WIDTH  40
#define MAX_WORLD_HEIGHT 40
#define MIN_WORLD_WIDTH  10
#define MIN_WORLD_HEIGHT 10
#define MAX_SNAKE_LENGTH 200
#define MAX_OBSTACLES    50

#define SHM_KEY 54321
#define SEM_SERVER_READY "/sem_server_ready_TM"
#define SEM_CLIENT_READY "/sem_client_ready_TM"
#define SEM_INIT_DONE "/sem_init_done_TM"

typedef enum {
    DIRECTION_UP,
    DIRECTION_DOWN,
    DIRECTION_LEFT,
    DIRECTION_RIGHT
} Direction;

typedef enum {
    WORLD_TYPE_EMPTY,
    WORLD_TYPE_OBSTACLES
} WorldType;

typedef struct {
    int x;
    int y;
} Position;

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

    int timed_mode;
    int time_limit;
    int elapsed_time;

	int pause_flag;
    int game_over_flag;
    int new_game_flag;
} GameState;

#endif // SHARED_H
