#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <pthread.h>
#include <semaphore.h>
#include "shared.h"  // Zahrnúť váš header file s definíciami

int main() {
    int shm_id;
    SharedData *shared_data;

    // Vytvorenie zdieľanej pamäte
    shm_id = shmget(SHM_KEY, sizeof(SharedData), IPC_CREAT | 0666);
    if (shm_id < 0) {
        perror("shmget");
        exit(1);
    }

    // Pripojenie zdieľanej pamäte
    shared_data = (SharedData *)shmat(shm_id, NULL, 0);
    if (shared_data == (SharedData *)-1) {
        perror("shmat");
        exit(1);
    }

    // Inicializácia mutexu a semaforov
    pthread_mutex_init(&shared_data->mutex, NULL);
    sem_init(&shared_data->sem_server_ready, 1, 0);
    sem_init(&shared_data->sem_client_ready, 1, 0);

    printf("Server: Zdieľaná pamäť a semafory inicializované.\n");

    // Simulácia čakania na klienta
    printf("Server: Čakám na klienta...\n");
    sem_wait(&shared_data->sem_client_ready);  // Čaká na signál od klienta
    printf("Server: Prijal som signál od klienta.\n");

    // Uvoľnenie zdrojov
    pthread_mutex_destroy(&shared_data->mutex);
    sem_destroy(&shared_data->sem_server_ready);
    sem_destroy(&shared_data->sem_client_ready);
    shmdt(shared_data);
    shmctl(shm_id, IPC_RMID, NULL);

    printf("Server: Ukončenie programu.\n");
    return 0;
}
