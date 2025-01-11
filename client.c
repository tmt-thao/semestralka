#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <semaphore.h>
#include <sys/wait.h>
#include "shared.h"

#include <signal.h>

void start_server_if_not_running() {
    int shm_id = shmget(SHM_KEY, sizeof(SharedData), 0666);
    if (shm_id < 0) {
        printf("Client: Server nie je spustený. Spúšťam nový server...\n");

        pid_t server_pid = fork();
        if (server_pid < 0) {
            perror("fork");
            exit(1);
        }

        if (server_pid == 0) {
            execl("./server", "server", NULL);
            perror("execl");
            exit(1);
        }

        sleep(1);  // Čaká, kým sa server inicializuje
    }
}

void connect_to_shared_memory(SharedData **shared_data, int *shm_id) {
    *shm_id = shmget(SHM_KEY, sizeof(SharedData), 0666);
    if (*shm_id < 0) {
        perror("shmget");
        exit(1);
    }

    *shared_data = (SharedData *)shmat(*shm_id, NULL, 0);
    if (*shared_data == (SharedData *)-1) {
        perror("shmat");
        exit(1);
    }

    printf("Client: Pripojenie k zdieľanej pamäti úspešné.\n");
}

void signal_server(SharedData *shared_data) {
    sem_post(&shared_data->sem_client_ready);
    printf("Client: Odoslaný signál serveru.\n");
}

// Funkcia na vykreslenie hernej mriežky
void draw_game_grid(Game *game) {
    printf("\033[H\033[J");
    printf("\nAktuálny stav hry:\n");
    for (int y = 0; y < game->grid.height; y++) {
        for (int x = 0; x < game->grid.width; x++) {
            printf("%c ", game->grid.grid[y][x]);
        }
        printf("\n");
    }
    printf("Skóre: %d\n", game->score);
}

int main() {
    int shm_id;
    SharedData *shared_data;

    start_server_if_not_running();
    connect_to_shared_memory(&shared_data, &shm_id);

    // Hlavný cyklus klienta – prijímanie stavu hry a vykreslenie mriežky
    for (int i = 0; i < 30; i++) {  // Zatiaľ pevný počet iterácií
        sem_wait(&shared_data->sem_server_ready);  // Čakanie na signál od servera

        pthread_mutex_lock(&shared_data->mutex);  // Zamknutie na čítanie zdieľaných dát
        draw_game_grid(&shared_data->game);
        pthread_mutex_unlock(&shared_data->mutex);  // Odomknutie zdieľaných dát

        sem_post(&shared_data->sem_client_ready);  // Signalizácia serveru
    }

    shmdt(shared_data);
    printf("Client: Ukončenie programu.\n");
    return 0;
}
