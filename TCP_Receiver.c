#include <stdio.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

/*
 * @brief The maximum number of clients that the receiver can handle.
 * @note The default maximum number of clients is 1.
 */
#define MAX_CLIENTS 1

/*
 * @brief The buffer size to store the received message.
 * @note The default buffer size is 1024.
 */
#define BUFFER_SIZE 2097152
#define RECEIVED_FILE_SIZE 2097152

typedef struct
{
    double *data;
    size_t capacity;
    size_t size;
} ArrayList;

void addToList(ArrayList *list, double num)
{
    if (list->size == list->capacity)
    {
        list->data = realloc(list->data, sizeof(double) * (list->capacity *= 2));
    }
    list->data[list->size++] = num;
}

double convertToMegaBytes(size_t bytes)
{
    size_t converstion = 1024 * 1024;
    return bytes / converstion;
}
double convertToSpeed(double bytes, double time)
{
    return convertToMegaBytes(bytes) / (time / 1000);
}
int main(int argc, char **argv)
{
    size_t i = 0;
    unsigned short port = 0;
    char *algo = NULL;
    unsigned short format = 0; // when true, print only the run #, runtime, and throughput (mb / s)
    while ((int)i < argc)
    {
        if (strcmp(argv[i], "-p") == 0)
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
        else if (strcmp(argv[i], "-format") == 0)
        {
            format = 1;
        }
        i++;
    }
    if (port == 0 || algo == NULL)
    {
        perror("Invalid arguments\n");
        return 1;
    }
    if (!format)
    {
        printf("Port: %d\n", port);
        printf("Algorithm: %s\n", algo);
        printf("Auto Run: %d\n", format);
    }

    // The variable to store the socket file descriptor.
    int sock = -1;

    // The variable to store the receiver's address.
    struct sockaddr_in receiver;

    // The variable to store the sender's address.
    struct sockaddr_in sender;

    // Stores the sender's structure length.
    socklen_t client_len = sizeof(sender);

    // The variable to store the socket option for reusing the receiver's address.
    int opt = 1;

    // Reset the receiver and sender structures to zeros.
    memset(&receiver, 0, sizeof(receiver));
    memset(&sender, 0, sizeof(sender));

    // Try to create a TCP socket (IPv4, stream-based, default protocol).
    sock = socket(AF_INET, SOCK_STREAM, 0);

    if (sock == -1)
    {
        perror("socket(2)");
        return 1;
    }

    // Set the socket option to reuse the receiver's address.
    // This is useful to avoid the "Address already in use" error message when restarting the receiver.
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        perror("setsockopt(2)");
        close(sock);
        return 1;
    }

    // Set the receiver's address to "0.0.0.0" (all IP addresses on the local machine).
    receiver.sin_addr.s_addr = INADDR_ANY;

    // Set the receiver's address family to AF_INET (IPv4).
    receiver.sin_family = AF_INET;

    // Set the receiver's port to the specified port. Note that the port must be in network byte order.
    receiver.sin_port = htons(port);

    socklen_t len = strlen(algo);
    setsockopt(sock, IPPROTO_TCP, TCP_CONGESTION, algo, len); // Set the algorithm of congestion control the socket would use.

    // A check to see if the congestion control algorithm passed successfully.
    if (getsockopt(sock, IPPROTO_TCP, TCP_CONGESTION, algo, &len) != 0)
    {
        perror("getsockopt");
        return -1;
    }

    // Try to bind the socket to the receiver's address and port.
    if (bind(sock, (struct sockaddr *)&receiver, sizeof(receiver)) < 0)
    {
        perror("bind(2)");
        close(sock);
        return 1;
    }

    // Try to listen for incoming connections.
    if (listen(sock, MAX_CLIENTS) < 0)
    {
        perror("listen(2)");
        close(sock);
        return 1;
    }
    if (!format)
    {
        fprintf(stdout, "Listening for incoming connections on port %d...\n", port);
    }

    int client_sock = -1;
    // Try to accept a new sender connection.
    client_sock = accept(sock, (struct sockaddr *)&sender, &client_len);

    // If the accept call failed, print an error message and return 1.
    if (client_sock < 0)
    {
        perror("accept(2)");
        close(sock);
        return 1;
    }
    // Print a message to the standard output to indicate that a new sender has connected.
    if (!format)
        fprintf(stdout, "Client %s:%d connected\n", inet_ntoa(sender.sin_addr), ntohs(sender.sin_port));

    size_t run = 0;
    ArrayList Times_list;
    Times_list.data = malloc(sizeof(double));
    Times_list.capacity = 1;
    Times_list.size = 0;

    ArrayList Speed_list;
    Speed_list.data = malloc(sizeof(double));
    Speed_list.capacity = 1;
    Speed_list.size = 0;
    // The receiver's main loop.
    while (1)
    {
        int bytes_received;
        // Create a buffer to store the received message.
        char buffer[BUFFER_SIZE] = {0};
        size_t amount_of_bytes_received = 0;
        clock_t start, end;
        double time_used_inMS;
        start = clock();
        while (amount_of_bytes_received < RECEIVED_FILE_SIZE && strcmp(buffer, "Closing connection") != 0)
        {
            // Receive a message from the sender and store it in the buffer.
            bytes_received = recv(client_sock, buffer, BUFFER_SIZE, 0);

            // If the message receiving failed, print an error message and return 1.
            if (bytes_received < 0)
            {
                perror("recv(2)");
                close(client_sock);
                close(sock);
                return 1;
            }
            amount_of_bytes_received += bytes_received;
        }
        end = clock();
        time_used_inMS = 1000 * (double)(end - start) / CLOCKS_PER_SEC; // Calculating the time it took for the message to be received.
        addToList(&Times_list, time_used_inMS);
        double speed = convertToSpeed(amount_of_bytes_received, time_used_inMS);
        addToList(&Speed_list, speed);
        if (!format)
        {
            printf("Time taken to receive that messege: %f ms\n", time_used_inMS);
            printf("Speed: %f MB/s\n", speed);
        }
        // Ensure that the buffer is null-terminated, no matter what message was received.
        // This is important to avoid SEGFAULTs when printing the buffer.
        if (buffer[BUFFER_SIZE - 1] != '\0')
            buffer[BUFFER_SIZE - 1] = '\0';

        if (!format)
            fprintf(stdout, "Received %ld bytes from the sender %s:%d\n", amount_of_bytes_received, inet_ntoa(sender.sin_addr), ntohs(sender.sin_port));
        if(format && strcmp(buffer, "Closing connection") != 0)
        {
            printf("%ld,%f,%f\n", run, time_used_inMS, (double)convertToMegaBytes(amount_of_bytes_received) / (time_used_inMS / 1000));
            run++;
        }
        
        
        // If the received message is "Closing connection", close the sender's socket and return 0.
        if (strcmp(buffer, "Closing connection") == 0)
        {
            if (!format)
                fprintf(stdout, "Sender finished!\n");
            close(client_sock);
            if (!format)
                fprintf(stdout, "Client %s:%d disconnected\n", inet_ntoa(sender.sin_addr), ntohs(sender.sin_port));
            if (!format)
                fprintf(stdout, "Server finished!\n");
            close(sock);
            double avg_time = 0;
            double avg_speed = 0;
            for (size_t i = 0; i < Times_list.size; i++)
            {
                avg_time += Times_list.data[i];
                avg_speed += Speed_list.data[i];
            }
            avg_time = avg_time / Times_list.size;
            avg_speed = avg_speed / Speed_list.size;
            if (!format)
                printf("Average time taken to receive a message: %f\n", avg_time);
            if (!format)
                printf("Average speed: %f\n", avg_speed);
            if(format)
            {
                printf("Average,%f,%f\n", avg_time, avg_speed);
            }
            free(Times_list.data);
            return 0;
        }
    }

    return 0;
}