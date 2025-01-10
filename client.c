#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>

#include "shared.h"



void cleanup(Resources *res) {
    if (res->shared_data != (void *)-1) {
        shmdt(res->shared_data);
    }
    sem_close(res->sem_server_ready);
    sem_close(res->sem_client_ready);

    printf("Klient: Korektne ukončený.\n");
}

int main() {
    Resources res;

    // Skús pripojiť sa k existujúcej zdieľanej pamäti
    res.shmid = shmget(SHM_KEY, sizeof(SharedData), 0666);
    if (res.shmid == -1) {
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
        res.shmid = shmget(SHM_KEY, sizeof(SharedData), 0666);
        if (res.shmid == -1) {
            perror("Chyba pri pripojení k zdieľanej pamäti");
            return 1;
        }
    }

    // Pripojenie k zdieľanej pamäti
    res.shared_data = (SharedData *)shmat(res.shmid, NULL, 0);
    if (res.shared_data == (void *)-1) {
        perror("Chyba pri pripájaní k zdieľanej pamäti");
        return 1;
    }

    // Pripojenie k existujúcim semaforom
    res.sem_server_ready = sem_open(SEM_SERVER_READY, 0);
    res.sem_client_ready = sem_open(SEM_CLIENT_READY, 0);
    if (res.sem_server_ready == SEM_FAILED || res.sem_client_ready == SEM_FAILED) {
        perror("Chyba pri pripájaní k semaforom");
        return 1;
    }

    printf("Klient: Pripojenie k zdieľaným zdrojom úspešné.\n");

    // Simulácia čítania stavu hry od servera
    sem_wait(res.sem_server_ready);  // Čaká na signalizáciu od servera
    pthread_mutex_lock(&res.shared_data->mutex);
    printf("Klient: Čítanie stavu hry, dummy_data = %d.\n", res.shared_data->dummy_data);
    pthread_mutex_unlock(&res.shared_data->mutex);

    // Odoslanie príkazu na zmenu stavu
    pthread_mutex_lock(&res.shared_data->mutex);
    printf("Klient: Odosielam príkaz na zmenu dummy_data na 100.\n");
    res.shared_data->dummy_data = 100;
    pthread_mutex_unlock(&res.shared_data->mutex);
    sem_post(res.sem_client_ready);  // Signalizácia serveru, že príkaz je pripravený

    // Korektné ukončenie klienta
    cleanup(&res);
    return 0;
}
