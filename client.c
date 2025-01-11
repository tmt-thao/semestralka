#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <semaphore.h>
#include <sys/wait.h>
#include "shared.h"  // Zahrnúť váš header file s definíciami

int main() {
    int shm_id;
    SharedData *shared_data;
    pid_t server_pid;

    // Skontroluje, či server už beží (pokúsi sa pripojiť k zdieľanej pamäti)
    shm_id = shmget(SHM_KEY, sizeof(SharedData), 0666);
    if (shm_id < 0) {
        // Ak zdieľaná pamäť neexistuje, spustí nový serverový proces
        printf("Client: Server nie je spustený. Spúšťam nový server...\n");

        server_pid = fork();
        if (server_pid < 0) {
            perror("fork");
            exit(1);
        }

        if (server_pid == 0) {
            // Dieťa (serverový proces) spustí server cez exec
            execl("./server", "server", NULL);
            perror("execl");
            exit(1);
        }

        // Rodič (klient) počká krátko na inicializáciu servera
        sleep(1);
    }

    // Pripojenie k existujúcej zdieľanej pamäti
    shm_id = shmget(SHM_KEY, sizeof(SharedData), 0666);
    if (shm_id < 0) {
        perror("shmget");
        exit(1);
    }

    shared_data = (SharedData *)shmat(shm_id, NULL, 0);
    if (shared_data == (SharedData *)-1) {
        perror("shmat");
        exit(1);
    }

    printf("Client: Pripojenie k zdieľanej pamäti úspešné.\n");

    // Signalizácia serveru
    sem_post(&shared_data->sem_client_ready);
    printf("Client: Odoslaný signál serveru.\n");

    // Uvoľnenie zdrojov
    shmdt(shared_data);

    printf("Client: Ukončenie programu.\n");
    return 0;
}
