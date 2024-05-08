#include <sys/types.h>
#include <sys/msg.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include "common.h"

// Funkcja ta służy do wysłania wiadomości inicjalizacyjnej do serwera.
// Przyjmuje klucz kolejki komunikatów klienta oraz identyfikator kolejki komunikatów serwera.
// Wysyła komunikat INIT do serwera, aby nawiązać połączenie, a następnie oczekuje na odpowiedź od serwera,
// która zawiera nadany identyfikator klienta.
int sendInitMessage( key_t clientMsgQueueKey, int msgQueueServerID ) {
    Message messageSend = {};

    messageSend.callType = INIT;
    messageSend.clientMsgQueueKey = clientMsgQueueKey;
    messageSend.messageType = 1;

    int msgQueueClientID = msgget (clientMsgQueueKey, 0666);
    if ( msgQueueClientID < 0 ) { perror ("Client - open queue error"); return 1; }

    // Wysyłanie komunikatów do kolejki komunikatów
    if ( msgsnd (msgQueueServerID, &messageSend, sizeof(messageSend) - sizeof(long), 0) < 0 ) {
        perror ("Client - send message error");
        return 1;
    }

    Message messageReceived = {};

    // Oczekiwanie na komunikat o typie 1
    if ( msgrcv (msgQueueClientID, &messageReceived, sizeof(messageReceived) - sizeof(long), 1, 0) < 0 ) {
        perror ("Client - receive message error");
        return 1;
    }

    return messageReceived.clientID;
}

int main () {
    srand ( time(NULL) );

    // Losowanie kodow z zakresu 100 - 999
    key_t clientMsgQueueKey = rand() % 900 + 100;

    // Pobranie istniejącej kolejki komunikatów lub jej stworzenie.
    int msgQueueClientID = msgget (clientMsgQueueKey, 0666 | IPC_CREAT);
    int msgQueueServerID = msgget ( (key_t) SERVER_ID, 0666 );

    if (msgQueueClientID < 0) { perror("Client - open client queue error"); return 1; }
    if (msgQueueServerID < 0) { perror("Client - open server queue error"); return 1; }

    int clientID = sendInitMessage( clientMsgQueueKey, msgQueueServerID );

    pid_t pid = fork ();
    if ( pid < 0 ) { printf ("Error - fork()"); return 1; }

    // Proces macierzysty odpowiedzalny ze wysylanie wiadomosci do serwera
    if ( pid != 0 ) {

        while (1) {
            printf ("What message are you going to send to other clients?\n");

            // Opróżnienie bufora wyjściowego
            fflush (stdout);

            char message[ MAX_TEXT ];

            // Odczytwanie z bufora
            fgets (message, MAX_TEXT, stdin);

            Message messageSend = {};
            messageSend.callType = OTHER;
            messageSend.messageType = 1;
            messageSend.clientID = clientID;
            messageSend.clientMsgQueueKey = clientMsgQueueKey;
            strcpy ( messageSend.message, message );


            if ( msgsnd (msgQueueServerID, &messageSend, sizeof(messageSend) - sizeof(long), 0) < 0 ) {
                perror ("Client - message send error");
                return 2;
            }
        }
    }
    // Proces potomny
    else {
        int msgQueueClientID = msgget (clientMsgQueueKey, 0666);
        Message messageReceived = {};

        while (1) {
            // Oczekuje na wiadomosci
            if ( msgrcv (msgQueueClientID, &messageReceived, sizeof(messageReceived) - sizeof(long), 1, 0) < 0 ) {
                perror ("Client - message receive error in parent process");
                return 3;
            }
            // Wyswietlam wiadomosc
            puts ( messageReceived.message );
        }
    }

    // Usunięcie kolejki komunikatów
    msgctl (msgQueueClientID, IPC_RMID, NULL);

    return 0;
}