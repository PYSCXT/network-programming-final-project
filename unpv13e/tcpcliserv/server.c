#define _CRT_SECURE_NO_WARNINGS
#define BUFFER_SIZE 1024
#define MAX_USERS 100
#define PORT 26699

#include "unp.h"
#include <stdbool.h>

/* Structure to store user details */
typedef struct {
    bool is_online;
    char password[50];
    char username[50];
    struct sockaddr_in address;
} User;

typedef struct {
    int dice_num;
    int life_num;
    int listenfd;
    int player_num;
    int playerfd[MAX_USERS];
} GameArgs;

int player_count = 0;
int playerfd[MAX_USERS];
int user_count = 0;
pthread_mutex_t player_count_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t user_mutex = PTHREAD_MUTEX_INITIALIZER;
User users[MAX_USERS]; /* Array to store users */

/* Function to save a new user to a file */
bool save_user(const char *file_name, const char *password, const char *username) {
    pthread_mutex_lock(&user_mutex);
    for (int i = 0; i < user_count; i++) {
        if (strcmp(users[i].username, username) == 0) {
            pthread_mutex_unlock(&user_mutex);
            return false; /* User already exists */
        }
    }

    strcpy(users[user_count].username, username);
    strcpy(users[user_count].password, password);
    users[user_count].is_online = false;
    user_count++;
    pthread_mutex_unlock(&user_mutex);

    FILE *file = Fopen(file_name, "a");
    if (!file) {
        perror("[Error] Failed to open user file.\n");
        return false;
    }

    fprintf(file, "%s %s\n", username, password);
    Fclose(file);
    return true;
}

/* Check function */
void Check(int now_player, int dice_num, int dice_array[][dice_num], int playerfd[]) {
    char sendline[MAXLINE];
    memset(sendline, 0, sizeof(sendline));
    for (int j = 0; j < dice_num; j++) {
        if (j == dice_num - 1) {
            sprintf(sendline + strlen(sendline), "%d%s", dice_array[now_player][j], "");
        } else if (j != dice_num - 1) {
            sprintf(sendline + strlen(sendline), "%d%s", dice_array[now_player][j], " ");
        }
    }
    Writen(playerfd[now_player], sendline, strlen(sendline));
}

