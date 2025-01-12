#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <pthread.h>
#include <semaphore.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include "shared.h"

#define SHM_KEY 54321
#define SEM_SERVER_READY "/sem_server_ready_TM"
#define SEM_CLIENT_READY "/sem_client_ready_TM"

typedef struct {
    pthread_mutex_t game_mutex;
    GameState state;
} SharedData;

int is_on_snake(GameState *state, Position pos) {
    for (int i = 0; i < state->snake_length; i++) {
        if (state->snake[i].x == pos.x && state->snake[i].y == pos.y) {
            return 1;
        }
    }
    return 0;
}

int is_on_obstacle(GameState *state, Position pos) {
    for (int i = 0; i < state->num_obstacles; i++) {
        if (state->obstacles[i].x == pos.x && state->obstacles[i].y == pos.y) {
            return 1;
        }
    }
    return 0;
}

void generate_fruit(GameState *state) {
    Position new_fruit;
    do {
        new_fruit.x = rand() % state->width;
        new_fruit.y = rand() % state->height;
    } while (is_on_snake(state, new_fruit) || is_on_obstacle(state, new_fruit));
    state->fruit = new_fruit;
}

void generate_obstacles(GameState *state) {
    int max_obstacles = (state->width * state->height) / 20;
    if (max_obstacles > MAX_OBSTACLES) {
        max_obstacles = MAX_OBSTACLES;
    }
    state->num_obstacles = max_obstacles;

    for (int i = 0; i < max_obstacles; i++) {
        Position new_obstacle;
        do {
            new_obstacle.x = rand() % state->width;
            new_obstacle.y = rand() % state->height;
        } while (is_on_snake(state, new_obstacle) ||
                 (new_obstacle.x == state->fruit.x && new_obstacle.y == state->fruit.y));
        state->obstacles[i] = new_obstacle;
    }
}

void move_snake(GameState *state) {
    Position new_head = state->snake[0];

    switch (state->direction) {
        case DIRECTION_UP: new_head.y--; break;
        case DIRECTION_DOWN: new_head.y++; break;
        case DIRECTION_LEFT: new_head.x--; break;
        case DIRECTION_RIGHT: new_head.x++; break;
    }

    if (state->world_type == WORLD_TYPE_EMPTY) {
        if (new_head.x < 0) new_head.x = state->width - 1;
        if (new_head.x >= state->width) new_head.x = 0;
        if (new_head.y < 0) new_head.y = state->height - 1;
        if (new_head.y >= state->height) new_head.y = 0;
    } else {
        if (new_head.x < 0 || new_head.x >= state->width ||
            new_head.y < 0 || new_head.y >= state->height) {
            state->game_over_flag = -1;
            return;
        }
    }

    for (int i = 0; i < state->num_obstacles; i++) {
        if (new_head.x == state->obstacles[i].x && new_head.y == state->obstacles[i].y) {
            state->game_over_flag = -1;
            return;
        }
    }

    for (int i = 1; i < state->snake_length; i++) {
        if (new_head.x == state->snake[i].x && new_head.y == state->snake[i].y) {
            state->game_over_flag = -1;
            return;
        }
    }

    if (new_head.x == state->fruit.x && new_head.y == state->fruit.y) {
        state->snake_length++;
        state->score += 10;
        generate_fruit(state);
    }

    for (int i = state->snake_length - 1; i > 0; i--) {
        state->snake[i] = state->snake[i - 1];
    }
    state->snake[0] = new_head;
}

void initialize_game(GameState *state) {
    state->snake_length = 1;
    state->snake[0].x = state->width / 2;
    state->snake[0].y = state->height / 2;
    state->score = 0;
    state->elapsed_time = 0;
    generate_fruit(state);
    state->num_obstacles = 0;
    if (state->world_type == WORLD_TYPE_OBSTACLES) {
        generate_obstacles(state);
    }
    state->pause_flag = 0;
    state->game_over_flag = 0;
}

