#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <pthread.h>
#include <semaphore.h>
#include <fcntl.h>
#include <unistd.h>
#include "shared.h"

#define SHM_KEY 54321
#define SEM_SERVER_READY "/sem_server_ready_TM"
#define SEM_CLIENT_READY "/sem_client_ready_TM"

typedef struct {
    pthread_mutex_t game_mutex;
    GameState state;
} SharedData;

typedef struct {
    SharedData *shared_data;
    sem_t *sem_client_ready;
    sem_t *sem_server_ready;
    int running;
    pthread_t input_tid;
} ThreadArgs;

void render_world(GameState *state) {
    char world[MAX_WORLD_HEIGHT][MAX_WORLD_WIDTH];

    for (int y = 0; y < state->height; y++) {
        for (int x = 0; x < state->width; x++) {
            world[y][x] = '.';
        }
    }

    world[state->fruit.y][state->fruit.x] = 'F';

    for (int i = 0; i < state->snake_length; i++) {
        Position part = state->snake[i];
        world[part.y][part.x] = (i == 0) ? 'H' : 'o';  // Hlava: 'H', telo: 'o'
    }

    for (int i = 0; i < state->num_obstacles; i++) {
        Position obstacle = state->obstacles[i];
        world[obstacle.y][obstacle.x] = '#';
    }

    printf("\033[H\033[J"); 
    for (int y = 0; y < state->height; y++) {
        for (int x = 0; x < state->width; x++) {
            switch (world[y][x]) {
                case 'H': 
                    printf("\033[93mH\033[0m ");
                    break;
                case 'o':  
                    printf("\033[92mo\033[0m ");
                    break;
                case 'F':  
                    printf("\033[38;5;205mF\033[0m ");
                    break;
                case '#':  
                    printf("\033[38;5;51m#\033[0m ");
                    break;
                default:  
                    printf(". ");
                    break;
            }
        }
        printf("\n");
    }

    printf("Skóre: %d\n", state->score);
    printf("Čas od začiatku hry: %d sekúnd\n", state->elapsed_time);
    if (state->timed_mode) {
        printf("Zostávajúci čas: %d sekúnd\n", state->time_limit - state->elapsed_time);
    }
}


void show_menu(GameState *state) {
    printf("\n=== Nastavenie hry ===\n");
    printf("Zadajte šírku herného sveta: ");
    scanf("%d", &state->width);
    if (state->width < 10) {
        state->width = 10;
    } else if (state->width > 40) {
        state->width = 40;
    }

    printf("Zadajte výšku herného sveta: ");
    scanf("%d", &state->height);
    if (state->height < 10) {
        state->height = 10;
    } else if (state->height > 40) {
        state->height = 40;
    }

    printf("Zadajte typ sveta (0 = bez prekážok, 1 = s prekážkami): ");
    int world_type;
    scanf("%d", &world_type);
    state->world_type = (world_type != 1) ? WORLD_TYPE_EMPTY : WORLD_TYPE_OBSTACLES;

    printf("Zadajte režim hry (0 = štandardný, 1 = časový): ");
    int game_type;
    scanf("%d", &game_type);
    state->timed_mode = (game_type == 1);

    if (state->timed_mode) {
        printf("Zadajte časový limit (v sekundách): ");
        scanf("%d", &state->time_limit);
    } else {
        state->time_limit = 0;
    }
}

void pause_menu(ThreadArgs *threadArgs) {
    SharedData *shared_data = threadArgs->shared_data;
    int choice;

    printf("\n=== Hra pozastavená ===\n");
    printf("1. Pokračovať v hre\n");
    printf("2. Spustiť novú hru\n");
    printf("3. Ukončiť aplikáciu\n");
    printf("Zvoľte možnosť: ");
    scanf("%d", &choice);

    if (choice == 1) {
        printf("\nPokračovanie hry za:");
        for (int i = 3; i > 0; i--) {
            printf(" %d...", i);
            fflush(stdout);
            sleep(1);
        }
        pthread_mutex_lock(&shared_data->game_mutex);
        shared_data->state.pause_flag = 0;
        pthread_mutex_unlock(&shared_data->game_mutex);
        sem_post(threadArgs->sem_client_ready);
    } else if (choice == 2) {
        printf("\033[H\033[J");
        printf("\nSpúšťanie novej hry...\n");
        pthread_mutex_lock(&shared_data->game_mutex);
        shared_data->state.new_game_flag = 1;
        pthread_mutex_unlock(&shared_data->game_mutex);
        sem_post(threadArgs->sem_client_ready);

        sem_wait(threadArgs->sem_server_ready);

        show_menu(&shared_data->state);

        pthread_mutex_lock(&shared_data->game_mutex);
        shared_data->state.new_game_flag = 0;
        shared_data->state.pause_flag = 0;
        pthread_mutex_unlock(&shared_data->game_mutex);
        sem_post(threadArgs->sem_client_ready);
    } else if (choice == 3) {
        printf("Ukončovanie aplikácie...\n");
        threadArgs->running = 0;
        pthread_cancel(threadArgs->input_tid);

        pthread_mutex_lock(&shared_data->game_mutex);
        shared_data->state.pause_flag = 0;
        shared_data->state.game_over_flag = 1;
        pthread_mutex_unlock(&shared_data->game_mutex);

        sem_post(threadArgs->sem_client_ready);
    } else {
        printf("Neplatná voľba, pokračujeme v hre.\n");
        pthread_mutex_lock(&shared_data->game_mutex);
        shared_data->state.pause_flag = 0;
        pthread_mutex_unlock(&shared_data->game_mutex);
        sem_post(threadArgs->sem_client_ready);
    }
}