int game(int dice_num, int life_num, int listenfd, int player_num, int playerfd[]) {
    srand(time(NULL));
    char recvline[MAXLINE], sendline[MAXLINE];
    int dice_array[player_num][dice_num], now_player = 0, player_counter = player_num, playerlife[player_num];
    memset(dice_array, 0, sizeof(dice_array));
    for (int i = 0; i < player_num; i++) {
        playerlife[i] = life_num;
    }
    memset(sendline, 0, sizeof(sendline));

    /* Every big round (until the player has 1 left) */
    while (1) {
        if (player_counter == 1) {
            memset(sendline, 0, sizeof(sendline));
            int winner = 0;
            for (int i = 0; i < player_num; i++) {
                if (playerlife[i] > 0) {
                    winner = i;
                }
            }
            snprintf(sendline, sizeof(sendline), "#%d player wins.\n", winner + 1);
            for (int i = 0; i < player_num; i++) {
                if (i != winner) {
                    Writen(playerfd[i], sendline, strlen(sendline));
                }
                memset(sendline, 0, sizeof(sendline));
                strcpy(sendline, "You are the winner, congrats.\n");
                Writen(playerfd[winner], sendline, strlen(sendline));
                return 1;
            }
        }

        /* Roll dice for surviving players */
        for (int i = 0; i < player_num; i++) {
            if (playerlife[i] > 0) {
                /* dice */
                for (int j = 0; j < dice_num; j++) {
                    dice_array[i][j] = rand() % 6 + 1;
                }
            }
        }

        /* start action */
        fd_set rset;
        int bid_dice_amount = 0, bid_dice_point = 0, prebid_dice_amont = 0, prebid_dice_point = 0, preplayer = 0, turn = 0;
        struct timeval now, start, timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        /* Each small round until the end of Call or Spot */
        while (1) {
            memset(sendline, 0, sizeof(sendline));
            strcpy(sendline, "Now it is your turn, you can type Bid, Call, Spot or Check.\n");
            Writen(playerfd[now_player], sendline, strlen(sendline));
            memset(sendline, 0, sizeof(sendline));
            snprintf(sendline, sizeof(sendline), "Now it is #%d player's turn.\n", now_player + 1);
            for (int i = 0; i < player_num; i++) {
                if (i != now_player) {
                    Writen(playerfd[i], sendline, strlen(sendline));
                }
            }
            Gettimeofday(&start, NULL);
            char message[MAXLINE];
            int ctl = 1, playerACT = 0; /* playerACT: 0 = TIMEOUT, 1 = BID, 2 = CALL, 3 = SPOT */
            memset(message, 0, sizeof(message));

            /* Set select and timeout, wait for now player response, and accept Check command during this period */
            while (ctl) {
                FD_ZERO(&rset);
                FD_SET(listenfd, &rset);
                int maxfdp1 = listenfd;
                for (int i = 0; i < player_num; i++) {
                    FD_SET(playerfd[i], &rset);
                    if (playerfd[i] > maxfdp1) {
                        maxfdp1 = playerfd[i];
                    }
                }
                maxfdp1++;
                Select(maxfdp1, &rset, NULL, NULL, &timeout);
                Gettimeofday(&now, NULL);
                double elapsed = (now.tv_usec - start.tv_usec) / 1000000 + now.tv_sec - start.tv_sec;
                if (elapsed >= 30) {
                    ctl = 0;
                    playerACT = 0;
                    memset(sendline, 0, sizeof(sendline));
                    strcpy(sendline, "You acted too slow, automatically acting...\n");
                    Writen(playerfd[now_player], sendline, strlen(sendline));
                }
                if (FD_ISSET(listenfd, &rset)) {
                    socklen_t clilen;
                    struct sockaddr_in cliaddr;
                    int tmp = Accept(listenfd, (SA *) &cliaddr, &clilen);
                    memset(sendline, 0, sizeof(sendline));
                    strcpy(sendline, "The game has started, please try to connect again after the game is over.\n");
                    Writen(tmp, sendline, strlen(sendline));
                    Close(tmp);
                }
                if (FD_ISSET(playerfd[now_player], &rset)) {
                    int n;
                    memset(recvline, 0, sizeof(recvline));
                    if ((n = Read(playerfd[now_player], recvline, sizeof(recvline) - 1)) <= 0) {
                        memset(sendline, 0, sizeof(sendline));
                        snprintf(sendline, sizeof(sendline), "#%d player disconnected, game terminated.\n", now_player + 1);
                        for (int i = 0; i < player_num; i++) {
                            if (i != now_player) {
                                Writen(playerfd[i], sendline, strlen(sendline));
                            }
                        }
                        Close(playerfd[now_player]);
                        return -1;
                    }
                    recvline[n] = '\0';
                    if (strcmp(recvline, "Check") == 0) {
                        /* Check function */
                        Check(now_player, dice_num, dice_array, playerfd);
                    } else if ((prebid_dice_amont != dice_num * player_counter || prebid_dice_point != 6) && strcmp(recvline, "Bid") == 0) {
                        playerACT = 1;
                        Gettimeofday(&start, NULL);
                        memset(sendline, 0, sizeof(sendline));
                        strcpy(sendline, "Please type in the amount and the point of dices you want to bid, e.g., 4 5\n");
                        Writen(playerfd[now_player], sendline, strlen(sendline));
                    } else if (playerACT == 1) {
                        int ch = sscanf(recvline, "%d %d", &bid_dice_amount, &bid_dice_point);

                        /* Format check */
                        if (ch != 2) {
                            memset(sendline, 0, sizeof(sendline));
                            strcpy(sendline, "Unallowed form of message, please type another one.\n");
                            Writen(playerfd[now_player], sendline, strlen(sendline));
                        }

                        /* digital check */
                        else if ((bid_dice_amount == prebid_dice_amont && bid_dice_point <= prebid_dice_point) || bid_dice_amount < prebid_dice_amont || bid_dice_amount <= 0 || bid_dice_amount > dice_num * player_counter || bid_dice_point <= 0 || bid_dice_point > 6) {
                            memset(sendline, 0, sizeof(sendline));
                            strcpy(sendline, "Unallowed number, please type another bid.\n");
                            Writen(playerfd[now_player], sendline, strlen(sendline));
                        }

                        else {
                            ctl = 0;
                        }
                    } else if (strcmp(recvline, "Call") == 0 && turn > 0) {
                        ctl = 0;
                        playerACT = 2;
                    } else if (strcmp(recvline, "Spot") == 0 && turn > 0) {
                        ctl = 0;
                        playerACT = 3;
                    } else {
                        memset(sendline, 0, sizeof(sendline));
                        strcpy(sendline, "Unknown or unallowed command, please retry.\n");
                        Writen(playerfd[now_player], sendline, strlen(sendline));
                    }
                }
                for (int i = 0; i < player_num; i++) {
                    if (FD_ISSET(playerfd[i], &rset) && i != now_player) {
                        int n;
                        memset(recvline, 0, sizeof(recvline));
                        if ((n = Read(playerfd[i], recvline, sizeof(recvline) - 1)) <= 0) {
                            memset(sendline, 0, sizeof(sendline));
                            snprintf(sendline, sizeof(sendline), "#%d player disconnected, game terminated.\n", i + 1);
                            for (int j = 0; j < player_num; j++) {
                                if (j != i) {
                                    Writen(playerfd[j], sendline, strlen(sendline));
                                }
                            }
                            Close(playerfd[i]);
                            return -1;
                        }
                        recvline[n] = '\0';
                        if (playerlife[i] > 0 && strcmp(recvline, "Check") == 0) {
                            /* Check function */
                            Check(now_player, dice_num, dice_array, playerfd);
                        } else {
                            memset(sendline, 0, sizeof(sendline));
                            strcpy(sendline, "unknown or unallowed command, please retry.\n");
                            Writen(playerfd[i], sendline, strlen(sendline));
                        }
                    }
                }
            }

            /* Take action according to playerACT */
            /* TIMEOUT */
            if (playerACT == 0) {
                if (turn == 0) {
                    bid_dice_amount = 1;
                    bid_dice_point = 1;
                    playerACT = 1;
                } else if (prebid_dice_amont == dice_num * player_counter) {
                    playerACT = 2;
                } else {
                    bid_dice_amount = prebid_dice_amont + 1;
                    bid_dice_point = prebid_dice_point;
                    playerACT = 1;
                }
            }

            /* Bid */
            if (playerACT == 1) {
                memset(sendline, 0, sizeof(sendline));
                snprintf(sendline, sizeof(sendline), "#%d player bid %d dice(s) of point %d\n", now_player + 1, bid_dice_amount, bid_dice_point);
                for (int i = 0; i < player_num; i++) {
                    if (i != now_player) {
                        Writen(playerfd[i], sendline, strlen(sendline));
                    }
                }
                memset(sendline, 0, sizeof(sendline));
                snprintf(sendline, sizeof(sendline), "You bid %d dice(s) of point %d\n", bid_dice_amount, bid_dice_point);
                Writen(playerfd[now_player], sendline, strlen(sendline));

                /* Record the bid result and the previous player */
                prebid_dice_amont = bid_dice_amount;
                prebid_dice_point = bid_dice_point;
                preplayer = now_player;

                /* Find the next now player */
                do {
                    now_player = (now_player + 1) % player_num;
                    if (playerlife[now_player] > 0) {
                        break;
                    }
                } while (true);
            }

            /* Call */
            if (playerACT == 2) {
                memset(sendline, 0, sizeof(sendline));
                strcpy(sendline, "You Called Liar.\n");
                Writen(playerfd[now_player], sendline, strlen(sendline));
                memset(sendline, 0, sizeof(sendline));
                snprintf(sendline, sizeof(sendline), "#%d player Called Liar.\n", now_player + 1);
                for (int i = 0; i < player_num; i++) {
                    if (i != now_player) {
                        Writen(playerfd[i], sendline, strlen(sendline));
                    }
                }

                int counter = 0;
                for (int i = 0; i < player_num; i++) {
                    for (int j = 0; j < dice_num; j++) {
                        if(dice_array[i][j] == bid_dice_point) {
                            counter++;
                        }
                    }
                }

                /* fail */
                if (counter > bid_dice_amount) {
                    playerlife[now_player]--;
                    memset(sendline, 0, sizeof(sendline));
                    snprintf(sendline, sizeof(sendline), "Your Call failed.\nYour life decreased by 1.\nYour life(s) now is %d.\n", playerlife[now_player]);
                    Writen(playerfd[now_player], sendline, strlen(sendline));
                    memset(sendline, 0, sizeof(sendline));
                    snprintf(sendline, sizeof(sendline), "#%d player's call failed.\n", now_player + 1);
                    for (int i = 0; i < player_num; i++) {
                        if (i != now_player) {
                            Writen(playerfd[i], sendline, strlen(sendline));
                        }
                    }
                    if (playerlife[now_player] == 0) {
                        player_counter--;
                        memset(sendline, 0, sizeof(sendline));
                        strcpy(sendline, "You are out of life and got elimated, good luck next time.\n");
                        Writen(playerfd[now_player], sendline, strlen(sendline));
                        memset(sendline, 0, sizeof(sendline));
                        snprintf(sendline, sizeof(sendline), "#%d player is elimated.\n", now_player + 1);
                    }
                }

                /* success */
                else {
                    playerlife[preplayer]--;
                    memset(sendline, 0, sizeof(sendline));
                    strcpy(sendline, "Your Call succeed.\n");
                    Writen(playerfd[now_player], sendline, strlen(sendline));
                    memset(sendline, 0, sizeof(sendline));
                    snprintf(sendline, sizeof(sendline), "You got caught.\nYour life decreased by 1.\nYour life(s) now is %d.\n", playerlife[preplayer]);
                    Writen(playerfd[preplayer], sendline, strlen(sendline));
                    memset(sendline, 0, sizeof(sendline));
                    snprintf(sendline, sizeof(sendline), "#%d player's call succeed.\n", now_player + 1);
                    for (int i = 0; i < player_num; i++) {
                        if (i != now_player) {
                            Writen(playerfd[i], sendline, strlen(sendline));
                        }
                    }
                    if (playerlife[preplayer] == 0) {
                        player_counter--;
                        memset(sendline, 0, sizeof(sendline));
                        strcpy(sendline, "You are out of life and got elimated, good luck next time.\n");
                        Writen(playerfd[preplayer], sendline, strlen(sendline));
                        memset(sendline, 0, sizeof(sendline));
                        snprintf(sendline, sizeof(sendline), "#%d player is elimated.\n", preplayer + 1);
                        for (int i = 0; i < player_num; i++) {
                            if (i != preplayer) {
                                Writen(playerfd[i], sendline, strlen(sendline));
                            }
                        }
                    }
                }

                /* Find the next now player */
                do {
                    now_player = (now_player + 1) % player_num;
                    if (playerlife[now_player] > 0) {
                        break;
                    }
                } while (true);

                /* The ferry ends and the record is cleared */
                prebid_dice_amont = 0;
                prebid_dice_point = 0;
                preplayer = 0;
                break;
            }

            /* Spot */
            if (playerACT == 3) {
                memset(sendline, 0, sizeof(sendline));
                strcpy(sendline, "You chose to guess SPOT ON.\n");
                Writen(playerfd[now_player], sendline, strlen(sendline));
                memset(sendline, 0, sizeof(sendline));
                snprintf(sendline, sizeof(sendline), "#%d player chose to guess SPOT ON.\n", now_player + 1);
                for (int i = 0; i < player_num; i++) {
                    if (i != now_player) {
                        Writen(playerfd[i], sendline, strlen(sendline));
                    }
                }

                int counter = 0;

                for (int i = 0; i < player_num; i++) {
                    for (int j = 0; j < dice_num; j++) {
                        if(dice_array[i][j] == bid_dice_point) {
                            counter++;
                        }
                    }
                }

                /* fail */
                if (counter > bid_dice_amount) {
                    playerlife[now_player]--;
                    memset(sendline, 0, sizeof(sendline));
                    snprintf(sendline, sizeof(sendline), "Your guess is wrong.\nYour life decreased by 1.\nYour life(s) now is %d.\n", playerlife[now_player]);
                    Writen(playerfd[now_player], sendline, strlen(sendline));
                    memset(sendline, 0, sizeof(sendline));
                    snprintf(sendline, sizeof(sendline), "#%d player's guess is wrong.\n", now_player + 1);
                    for (int i = 0; i < player_num; i++) {
                        if (i != now_player) {
                            Writen(playerfd[i], sendline, strlen(sendline));
                        }
                    }
                    if (playerlife[now_player] == 0) {
                        player_counter--;
                        memset(sendline, 0, sizeof(sendline));
                        strcpy(sendline, "You are out of life and got elimated, good luck next time.\n");
                        Writen(playerfd[now_player], sendline, strlen(sendline));
                        memset(sendline, 0, sizeof(sendline));
                        snprintf(sendline, sizeof(sendline), "#%d player is elimated.\n", now_player + 1);
                    }
                }

                /* success */
                else {
                    memset(sendline, 0, sizeof(sendline));
                    strcpy(sendline, "Your guess is right.\nEverybody else's life decreased by 1.\n");
                    Writen(playerfd[now_player], sendline, strlen(sendline));
                    memset(sendline, 0, sizeof(sendline));
                    snprintf(sendline, sizeof(sendline), "#%d player's guess is right.\nYour life decreased by 1.\n", now_player + 1);
                    for (int i = 0; i < player_num; i++) {
                        if (i != now_player) {
                            Writen(playerfd[i], sendline, strlen(sendline));
                        }
                    }
                    for (int i = 0; i < player_num; i++) {
                        if (i != now_player && playerlife[i] > 0) {
                            playerlife[i]--;
                            if (playerlife[i] == 0) {
                                player_counter--;
                                memset(sendline, 0, sizeof(sendline));
                                strcpy(sendline, "You are out of life and got elimated, good luck next time.\n");
                                Writen(playerfd[preplayer], sendline, strlen(sendline));
                                memset(sendline, 0, sizeof(sendline));
                                snprintf(sendline, sizeof(sendline), "#%d player is elimated.\n", preplayer + 1);
                                for (int j = 0; i < player_num; j++) {
                                    if (j != i) {
                                        Writen(playerfd[i], sendline, strlen(sendline));
                                    }
                                }
                            }
                        }
                    }
                }

                /* Find the next now player */
                do {
                    now_player = (now_player + 1) % player_num;
                    if (playerlife[now_player] > 0) {
                        break;
                    }
                } while (true);

                /* The ferry ends and the record is cleared */
                prebid_dice_amont = 0;
                prebid_dice_point = 0;
                preplayer = 0;
                break;
            }
            turn++;
        }
    }
}

