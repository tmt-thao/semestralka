#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <pthread.h>


#include "shared.h"

int connect_to_shm(Resources *res) {
    res->shmid = shmget(SHM_KEY, sizeof(SharedData), IPC_CREAT | 0666);
    if (res->shmid == -1) {
        perror("Chyba pri vytváraní zdieľanej pamäte");
        return 1;
    }

    // Pripojenie k zdieľanej pamäti
    res->shared_data = (SharedData *)shmat(res->shmid, NULL, 0);
    if (res->shared_data == (void *)-1) {
        perror("Chyba pri pripájaní k zdieľanej pamäti");
        return 1;
    }

    return 0;
}

int connect_to_sem(Resources *res) {
    res->sem_server_ready = sem_open(SEM_SERVER_READY, O_CREAT, 0666, 0);
    res->sem_client_ready = sem_open(SEM_CLIENT_READY, O_CREAT, 0666, 0);
    if (res->sem_server_ready == SEM_FAILED || res->sem_client_ready == SEM_FAILED) {
        perror("Chyba pri vytváraní semaforov");
        return 1;
    }

    return 0;
}

void cleanup(Resources *res) {
    if (res->shared_data != (void *)-1) {
        pthread_mutex_destroy(&res->shared_data->mutex);
        shmdt(res->shared_data);
        shmctl(res->shmid, IPC_RMID, NULL);
    }

    sem_close(res->sem_server_ready);
    sem_close(res->sem_client_ready);
    sem_unlink(SEM_SERVER_READY);
    sem_unlink(SEM_CLIENT_READY);

    printf("Server: Korektne ukončený.\n");
}

void generate_fruit(GameGrid *game_grid) {
    int x, y;
    do {
        x = rand() % game_grid->width;
        y = rand() % game_grid->height;
    } while (game_grid->grid[y][x] != EMPTY_GRID);  // Náhodná pozícia musí byť prázdna

    game_grid->fruit.position.x = x;
    game_grid->fruit.position.y = y;
    game_grid->grid[y][x] = FRUIT;
}

void initialize_game(Game *game, int width, int height, WorldType world_type, GameType game_type) {
    game->grid.width = width;
    game->grid.height = height;
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            game->grid.grid[y][x] = EMPTY_GRID;
        }
    }

    game->world_type = world_type;
    game->game_type = game_type;
    game->score = 0;
    game->status = GAME_RUNNING;
    game->elapsed_time = 0;
    game->time_limit = (game_type == TIMED) ? 5 : 0;  // Nastavenie časového limitu, ak je časový režim

    // Inicializácia hadíka
    game->grid.snake.length = 1;
    game->grid.snake.body[0].x = width / 2;
    game->grid.snake.body[0].y = height / 2;
    game->grid.snake.direction = UP;
    game->grid.grid[height / 2][width / 2] = SNAKE_HEAD;

    // Generovanie ovocia
    generate_fruit(&game->grid);
}

