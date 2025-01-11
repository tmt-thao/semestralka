#ifndef SHARED_H
#define SHARED_H

#include <pthread.h>
#include <semaphore.h>

#define MAX_WIDTH 50
#define MAX_HEIGHT 50
#define MAX_SNAKE_LENGTH 200
#define MAX_OBSTACLES 50

#define GAME_TICK_INTERVAL 500000    
#define GAME_CONTINUE_TIMEOUT 3000000

#define SHM_KEY 37915

typedef enum {
    UP      = 'w',
    LEFT    = 'a',
    DOWN    = 's',
    RIGHT   = 'd'
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
    SNAKE_BODY  = 'O',
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
    GridValue grid[MAX_HEIGHT][MAX_WIDTH];
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

    char input;
} Game;

typedef struct {
    Game game;
    pthread_mutex_t mutex;
    sem_t sem_server_ready;
    sem_t sem_client_ready;
} SharedData;

typedef struct {
    int shmid;
    SharedData *shared_data;
} Resources;

#endif // SHARED_H