/* Function to load users from a file */
void load_users(const char *file_name) {
    FILE *file = fopen(file_name, "r");
    if (!file) {
        printf("[Info] %s not found.\n[Info] Starting with no users.\n", file_name);
        return;
    }

    char line[100];
    while (fgets(line, sizeof(line), file)) {
        char password[50], username[50];
        if (sscanf(line, "%49s %49s", username, password) == 2) {
            pthread_mutex_lock(&user_mutex);
            strcpy(users[user_count].username, username);
            strcpy(users[user_count].password, password);
            users[user_count].is_online = false;
            user_count++;
            pthread_mutex_unlock(&user_mutex);
        }
    }
    fclose(file);
}

void *game_thread_func(void *arg) {
    GameArgs *gameArgs = (GameArgs *)arg;
    game(gameArgs->dice_num, gameArgs->life_num, gameArgs->listenfd, gameArgs->player_num, gameArgs->playerfd);
    free(gameArgs);
    return NULL;
}

/* Function to handle communication with a client */
void *handle_client(void *arg) {
    int client_socket = *(int *)arg;
    free(arg);

    char buffer[BUFFER_SIZE], current_user[50] = {0};

    while (true) {
        int bytes_received = Recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
        if (bytes_received <= 0) {
            printf("[Info] Client disconnected.\n");
            Close(client_socket);
            pthread_exit(NULL);
        }

        buffer[bytes_received] = '\0';

        if (strcmp(buffer, "register") == 0) {
            Send(client_socket, "Username: ", 11, 0);
            bytes_received = Recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
            buffer[bytes_received] = '\0';
            char username[50];
            strcpy(username, buffer);

            Send(client_socket, "Password: ", 11, 0);
            bytes_received = Recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
            buffer[bytes_received] = '\0';
            char password[50];
            strcpy(password, buffer);

            if (save_user("users.csv", username, password)) {
                Send(client_socket, "Registered.\n", 13, 0);
                printf("[Success] User '%s' registered.\n", username);
            } else {
                Send(client_socket, "Exists.\n", 9, 0);
            }
        } else if (strcmp(buffer, "login") == 0) {
            Send(client_socket, "Username: ", 11, 0);
            bytes_received = Recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
            buffer[bytes_received] = '\0';
            char username[50];
            strcpy(username, buffer);

            Send(client_socket, "Password: ", 11, 0);
            bytes_received = Recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
            buffer[bytes_received] = '\0';
            char password[50];
            strcpy(password, buffer);

            bool valid_user = false;
            pthread_mutex_lock(&user_mutex);
            for (int i = 0; i < user_count; i++) {
                if (strcmp(users[i].username, username) == 0 && strcmp(users[i].password, password) == 0) {
                    valid_user = true;
                    users[i].is_online = true;
                    strcpy(current_user, username);
                    break;
                }
            }
            pthread_mutex_unlock(&user_mutex);

            if (valid_user) {
                Send(client_socket, "Welcome!\n", 10, 0);
                printf("[Info] User '%s' logged in.\n", username);
            } else {
                Send(client_socket, "Invalid.\n", 10, 0);
            }
        } else if (strcmp(buffer, "exit") == 0) {
            printf("[Info] Client exiting.\n");
            break;
        } else if (strcmp(buffer, "start_game") == 0) {
            pthread_mutex_lock(&player_count_mutex);
            playerfd[player_count++] = client_socket;

            /* Example: Two players required */
            if (player_count == 2) {
                for (int i = 0; i < player_count; i++) {
                    Send(playerfd[i], "Game started.\n", 15, 0);
                }
                GameArgs *args = malloc(sizeof(GameArgs));
                args->dice_num = 5;
                args->life_num = 3;
                args->player_num = 2;
                memcpy(args->playerfd, playerfd, sizeof(playerfd));

                pthread_t game_thread;
                pthread_create(&game_thread, NULL, game_thread_func, args);
                pthread_detach(game_thread);

                player_count = 0; /* Reset for the next game */
            }
            pthread_mutex_unlock(&player_count_mutex);
        } else {
            Send(client_socket, "Unrecognized command.\n", 23, 0);
        }
    }

    Close(client_socket);
    pthread_exit(NULL);
}

/* Function to start the server */
void start_server() {
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("[Error] Socket creation failed.\n");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in server_address;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(PORT);

    if (bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address)) == -1) {
        perror("[Error] Binding failed.\n");
        Close(server_socket);
        exit(EXIT_FAILURE);
    }

    if (listen(server_socket, 10) == -1) {
        perror("[Error] Listening failed.\n");
        Close(server_socket);
        exit(EXIT_FAILURE);
    }

    printf("[Server started] Waiting for connections...\n");

    while (true) {
        struct sockaddr_in client_address;
        socklen_t client_address_len = sizeof(client_address);
        int *client_socket = malloc(sizeof(int));

        *client_socket = Accept(server_socket, (struct sockaddr *)&client_address, &client_address_len);
        if (*client_socket == -1) {
            perror("[Error] Accepting connection failed.\n");
            free(client_socket);
            continue;
        }

        printf("[Info] Client connected: %s:%d\n", inet_ntoa(client_address.sin_addr), ntohs(client_address.sin_port));

        pthread_t thread;
        pthread_create(&thread, NULL, handle_client, client_socket);
        pthread_detach(thread);
    }

    Close(server_socket);
}

int main() {
    load_users("users.csv");
    start_server();
    return 0;
}