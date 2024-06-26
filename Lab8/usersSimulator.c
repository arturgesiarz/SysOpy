// Wykorzystano POSIX
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/wait.h>
#include <signal.h>
#include <stdbool.h>

#include "sharedMemorySpces.h"

volatile bool should_close = false;

void SIGNAL_handler(int signum) {
    should_close = true;
}

// Funkcja generujacy losowy napis
void generate_random_string(char* buffer, int length) {
    for(int i = 0; i < length; i++) {
        buffer[i] = 'a' + rand() % 26;
    }
    buffer[length] = '\0';
}

int main(int argc, char** argv) {
    if(argc < 2) {
        return -1;
    }

    long number_of_users = strtol(argv[1], NULL, 10);

    // Proba utworzenia i otwarcia deskryptora pliku dla pamieci wspoldzielonej
    int memory_fd = shm_open(SHARED_MEMORY_DESCRIPTOR_NAME, O_RDWR, S_IRUSR | S_IWUSR);
    if(memory_fd < 0)
        perror("shm_open");

    // Próba zmapowania regionu pamięci współdzielonej do przestrzeni adresowej programu
    memoryMapT * memory_map = mmap(NULL, sizeof(memoryMapT), PROT_READ | PROT_WRITE, MAP_SHARED, memory_fd, 0);
    if (memory_map == MAP_FAILED)
        perror("mmap");

    char user_buffer[MAX_PRINTER_BUFFER_SIZE] = {0};

    // Rejestracie sygnalu
    for (int sig = 1; sig < SIGRTMAX; sig++) {
        signal(sig, SIGNAL_handler);
    }

    // Generowanie uzytkonikow
    for (int i = 0; i < number_of_users; i++){
        pid_t user_pid = fork();

        if (user_pid < 0) {
            perror("fork");
            return -1;
        }

        else if(user_pid == 0) {

            // Próba wysłania danych do drukarek w losowy sposób
            while(!should_close) {

                // Generowanie losowej wiadomosci
                generate_random_string(user_buffer, 10);

                // Proba znalezienia drukarki, która nie jest w tej chwili zajęta
                int printer_index = -1;
                for (int j = 0; j < memory_map->numberOfPrinters; j++) {
                    int val;

                    // Pobieranie biezacej wartosci semafora
                    sem_getvalue(&memory_map->printers[j].printerSemaphore, &val);

                    // Znaleziono wolna drukarke
                    if(val > 0) {
                        printer_index = j;
                        break;
                    }
                }

                // Jeśli wszystkie drukarki są zajęte
                // wysłamy prace do losowej drukarki w celu równomiernego rozłożenia pracy
                if(printer_index == -1)
                    printer_index = rand() % memory_map->numberOfPrinters;

                // Próbuje zmniejszyć semafor,
                // blokuje program, jeśli drukarka jest aktualnie zajęta
                if(sem_wait(&memory_map->printers[printer_index].printerSemaphore) < 0)
                    perror("sem_wait");

                // Kopiowanie danych do bufora drukarki
                memcpy(memory_map->printers[printer_index].printerBuffer, user_buffer, MAX_PRINTER_BUFFER_SIZE);
                memory_map->printers[printer_index].printerBufferSize = strlen(user_buffer);

                // Ustawienie stanu drukarki na drukowanie
                memory_map->printers[printer_index].printerState = PRINTING;

                printf("User %d is printing on printer %d\n", i, printer_index);
                fflush(stdout);

                // Uśpienie na losową ilość czasu przed próbą wysłania nowej pracy
                sleep(rand() % 3 + 1);
            }
            exit(0);
        }
    }

    // Czekamy aż wszystkie dzieci skończą
    while(wait(NULL) > 0) {};

    // Odmapowanie mapy pamięci
    munmap(memory_map, sizeof(memoryMapT));
}