int main() {
    int shmid;
    SharedData *shared_data;
    sem_t *sem_server_ready, *sem_client_ready;

    shmid = shmget(SHM_KEY, sizeof(SharedData), IPC_CREAT | 0666);
    if (shmid == -1) {
        perror("Chyba pri vytváraní zdieľanej pamäte");
        exit(1);
    }

    shared_data = (SharedData *)shmat(shmid, NULL, 0);
    if (shared_data == (void *)-1) {
        perror("Chyba pri pripájaní k zdieľanej pamäti");
        exit(1);
    }

    if (pthread_mutex_init(&shared_data->game_mutex, NULL) != 0) {
        perror("Chyba pri inicializácii mutexu");
        exit(1);
    }

    sem_server_ready = sem_open(SEM_SERVER_READY, O_CREAT, 0666, 0);
    sem_client_ready = sem_open(SEM_CLIENT_READY, O_CREAT, 0666, 0);
    if (sem_server_ready == SEM_FAILED || sem_client_ready == SEM_FAILED) {
        perror("Chyba pri vytváraní semaforov");
        exit(1);
    }

    sem_t *sem_init_done = sem_open("/sem_init_done", O_CREAT, 0666, 0);
    if (sem_init_done == SEM_FAILED) {
        perror("Chyba pri vytváraní semaforu inicializácie");
        exit(1);
    }
    sem_post(sem_init_done);  // Signalizácia klientovi, že server je pripravený
    sem_close(sem_init_done);


    sem_wait(sem_client_ready);

    srand(time(NULL));
    initialize_game(&shared_data->state);

    Direction prev_direction = shared_data->state.direction;
    time_t start_time = time(NULL);

    while (1) {
        usleep(300000);

        pthread_mutex_lock(&shared_data->game_mutex);
        shared_data->state.elapsed_time = time(NULL) - start_time;
        if (shared_data->state.timed_mode &&
            shared_data->state.elapsed_time >= shared_data->state.time_limit) {
            shared_data->state.game_over_flag = -1;
            pthread_mutex_unlock(&shared_data->game_mutex);
            break;
        }

        if (shared_data->state.new_game_flag == 1) {
            pthread_mutex_unlock(&shared_data->game_mutex);

            // Čakanie na signál od klienta, že sú nové nastavenia pripravené
            sem_wait(sem_client_ready);

            pthread_mutex_lock(&shared_data->game_mutex);
            initialize_game(&shared_data->state);
            start_time = time(NULL);
            shared_data->state.new_game_flag = 0;
            pthread_mutex_unlock(&shared_data->game_mutex);

            // Signalizácia klientovi, že server je pripravený
            sem_post(sem_server_ready);
            continue;
        }

        if (shared_data->state.pause_flag) {
            pthread_mutex_unlock(&shared_data->game_mutex);
            sem_wait(sem_client_ready);

            if (shared_data->state.game_over_flag == 1) {
                pthread_mutex_unlock(&shared_data->game_mutex);
                break;
            }
        }
        pthread_mutex_unlock(&shared_data->game_mutex);

        if (sem_trywait(sem_client_ready) == 0) {
            pthread_mutex_lock(&shared_data->game_mutex);
            Direction new_direction = shared_data->state.direction;
            if (!((new_direction == DIRECTION_UP && prev_direction == DIRECTION_DOWN) ||
                  (new_direction == DIRECTION_DOWN && prev_direction == DIRECTION_UP) ||
                  (new_direction == DIRECTION_LEFT && prev_direction == DIRECTION_RIGHT) ||
                  (new_direction == DIRECTION_RIGHT && prev_direction == DIRECTION_LEFT))) {
                prev_direction = new_direction;
            }
            pthread_mutex_unlock(&shared_data->game_mutex);
        }

        pthread_mutex_lock(&shared_data->game_mutex);
        shared_data->state.direction = prev_direction;
        move_snake(&shared_data->state);
        if (shared_data->state.game_over_flag == -1) {
            pthread_mutex_unlock(&shared_data->game_mutex);
            break;
        }
        pthread_mutex_unlock(&shared_data->game_mutex);

        sem_post(sem_server_ready);
    }

    pthread_mutex_destroy(&shared_data->game_mutex);
    sem_close(sem_server_ready);
    sem_close(sem_client_ready);
    sem_unlink(SEM_SERVER_READY);
    sem_unlink(SEM_CLIENT_READY);
    shmdt(shared_data);
    shmctl(shmid, IPC_RMID, NULL);
    return 0;
}
