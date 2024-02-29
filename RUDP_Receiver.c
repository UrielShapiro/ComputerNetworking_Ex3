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

#define TRUE 1
#define FALSE 0

/*
 * @brief The maximum number of clients that the receiver can handle.
 * @note The default maximum number of clients is 1.
 */
#define MAX_CLIENTS 1

typedef struct
{
    double *data;
    size_t capacity;
    size_t size;
} ArrayList;

// ------------------------ FUNCTIONS THAT ARE USED IN MAIN() -------------------------------

void addToList(ArrayList *list, double time)
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
double convertToSpeed(double bytes, double time)
{
    return convertToMegaBytes(bytes) / (time / 1000);
}
void endPrints(ArrayList *Times_list, ArrayList *Speed_list, size_t run, unsigned short format)
{
    double avg_time = 0;
    double avg_speed = 0;
    for (size_t i = 0; i < Times_list->size && i < Speed_list->size; i++)
    {
        avg_time += Times_list->data[i];
        avg_speed += Speed_list->data[i];
    }
    avg_time = avg_time / Times_list->size;
    avg_speed = avg_speed / Speed_list->size;
    if (!format)
    {
        printf("Average time taken to receive a message: %f\n", avg_time);
        printf("Average speed: %f\n", avg_speed);
        printf("Number of runs: %ld\n", run);
    }
    if (format)
    {
        printf("Average,%f,%f\n", avg_time, avg_speed);
    }
}
void endFree(ArrayList *Times_list, ArrayList *Speed_list, char *buffer)
{
    free(Times_list->data);
    free(Speed_list->data);
    free(buffer);
}
//----------------------------------------------- MAIN -----------------------------------------------------------

int main(int argc, char **argv)
{
    //--------------------------------------PARSING THE INPUT---------------------------------------------------
    if (argc == 1)
    {
        printf("Cant run with no arguments\n");
        return 1;
    }
    size_t i = 0;
    unsigned short port = 0;
    unsigned short format = FALSE; // when true, print only the run #, runtime, and throughput (mb / s)
    while ((int)i < argc)          // Parsing the input
    {
        if (strcmp(argv[i], "-p") == 0)
        {
            port = (unsigned short)atoi(argv[++i]);
            if (port < 1024)
            {
                printf("Invalid port\n");
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
        printf("Invalid arguments\n");
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

    ArrayList Speed_list;
    Speed_list.data = malloc(sizeof(double));
    Speed_list.capacity = 1;
    Speed_list.size = 0;
    int input_size = 0;
    size_t sizeof_input;
    while (!input_size)
    {
        // Before sending the packege, the sender will send the weight of the package in bytes.
        input_size = rudp_recv(receiver, &sizeof_input, sizeof(sizeof_input));
    }
    sizeof_input = ntohl(sizeof_input);
    printf("Size of input: %ld bytes\n", sizeof_input);
    size_t BUFFER_SIZE = sizeof_input;
    char *buffer = calloc(BUFFER_SIZE, sizeof(char));
    unsigned short noEndMessage = TRUE; // Indicator if the end message was received.
    // The receiver's main loop.
    while (noEndMessage)
    {
        int bytes_received = 0;
        // Create a buffer to store the received message.
        size_t amount_of_bytes_received = 0;
        clock_t start, end;
        double time_used_inMS;
        start = clock();
        while (amount_of_bytes_received < BUFFER_SIZE && bytes_received >= 0)
        {
            bytes_received = rudp_recv(receiver, buffer, BUFFER_SIZE);

            amount_of_bytes_received += bytes_received;
        }
        // If the message receiving failed, print an error message and return 1.
        if (bytes_received == -2)
        {
            fprintf(stderr, "Failed to receive from sender\n");
            rudp_close_receiver(receiver);
            endFree(&Times_list, &Speed_list, buffer);
            return 1;
        }
        // Receive a message from the sender and store it in the buffer.
        end = clock();
        time_used_inMS = 1000 * (double)(end - start) / CLOCKS_PER_SEC; // Calculating the time it took for the message to be received.
        if (time_used_inMS > 0)
            addToList(&Times_list, time_used_inMS);
        double speed = convertToSpeed(amount_of_bytes_received, time_used_inMS);
        if (speed > 0)
            addToList(&Speed_list, speed);
        if (!format)
        {
            printf("Time taken to receive that messege: %f ms\n", time_used_inMS);
            printf("Speed: %f MB/s\n", speed);
        }
        // If the received message is "Closing connection", close the sender's socket and return 0.
        if (bytes_received == -1)
        {
            if (!format)
                fprintf(stdout, "Sender finished!\n");
            rudp_close_receiver(receiver);
            endPrints(&Times_list, &Speed_list, run, format);
            endFree(&Times_list, &Speed_list, buffer);
            noEndMessage = FALSE;
        }

        if (!format)
            fprintf(stdout, "Received %d bytes from the sender\n", bytes_received); // TODO: edit?
        if (format)
        {
            printf("%ld,%f,%f\n", run, time_used_inMS, (double)convertToMegaBytes(bytes_received) / (time_used_inMS / 1000));
            run++;
        }
        if (!format)
            fprintf(stdout, "Received %ld bytes from the sender\n", amount_of_bytes_received);
        if (format && bytes_received != -1)
        {
            printf("%ld,%f,%f\n", run, time_used_inMS, (double)convertToMegaBytes(amount_of_bytes_received) / (time_used_inMS / 1000));
        }
        run++;
    }
    return 0;
}