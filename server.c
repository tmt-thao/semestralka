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

int main() {
    Resources res;

    // Vytvorenie zdieľanej pamäte
    res.shmid = shmget(SHM_KEY, sizeof(SharedData), IPC_CREAT | 0666);
    if (res.shmid == -1) {
        perror("Chyba pri vytváraní zdieľanej pamäte");
        return 1;
    }

    // Pripojenie k zdieľanej pamäti
    res.shared_data = (SharedData *)shmat(res.shmid, NULL, 0);
    if (res.shared_data == (void *)-1) {
        perror("Chyba pri pripájaní k zdieľanej pamäti");
        return 1;
    }

    // Inicializácia mutexu
    if (pthread_mutex_init(&res.shared_data->mutex, NULL) != 0) {
        perror("Chyba pri inicializácii mutexu");
        return 1;
    }

    // Vytvorenie semaforov
    res.sem_server_ready = sem_open(SEM_SERVER_READY, O_CREAT, 0666, 0);
    res.sem_client_ready = sem_open(SEM_CLIENT_READY, O_CREAT, 0666, 0);
    if (res.sem_server_ready == SEM_FAILED || res.sem_client_ready == SEM_FAILED) {
        perror("Chyba pri vytváraní semaforov");
        return 1;
    }

    printf("Server: Inicializácia úspešná.\n");

    // Simulácia odoslania stavu hry klientovi
    pthread_mutex_lock(&res.shared_data->mutex);
    res.shared_data->dummy_data = 42;  // Nastav testovaciu hodnotu pre stav hry
    pthread_mutex_unlock(&res.shared_data->mutex);
    sem_post(res.sem_server_ready);  // Signalizácia klientovi, že stav hry je pripravený


	// Čakanie na príkaz od klienta
	sem_wait(res.sem_client_ready);  // Čaká na signalizáciu od klienta
	pthread_mutex_lock(&res.shared_data->mutex);
	printf("Server: Prijatý príkaz, nový dummy_data = %d.\n", res.shared_data->dummy_data);
	pthread_mutex_unlock(&res.shared_data->mutex);

    cleanup(&res);
    return 0;
}