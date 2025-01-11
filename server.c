#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <signal.h>

#include "shared.h"

void init_shared_memory(SharedData **shared_data, int *shm_id) {
    *shm_id = shmget(SHM_KEY, sizeof(SharedData), IPC_CREAT | 0666);
    if (*shm_id < 0) {
        perror("shmget");
        exit(1);
    }

    *shared_data = (SharedData *)shmat(*shm_id, NULL, 0);
    if (*shared_data == (SharedData *)-1) {
        perror("shmat");
        exit(1);
    }

    printf("Server: Zdieľaná pamäť inicializovaná.\n");
}

void init_sync_primitives(SharedData *shared_data) {
    pthread_mutex_init(&shared_data->mutex, NULL);
    sem_init(&shared_data->sem_server_ready, 1, 0);
    sem_init(&shared_data->sem_client_ready, 1, 0);
    printf("Server: Mutex a semafory inicializované.\n");
}

void wait_for_client(SharedData *shared_data) {
    printf("Server: Čakám na klienta...\n");
    sem_wait(&shared_data->sem_client_ready);  // Čaká na signál od klienta
    printf("Server: Prijal som signál od klienta.\n");
}

void cleanup_resources(SharedData *shared_data, int shm_id) {
    pthread_mutex_destroy(&shared_data->mutex);
    sem_destroy(&shared_data->sem_server_ready);
    sem_destroy(&shared_data->sem_client_ready);
    shmdt(shared_data);
    shmctl(shm_id, IPC_RMID, NULL);
    printf("Server: Všetky zdroje boli korektne uvoľnené.\n");
}

void generate_fruit(Game *game) {
    do {
        game->grid.fruit.position.x = rand() % game->grid.width;
        game->grid.fruit.position.y = rand() % game->grid.height;
    } while (game->grid.grid[game->grid.fruit.position.y][game->grid.fruit.position.x] != EMPTY_GRID);

    game->grid.grid[game->grid.fruit.position.y][game->grid.fruit.position.x] = FRUIT;
    printf("Server: Vygenerované ovocie na [%d, %d].\n", game->grid.fruit.position.x, game->grid.fruit.position.y);
}

// Funkcia na inicializáciu herného sveta
void init_game(Game *game) {
    game->grid.width = 20;
    game->grid.height = 20;
    game->score = 0;
    game->status = GAME_RUNNING;

    // Vyplnenie mriežky prázdnymi políčkami
    for (int y = 0; y < game->grid.height; y++) {
        for (int x = 0; x < game->grid.width; x++) {
            game->grid.grid[y][x] = EMPTY_GRID;
        }
    }

    // Umiestnenie hadíka do stredu mriežky
    game->grid.snake.length = 1;
    game->grid.snake.body[0].x = game->grid.width / 2;
    game->grid.snake.body[0].y = game->grid.height / 2;
    game->grid.snake.direction = RIGHT;
    game->grid.grid[game->grid.snake.body[0].y][game->grid.snake.body[0].x] = SNAKE_HEAD;

    // Náhodné vygenerovanie ovocia
    generate_fruit(game);

    printf("Server: Herný svet inicializovaný.\n");
}

// Funkcia na aktualizáciu pozície hadíka
void move_snake(Game *game) {
    // Pred posunutím hadíka nastavíme staré pozície na EMPTY_GRID
    for (int i = 0; i < game->grid.snake.length; i++) {
        Position pos = game->grid.snake.body[i];
        game->grid.grid[pos.y][pos.x] = EMPTY_GRID;
    }

    Position new_head = game->grid.snake.body[0];

    // Určenie novej pozície hlavy hadíka podľa smeru
    switch (game->grid.snake.direction) {
        case UP:    new_head.y = (new_head.y - 1 + game->grid.height) % game->grid.height; break;
        case DOWN:  new_head.y = (new_head.y + 1) % game->grid.height; break;
        case LEFT:  new_head.x = (new_head.x - 1 + game->grid.width) % game->grid.width; break;
        case RIGHT: new_head.x = (new_head.x + 1) % game->grid.width; break;
    }

    // Skontrolovanie, či hadík zje ovocie
    if (new_head.x == game->grid.fruit.position.x && new_head.y == game->grid.fruit.position.y) {
        game->score += 10;
        game->grid.snake.length++;
        
        generate_fruit(game);
    }

    // Posun tela hadíka
    for (int i = game->grid.snake.length - 1; i > 0; i--) {
        game->grid.snake.body[i] = game->grid.snake.body[i - 1];
    }

    // Aktualizácia pozície hlavy hadíka
    game->grid.snake.body[0] = new_head;
    game->grid.grid[new_head.y][new_head.x] = SNAKE_HEAD;

    printf("Server: Hadík sa pohol na [%d, %d].\n", new_head.x, new_head.y);
}

int main() {
    int shm_id;
    SharedData *shared_data;

    init_shared_memory(&shared_data, &shm_id);
    init_sync_primitives(shared_data);

    srand(time(NULL));

    // Inicializácia herného sveta
    pthread_mutex_lock(&shared_data->mutex);
    init_game(&shared_data->game);
    pthread_mutex_unlock(&shared_data->mutex);

    // Herná slučka (zatiaľ len pevný počet iterácií)
    for (int i = 0; i < 30; i++) {
        pthread_mutex_lock(&shared_data->mutex);
        move_snake(&shared_data->game);
        pthread_mutex_unlock(&shared_data->mutex);

        sem_post(&shared_data->sem_server_ready);
        sem_wait(&shared_data->sem_client_ready);
    
        usleep(GAME_TICK_INTERVAL);
    }

    cleanup_resources(shared_data, shm_id);
    printf("Server: Ukončenie programu.\n");
    return 0;
}
