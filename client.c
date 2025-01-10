#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>

#include "shared.h"

int connect_to_shm(Resources *res) {
    res->shmid = shmget(SHM_KEY, sizeof(SharedData), 0666);
    if (res->shmid == -1) {
        printf("Klient: Server ešte nebeží, spúšťam ho...\n");

        // Vytvorenie nového procesu pre server
        pid_t pid = fork();
        if (pid == 0) {
            // Detský proces spustí server
            execl("./server", "./server", NULL);
            perror("Chyba pri spustení servera");
            return 1;
        } else if (pid < 0) {
            perror("Chyba pri vytváraní procesu pre server");
            return 1;
        }

        // Čakanie na vytvorenie zdieľanej pamäte serverom
        sleep(2);  // Krátke čakanie, aby sa server inicializoval

        // Znova sa pokús o pripojenie
        res->shmid = shmget(SHM_KEY, sizeof(SharedData), 0666);
        if (res->shmid == -1) {
            perror("Chyba pri pripojení k zdieľanej pamäti");
            return 1;
        }
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
    res->sem_server_ready = sem_open(SEM_SERVER_READY, 0);
    res->sem_client_ready = sem_open(SEM_CLIENT_READY, 0);
    if (res->sem_server_ready == SEM_FAILED || res->sem_client_ready == SEM_FAILED) {
        perror("Chyba pri pripájaní k semaforom");
        return 1;
    }

    return 0;
}

void cleanup(Resources *res) {
    if (res->shared_data != (void *)-1) {
        shmdt(res->shared_data);
    }
    sem_close(res->sem_server_ready);
    sem_close(res->sem_client_ready);

    printf("Klient: Korektne ukončený.\n");
}

void *input_thread(void *arg) {
    Resources *res = (Resources *)arg;
    char input;

    printf("Klient: Začínam spracovanie vstupu.\n");

    while (1) {
        input = getchar();

        pthread_mutex_lock(&res->shared_data->mutex);

        if (res->shared_data->game.status == GAME_OVER) {
            pthread_mutex_unlock(&res->shared_data->mutex);
            break;  // Ukonči vlákno, ak je hra skončená
        }

        if (input == 'p') {
            res->shared_data->game.pause_choice = PAUSE_CONTINUE;  // Príklad spracovania pauzy
        } else if (input == 'w' || input == 'a' || input == 's' || input == 'd') {
            res->shared_data->game.grid.snake.direction = (Direction)input;  // Nastavenie smeru
        }

        pthread_mutex_unlock(&res->shared_data->mutex);

        sem_post(res->sem_client_ready);  // Signalizácia serveru
    }

    return NULL;
}

void *render_thread(void *arg) {
    Resources *res = (Resources *)arg;

    while (1) {
        sem_wait(res->sem_server_ready);  // Čaká na signál od servera

        pthread_mutex_lock(&res->shared_data->mutex);
        GameStatus status = res->shared_data->game.status;
        int elapsed_time = res->shared_data->game.elapsed_time;
        int score = res->shared_data->game.score;
        
        printf("\033[H\033[J");
        for (int y = 0; y < res->shared_data->game.grid.height; y++) {
            for (int x = 0; x < res->shared_data->game.grid.width; x++) {
                printf("%c ", res->shared_data->game.grid.grid[y][x]);
            }
            printf("\n");
        }
        pthread_mutex_unlock(&res->shared_data->mutex);

        printf("Čas: %d sekúnd | Skóre: %d\n", elapsed_time, score);

        if (status == GAME_OVER) {
            printf("Klient: Hra skončila.\n");
            break;
        }
    }

    return NULL;
}

int main() {
    Resources res;
    pthread_t input_tid, render_tid;

    // Pripojenie k zdieľanej pamäti
    if (connect_to_shm(&res) != 0) {
        return 1;
    }

    // Pripojenie k semaforom
    if (connect_to_sem(&res) != 0) {
        cleanup(&res);
        return 1;
    }

    printf("Klient: Pripojenie k zdieľaným zdrojom úspešné.\n");

    // Nastavenie neblokujúceho režimu na stdin
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);

    // Vytvorenie vlákna na spracovanie vstupu
    if (pthread_create(&input_tid, NULL, input_thread, &res) != 0) {
        perror("Chyba pri vytváraní vlákna na spracovanie vstupu");
        cleanup(&res);
        return 1;
    }

    // Vytvorenie vlákna na vykresľovanie
    if (pthread_create(&render_tid, NULL, render_thread, &res) != 0) {
        perror("Chyba pri vytváraní vlákna na vykresľovanie");
        cleanup(&res);
        return 1;
    }

    // Počkaj na ukončenie oboch vlákien
    pthread_join(input_tid, NULL);
    pthread_join(render_tid, NULL);

    // Korektné ukončenie klienta
    cleanup(&res);
    return 0;
}
