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
    char *algo = NULL;
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
        else if (strcmp(argv[i], "-algo") == 0)
        {
            if (strcmp(argv[i + 1], "reno") != 0 && strcmp(argv[i + 1], "cubic") != 0)
            {
                perror("Invalid algorithm\n");
                return 1;
            }
            algo = argv[++i];
        }
        else if (strcmp(argv[i], "-auto") == 0)
        {
            auto_run = (unsigned short)atoi(argv[++i]);
        }
        i++;
    }
    if (ip == NULL || port == 0 || algo == NULL)
    {
        perror("Invalid arguments\n");
        return 1;
    }
    printf("IP: %s\n", ip);
    printf("Port: %d\n", port);
    printf("Algorithm: %s\n", algo);
    if (auto_run != 0)
        printf("Auto run: %d\n", auto_run);

    struct sockaddr_in receiver;
    int sock = -1;
    char *message = util_generate_random_data(FILE_SIZE);
    memset(&receiver, 0, sizeof(receiver)); // Zero out the receiver structure

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1)
    {
        perror("The socket has failed");
        return 1;
    }

    // Convert the server's address from text to binary form and store it in the server structure.
    // This should not fail if the address is valid (e.g. "127.0.0.1").
    if (inet_pton(AF_INET, ip, &receiver.sin_addr) <= 0)
    {
        perror("inet_pton(3)");
        close(sock);
        return 1;
    }

    // Set the receiver's address family to AF_INET (IPv4).
    receiver.sin_family = AF_INET;

    // Set the reciever's port to the defined port. Note that the port must be in network byte order,
    // so we first convert it to network byte order using the htons function.
    receiver.sin_port = htons(port);

    socklen_t len = strlen(algo);
    setsockopt(sock, IPPROTO_TCP, TCP_CONGESTION, algo, len); // Set the algorithm of congestion control the socket would use.

    // A check to see if the congestion control algorithm passed successfully.
    if (getsockopt(sock, IPPROTO_TCP, TCP_CONGESTION, algo, &len) != 0)
    {
        perror("getsockopt");
        return -1;
    }

    fprintf(stdout, "Connecting to %s:%d...\n", ip, port);

    // Try to connect to the receiver using the socket and the receiver structure.
    if (connect(sock, (struct sockaddr *)&receiver, sizeof(receiver)) < 0)
    {
        perror("connect(2)");
        close(sock);
        return 1;
    }

    fprintf(stdout, "Successfully connected to the receiver!\n");

    int bytes_sent;
    char choice = 'Y';
    if (auto_run == 0)
    {
        do
        {
            // Try to send the message to the receiver using the socket.
            bytes_sent = send(sock, message, FILE_SIZE, 0);
            // If the message sending failed, print an error message and return 1.
            // If no data was sent, print an error message and return 1. Only occurs if the connection was closed.
            if (bytes_sent <= 0)
            {
                perror("send(2)");
                close(sock);
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
            bytes_sent = send(sock, message, FILE_SIZE, 0);
            if (bytes_sent <= 0)
            {
                perror("send(2)");
                close(sock);
                return 1;
            }
            fprintf(stdout, "Sent %d bytes to the receiver!\n", bytes_sent);
        }
    }
    free(message);
    char *ending_message = "Closing connection";
    // Try to send the message to the receiver using the socket.
    bytes_sent = send(sock, ending_message, strlen(ending_message) + 1, 0);
    
    // If the message sending failed, print an error message and return 1.
    // If no data was sent, print an error message and return 1. Only occurs if the connection was closed.
    if (bytes_sent <= 0)
    {
        perror("send(2)");
        close(sock);
        return 1;
    }
    // Close the socket with the receiver.
    close(sock);

    fprintf(stdout, "Connection closed!\n");

    // Return 0 to indicate that the client ran successfully.
    return 0;
}