void *input_thread(void *arg) {
    ThreadArgs *args = (ThreadArgs *)arg;
    SharedData *shared_data = args->shared_data;
    char input;

    while (args->running) {
        input = getchar();

        if (input == 'p') {
            pthread_mutex_lock(&shared_data->game_mutex);
            shared_data->state.pause_flag = 1;
            pthread_mutex_unlock(&shared_data->game_mutex);
            pause_menu(args);
            continue;
        }

        Direction new_direction;
        switch (input) {
            case 'w': new_direction = DIRECTION_UP; break;
            case 's': new_direction = DIRECTION_DOWN; break;
            case 'a': new_direction = DIRECTION_LEFT; break;
            case 'd': new_direction = DIRECTION_RIGHT; break;
            default: continue;
        }

        pthread_mutex_lock(&shared_data->game_mutex);
        shared_data->state.direction = new_direction;
        pthread_mutex_unlock(&shared_data->game_mutex);
        sem_post(args->sem_client_ready);
    }
    return NULL;
}

int main() {
    pid_t pid = fork();

    if (pid == -1) {
        perror("Chyba pri vytváraní procesu servera");
        exit(1);
    } else if (pid == 0) {
        execl("./server", "./server", (char *)NULL);
        perror("Chyba pri spustení servera");
        exit(1);
    }

    sem_t *sem_init_done = sem_open("/sem_init_done", O_CREAT, 0666, 0);
    if (sem_init_done == SEM_FAILED) {
        perror("Chyba pri vytváraní semaforu inicializácie");
        exit(1);
    }

    sem_wait(sem_init_done);
    sem_close(sem_init_done);
    sem_unlink("/sem_init_done");


    int shmid;
    SharedData *shared_data;
    sem_t *sem_server_ready, *sem_client_ready;

    shmid = shmget(SHM_KEY, sizeof(SharedData), 0666);
    if (shmid == -1) {
        perror("Chyba pri pripojení k zdieľanej pamäti");
        exit(1);
    }

    shared_data = (SharedData *)shmat(shmid, NULL, 0);
    if (shared_data == (void *)-1) {
        perror("Chyba pri pripájaní k zdieľanej pamäti");
        exit(1);
    }

    sem_server_ready = sem_open(SEM_SERVER_READY, 0);
    sem_client_ready = sem_open(SEM_CLIENT_READY, 0);
    if (sem_server_ready == SEM_FAILED || sem_client_ready == SEM_FAILED) {
        perror("Chyba pri pripájaní k semaforom");
        exit(1);
    }

    show_menu(&shared_data->state);

    pthread_mutex_lock(&shared_data->game_mutex);
    sem_post(sem_client_ready);
    pthread_mutex_unlock(&shared_data->game_mutex);

    ThreadArgs args;
    args.shared_data = shared_data;
    args.sem_client_ready = sem_client_ready;
    args.sem_server_ready = sem_server_ready;
    args.running = 1;

    pthread_create(&args.input_tid, NULL, input_thread, &args);

    while (args.running) {
        pthread_mutex_lock(&shared_data->game_mutex);
        if (shared_data->state.game_over_flag) {
            pthread_mutex_unlock(&shared_data->game_mutex);
            break;
        }
        pthread_mutex_unlock(&shared_data->game_mutex);

        if (sem_trywait(sem_server_ready) == 0) {
            pthread_mutex_lock(&shared_data->game_mutex);
            if (!shared_data->state.pause_flag) {
                render_world(&shared_data->state);
            }
            pthread_mutex_unlock(&shared_data->game_mutex);
        } else {
            usleep(100000);
        }
    }

    pthread_cancel(args.input_tid);
    pthread_join(args.input_tid, NULL);

    sem_close(sem_server_ready);
    sem_close(sem_client_ready);
    shmdt(shared_data);
    return 0;
}
