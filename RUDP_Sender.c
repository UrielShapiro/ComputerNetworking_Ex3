#include "rudp.h"

#include <stdio.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#define FILE_SIZE 2097152

/*
 * @brief A random data generator function based on srand() and rand().
 * @param size The size of the data to generate (up to 2^32 bytes).
 * @return A pointer to the buffer.
 */
char *util_generate_random_data(unsigned int size)
{
    char *generated_file = NULL;
    // Argument check.
    if (size == 0)
        return NULL;
    generated_file = (char *)calloc(size, sizeof(char));
    // Error checking.
    if (generated_file == NULL)
        return NULL;
    // Randomize the seed of the random number generator.
    srand(time(NULL));
    for (unsigned int i = 0; i < size; i++)
        *(generated_file + i) = ((unsigned int)rand() % 256);
    return generated_file;
}

int main(int argc, char **argv)
{
    size_t i = 0;
    char *ip = NULL;
    unsigned short port = 0;
    unsigned short auto_run = 0;
    while ((int)i < argc)
    {
        if (strcmp(argv[i], "-ip") == 0)
        {
            ip = argv[++i];
        }
        else if (strcmp(argv[i], "-p") == 0)
        {
            port = (unsigned short)atoi(argv[++i]);
            if (port < 1024)
            {
                perror("Invalid port\n");
                return 1;
            }
        }
        else if (strcmp(argv[i], "-auto") == 0)
        {
            auto_run = (unsigned short)atoi(argv[++i]);
        }
        i++;
    }
    if (ip == NULL || port == 0)
    {
        perror("Invalid arguments\n");
        return 1;
    }
    printf("IP: %s\n", ip);
    printf("Port: %d\n", port);
    if (auto_run != 0)
        printf("Auto run: %d\n", auto_run);

    fprintf(stdout, "Connecting to %s:%d...\n", ip, port);
    rudp_sender *sender = rudp_open_sender(ip, port);
    if (!sender)
    {
        fprintf(stderr, "Failed to connect to open sender\n");
        return 1;
    }
    fprintf(stdout, "Successfully connected to the receiver!\n");

    char *message = util_generate_random_data(FILE_SIZE);

    int bytes_sent;
    char choice = 'Y';
    if (auto_run == 0)
    {
        do
        {
            // Try to send the message to the receiver using the socket.
            bytes_sent = rudp_send(sender, message, FILE_SIZE);
            // If the message sending failed, print an error message and return 1.
            // If no data was sent, print an error message and return 1. Only occurs if the connection was closed.
            if (bytes_sent <= 0)
            {
                fprintf(stderr, "rudp_send failed\n");
                rudp_close_sender(sender);
                return 1;
            }

            fprintf(stdout, "Sent %d bytes to the receiver!\n", bytes_sent);
            printf("Do you want to send again? (Y/n)\n");
            choice = getchar();
            getchar();
        } while (choice != 'n' && choice != 'N');
    }
    else
    {
        for (size_t i = 0; i < auto_run; i++)
        {
            bytes_sent = rudp_send(sender, message, FILE_SIZE);
            if (bytes_sent <= 0)
            {
                fprintf(stderr, "rudp_send failed\n");
                rudp_close_sender(sender);
                return 1;
            }
            fprintf(stdout, "Sent %d bytes to the receiver!\n", bytes_sent);
        }
    }
    free(message);
    
    rudp_close_sender(sender);

    fprintf(stdout, "Connection closed!\n");

    // Return 0 to indicate that the client ran successfully.
    return 0;
}