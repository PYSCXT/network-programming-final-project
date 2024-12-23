#define _CRT_SECURE_NO_WARNINGS
#define BUFFER_SIZE 1024
#define PORT 26699

#include "unp.h"

int flag = 0;
pthread_cond_t game_started_cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t game_started_mutex = PTHREAD_MUTEX_INITIALIZER;
volatile int game_started = 0;

void print_dice_image(int number) {
    switch (number) {
        case 1:
            printf(" ----- \n");
            printf("|     |\n");
            printf("|  o  |\n");
            printf("|     |\n");
            printf(" ----- \n");
            break;
        case 2:
            printf(" ----- \n");
            printf("| o   |\n");
            printf("|     |\n");
            printf("|   o |\n");
            printf(" ----- \n");
            break;
        case 3:
            printf(" ----- \n");
            printf("| o   |\n");
            printf("|  o  |\n");
            printf("|   o |\n");
            printf(" ----- \n");
            break;
        case 4:
            printf(" ----- \n");
            printf("| o o |\n");
            printf("|     |\n");
            printf("| o o |\n");
            printf(" ----- \n");
            break;
        case 5:
            printf(" ----- \n");
            printf("| o o |\n");
            printf("|  o  |\n");
            printf("| o o |\n");
            printf(" ----- \n");
            break;
        case 6:
            printf(" ----- \n");
            printf("| o o |\n");
            printf("| o o |\n");
            printf("| o o |\n");
            printf(" ----- \n");
            break;
        default:
            printf("[Error] Invalid dice number: %d\n", number);
    }
}

void process_dice_numbers(const char *buffer) {
    char *token = strtok((char *)buffer, " ");
    printf("[Dice Roll Results]\n");
    while (token != NULL) {
        int dice_number = atoi(token);
        print_dice_image(dice_number);
        token = strtok(NULL, " ");
    }
}

/* Thread function to handle receiving messages from server */
void *receive_handler(void *arg) {
    int socket_fd = *(int *)arg;
    char buffer[BUFFER_SIZE];

    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_received = Recv(socket_fd, buffer, BUFFER_SIZE - 1, 0);
        if (bytes_received <= 0) {
            printf("[Info] Server disconnected.\n");
            pthread_exit(NULL);
        }

        buffer[bytes_received] = '\0';

        if (strcmp(buffer, "game started") == 0) {
            pthread_mutex_lock(&game_started_mutex);
            game_started = 1;
            pthread_cond_signal(&game_started_cond);
            pthread_mutex_unlock(&game_started_mutex);
        }

        if (strspn(buffer, " 1234567890") == strlen(buffer)) {
            /* If the message contains only numbers and spaces, treat it as dice numbers */
            process_dice_numbers(buffer);
        } else {
            printf("\n[Server] %s\n", buffer);
        }
    }
}

/* Function to handle user commands */
void handle_commands(int socket_fd) {
    char buffer[BUFFER_SIZE];
    while (1) {
        Fgets(buffer, BUFFER_SIZE, stdin);
        buffer[strcspn(buffer, "\n")] = '\0'; /* Remove trailing newline */

        if (send(socket_fd, buffer, strlen(buffer), 0) == -1) {
            perror("[Error] Sending command failed.\n");
            break;
        }

        if (strcmp(buffer, "exit") == 0) {
            printf("[Info] Exiting client.\n");
            break;
        }

        if (strcmp(buffer, "start_game") == 0) {
            printf("[Info] Waiting for the game to start...\n");

            pthread_mutex_lock(&game_started_mutex);
            while (!game_started) {
                pthread_cond_wait(&game_started_cond, &game_started_mutex);
            }
            pthread_mutex_unlock(&game_started_mutex);
        }

        memset(buffer, 0, BUFFER_SIZE);
    }
}

/* Main function */
int main(int argc, char **argv) {
    if (argc != 2) {
        err_quit("usage: tcpcli <IPaddress>\n");
    }

    int socket_fd;
    struct sockaddr_in server_addr;

    /* Create socket */
    if ((socket_fd = Socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("[Error] Socket creation failed.\n");
        exit(EXIT_FAILURE);
    }

    /* Configure server address */
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, argv[1], &server_addr.sin_addr) <= 0) {
        perror("[Error] Invalid address or address not supported.\n");
        Close(socket_fd);
        exit(EXIT_FAILURE);
    }

    /* Connect to server */
    if (connect(socket_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("[Error] Connection to server failed.\n");
        Close(socket_fd);
        exit(EXIT_FAILURE);
    }

    printf("[Info] Connected to the server.\n");

    /* Create a thread to handle server responses */
    pthread_t recv_thread;
    if (pthread_create(&recv_thread, NULL, receive_handler, (void *)&socket_fd) != 0) {
        perror("[Error] Could not create thread.\n");
        Close(socket_fd);
        exit(EXIT_FAILURE);
    }

    /* Handle user commands */
    printf("\nAvailable commands : register / login / start_game / exit : ");
    handle_commands(socket_fd);

    /* Clean up */
    pthread_cancel(recv_thread);
    pthread_join(recv_thread, NULL);
    Close(socket_fd);

    return 0;
}