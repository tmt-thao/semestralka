#ifndef SHARED_H
#define SHARED_H

#include <pthread.h>
#include <semaphore.h>

#define MAX_WORLD_WIDTH     50
#define MAX_WORLD_HEIGHT    50
#define MAX_SNAKE_LENGTH    200
#define MAX_OBSTACLES       50

#define TICK                5000000    
#define CONTINUE_GAME       30000000

#define SHM_KEY             55555
#define SEM_SERVER_READY    "/sem_server_ready_TM"
#define SEM_CLIENT_READY    "/sem_client_ready_TM"

typedef enum {
    UP      = 'W',
    LEFT    = 'A',
    DOWN    = 'S',
    RIGHT   = 'D'
} Direction;

typedef enum {
    WRAP_AROUND,
    OBSTACLES
} WorldType;

typedef enum {
    STANDARD,
    TIMED
} GameType;

typedef enum {
    EMPTY_GRID  = '.',
    SNAKE_HEAD  = 'H',
    SNAKE_BODY  = 'o',
    FRUIT       = 'F',
    OBSTACLE    = '#'
} GridValue;

typedef enum {
    PAUSE_CONTINUE,
    PAUSE_RESTART,
    PAUSE_END_GAME
} PauseChoice;

typedef enum {
    GAME_RUNNING,
    GAME_PAUSED,
    GAME_OVER
} GameStatus;

typedef struct {
    int x;
    int y;
} Position;

typedef struct {
    Position position;
} Fruit;

typedef struct {
    int length;
    Position body[MAX_SNAKE_LENGTH];
    Direction direction;
} Snake;

typedef struct {
    int count;
    Position map[MAX_OBSTACLES];
} Obstacles;

typedef struct {
    int width;
    int height;
    GridValue grid[MAX_WORLD_WIDTH][MAX_WORLD_HEIGHT];
    Fruit fruit;
    Snake snake;
    Obstacles obstacles;
} GameGrid;

typedef struct {
    GameGrid grid;
    int score;

    WorldType world_type;
    GameType game_type;

    GameStatus status;
    PauseChoice pause_choice;
    
    int elapsed_time;
    int time_limit;
} Game;

typedef struct {
    Game game;
    int dummy_data;
    pthread_mutex_t mutex;
} SharedData;

typedef struct {
    int shmid;
    SharedData *shared_data;
    sem_t *sem_server_ready;
    sem_t *sem_client_ready;
} Resources;

#endif // SHARED_H
