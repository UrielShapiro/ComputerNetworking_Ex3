#include <stdio.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "rudp.h"

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

void addTimeToList(ArrayList *list, double time)
{
    if (list->size == list->capacity)
    {
        list->data = realloc(list->data, sizeof(double) * (list->capacity *= 2));
    }
    list->data[list->size++] = time;
}

int convertToMegaBytes(size_t bytes)
{
    size_t converstion = 1024 * 1024;
    return bytes / converstion;
}

int main(int argc, char **argv)
{
    size_t i = 0;
    unsigned short port = 0;
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
        else if (strcmp(argv[i], "-format") == 0)
        {
            format = 1;
        }
        i++;
    }
    if (port == 0)
    {
        perror("Invalid arguments\n");
        return 1;
    }
    if (!format)
    {
        printf("Port: %d\n", port);
        printf("Auto Run: %d\n", format);
    }

    rudp_receiver *receiver = rudp_open_receiver(port);

    if (!receiver)
    {
        fprintf(stderr, "Failed to open receiver");
        return 1;
    }

    // Print a message to the standard output to indicate that a new sender has connected.
    // if (!format)
    //     fprintf(stdout, "Client %s:%d connected\n", inet_ntoa(sender.sin_addr), ntohs(sender.sin_port));     // TODO: edit to use sender address?

    size_t run = 0;
    ArrayList Times_list;
    Times_list.data = malloc(sizeof(double));
    Times_list.capacity = 1;
    Times_list.size = 0;
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
            bytes_received = rudp_recv(receiver, buffer, BUFFER_SIZE);

            // If the message receiving failed, print an error message and return 1.
            if (bytes_received == -2)
            {
                fprintf(stderr, "Failed to receive from sender\n");
                rudp_close_receiver(receiver);
                return 1;
            }
            if (bytes_received == -1)
            {
                break;
            }
            amount_of_bytes_received += bytes_received;
        }
        end = clock();
        time_used_inMS = 1000 * (double)(end - start) / CLOCKS_PER_SEC; // Calculating the time it took for the message to be received.
        if (format == 0)
            printf("Time taken to receive that messege: %f ms\n", time_used_inMS);
        addTimeToList(&Times_list, time_used_inMS);

        // if (!format)
        //     fprintf(stdout, "Received %ld bytes from the sender %s:%d\n", amount_of_bytes_received, inet_ntoa(sender.sin_addr), ntohs(sender.sin_port)); // TODO: edit?
        if(format)
        {
            printf("%ld,%f,%f\n", run, time_used_inMS, (double)convertToMegaBytes(amount_of_bytes_received) / (time_used_inMS / 1000));
            run++;
        }
        
        
        // If the received message is "Closing connection", close the sender's socket and return 0.
        if (bytes_received == -1)
        {
            if (!format)
                fprintf(stdout, "Sender finished!\n");
            // if (!format)
            //     fprintf(stdout, "Client %s:%d disconnected\n", inet_ntoa(sender.sin_addr), ntohs(sender.sin_port));
            rudp_close_receiver(receiver);
            double avg = 0;
            for (size_t i = 0; i < Times_list.size; i++)
            {
                avg += Times_list.data[i];
            }
            avg = avg / Times_list.size;
            if (!format)
                printf("Average time taken to receive a message: %f\n", avg);
            free(Times_list.data);
            return 0;
        }
    }

    return 0;
}