void update_snake_position(Game *game) {
    Snake *snake = &game->grid.snake;
    Position new_head = snake->body[0];

    // Vymaž staré telo hadíka z gridu
    for (int i = 0; i < snake->length; i++) {
        game->grid.grid[snake->body[i].y][snake->body[i].x] = EMPTY_GRID;
    }

    // Aktualizácia hlavy hadíka podľa smeru
    switch (snake->direction) {
        case UP:    new_head.y--; break;
        case DOWN:  new_head.y++; break;
        case LEFT:  new_head.x--; break;
        case RIGHT: new_head.x++; break;
    }

    // Kontrola kolízie so stenou alebo prechod na opačnú stranu
    if (new_head.x < 0 || new_head.x >= game->grid.width ||
        new_head.y < 0 || new_head.y >= game->grid.height) {
        if (game->world_type == WRAP_AROUND) {
            if (new_head.x < 0) new_head.x = game->grid.width - 1;
            if (new_head.x >= game->grid.width) new_head.x = 0;
            if (new_head.y < 0) new_head.y = game->grid.height - 1;
            if (new_head.y >= game->grid.height) new_head.y = 0;
        } else {
            game->status = GAME_OVER;  // Kolízia so stenou
            return;
        }
    }

    // Kontrola kolízie so sebou samým
    for (int i = 0; i < snake->length; i++) {
        if (new_head.x == snake->body[i].x && new_head.y == snake->body[i].y) {
            game->status = GAME_OVER;
            return;
        }
    }

    // Kontrola zjedenia ovocia
    if (new_head.x == game->grid.fruit.position.x && new_head.y == game->grid.fruit.position.y) {
        game->score++;
        snake->length++;
        generate_fruit(&game->grid);  // Vygenerovanie nového ovocia
    }

    // Posun tela hadíka
    for (int i = snake->length - 1; i > 0; i--) {
        snake->body[i] = snake->body[i - 1];
    }
    snake->body[0] = new_head;

    // Aktualizácia gridu pre nové telo hadíka
    game->grid.grid[new_head.y][new_head.x] = SNAKE_HEAD;
    for (int i = 1; i < snake->length; i++) {
        game->grid.grid[snake->body[i].y][snake->body[i].x] = SNAKE_BODY;
    }
}

void run_game_cycle(Resources *res) {
    time_t start_time = time(NULL);
    while (1) {
        usleep(GAME_TICK_INTERVAL);  // Počkaj na ďalší tick

        pthread_mutex_lock(&res->shared_data->mutex);
        update_snake_position(&res->shared_data->game);  // Aktualizácia pozície hadíka

        // Aktualizuj čas
        res->shared_data->game.elapsed_time = time(NULL) - start_time;

        // Ak je časový režim a došiel stanovený čas
        if (res->shared_data->game.game_type == TIMED &&
            res->shared_data->game.elapsed_time >= res->shared_data->game.time_limit) {
            res->shared_data->game.status = GAME_OVER;
        }

        // Skontroluj, či hra skončila
        if (res->shared_data->game.status == GAME_OVER) {
            pthread_mutex_unlock(&res->shared_data->mutex);  // Odomknutie mutexu pred ukončením
            sem_post(res->sem_server_ready);  // Poslanie signálu klientovi, že hra skončila
            break;  // Ukončenie herného cyklu
        }

        pthread_mutex_unlock(&res->shared_data->mutex);

        // Odošli nový stav hry klientovi
        sem_post(res->sem_server_ready);
    }

    printf("Server: Herný cyklus ukončený.\n");
}

void *game_thread(void *arg) {
    Resources *res = (Resources *)arg;
    run_game_cycle(res);  // Spustenie herného cyklu vo vlákne
    return NULL;
}

int main() {
    Resources res;
    pthread_t game_tid;

    // Vytvorenie a pripojenie k zdieľanej pamäti
    if (connect_to_shm(&res) != 0) {
        return 1;
    }

    // Inicializácia mutexu
    if (pthread_mutex_init(&res.shared_data->mutex, NULL) != 0) {
        perror("Chyba pri inicializácii mutexu");
        cleanup(&res);
        return 1;
    }

    // Vytvorenie semaforov
    if (connect_to_sem(&res) != 0) {
        cleanup(&res);
        return 1;
    }

    printf("Server: Inicializácia úspešná.\n");

    srand(time(NULL));

    // Inicializácia herného sveta
    initialize_game(&res.shared_data->game, 20, 20, WRAP_AROUND, TIMED);
    printf("Server: Herný svet inicializovaný, spúšťam hru.\n");

    // Vytvorenie herného vlákna
    if (pthread_create(&game_tid, NULL, game_thread, &res) != 0) {
        perror("Chyba pri vytváraní herného vlákna");
        cleanup(&res);
        return 1;
    }

    // Počkaj na ukončenie herného vlákna
    pthread_join(game_tid, NULL);
    printf("Server: Hra skončila. Skóre: %d\n", res.shared_data->game.score);

    cleanup(&res);
    return 0;
}