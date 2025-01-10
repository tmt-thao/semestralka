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

void game_loop(Resources *res) {
    while (1) {
        // Čaká na signál od servera, že je pripravený nový stav hry
        sem_wait(res->sem_server_ready);

        // Zámok pre bezpečné čítanie zdieľanej pamäte
        pthread_mutex_lock(&res->shared_data->mutex);

        // Prečítanie a zobrazenie aktuálneho stavu hry
        printf("Klient: Hra prebieha... Skóre: %d, Uplynutý čas: %d sekúnd\n",
               res->shared_data->game.score,
               res->shared_data->game.elapsed_time);

        // Výpis stavu herného sveta (mriežky)
        for (int y = 0; y < res->shared_data->game.grid.height; y++) {
            for (int x = 0; x < res->shared_data->game.grid.width; x++) {
                printf("%c ", res->shared_data->game.grid.grid[y][x]);
            }
            printf("\n");
        }
        printf("\n");

        // Kontrola ukončenia hry
        if (res->shared_data->game.status == GAME_OVER) {
            pthread_mutex_unlock(&res->shared_data->mutex);
            break;
        }
        pthread_mutex_unlock(&res->shared_data->mutex);
    }

    printf("Klient: Hra skončila. Finálne skóre: %d\n", res->shared_data->game.score);
}


int main() {
    Resources res;

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

    // Spustenie hernej slučky
    game_loop(&res);

    // Korektné ukončenie klienta
    cleanup(&res);
    return 0;
}
