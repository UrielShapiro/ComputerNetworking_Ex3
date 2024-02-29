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

/*
    * @brief A struct that will act as an ArrayList.
*/
typedef struct
{
    double *data;
    size_t capacity;
    size_t size;
} ArrayList;

// ------------------------ FUNCTIONS THAT ARE USED IN MAIN() -------------------------------

/*
    * @brief Add a number to the list.
    * @param list The list to add the number to.
    * @param num The number to add to the list.
*/
void addToList(ArrayList *list, double time)
{
    if (list->size == list->capacity)
    {
        list->data = realloc(list->data, sizeof(double) * (list->capacity *= 2));
    }
    list->data[list->size++] = time;
}
/*
    * @brief Convert the bytes to MegaBytes.
    * @param bytes The amount of bytes.
    * @return The amount of MegaBytes.
*/
int convertToMegaBytes(size_t bytes)
{
    size_t converstion = 1024 * 1024;
    return bytes / converstion;
}
/*
    * @brief Convert the bytes and time to speed in MB/s.
    * @param bytes The amount of bytes received.
    * @param time The time it took to receive the bytes.
    * @return The speed in MB/s.
*/
double convertToSpeed(double bytes, double time)
{
    return convertToMegaBytes(bytes) / (time / 1000);
}
/*
    * @brief Print the average time speed, and amount of runs of the messages received.
    * @param Times_list The list of times.
    * @param Speed_list The list of speeds.
    * @param run The amount of runs the program did.
    * @param format a boolean that says if how the output should be printed.
*/
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
/*
    * @brief Free the memory allocated for the lists and the buffer.
    * @param Times_list The list of times.
    * @param Speed_list The list of speeds.
    * @param buffer The buffer.
*/
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
        printf("Format: %d\n", format);
    }

    rudp_receiver *receiver = rudp_open_receiver(port);

    if (!receiver)
    {
        fprintf(stderr, "Failed to open receiver");
        return 1;
    }


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
    if(!format) printf("Size of input: %ld bytes\n", sizeof_input);
    size_t buffer_size = sizeof_input;
    char *buffer = calloc(buffer_size, sizeof(char));
    unsigned short noEndMessage = TRUE; // Indicator if the end message was received.
    if(format) printf("Time (ms),Speed (MB/s)\n");
    // The receiver's main loop.
    while (noEndMessage)
    {
        int bytes_received = 0;
        // Create a buffer to store the received message.
        clock_t start, end;
        double time_used_inMS;
        start = clock();
        bytes_received = rudp_recv(receiver, buffer, buffer_size);  // RUDP passes the entire message to the buffer all at once.
        end = clock();
        // If the message receiving failed, print an error message and return 1.
        if (bytes_received == -2)
        {
            fprintf(stderr, "Failed to receive from sender\n");
            rudp_close_receiver(receiver);
            endFree(&Times_list, &Speed_list, buffer);
            return 1;
        }
        // If the received message is "Closing connection", close the sender's socket and return 0.
        if (bytes_received == -1)
        {
            noEndMessage = FALSE;
            continue;
        }
        if (bytes_received == 0)
            continue;
        time_used_inMS = 1000 * (double)(end - start) / CLOCKS_PER_SEC; // Calculating the time it took for the message to be received.
        double speed = convertToSpeed(bytes_received, time_used_inMS);
        addToList(&Times_list, time_used_inMS);
        addToList(&Speed_list, speed);
        if (!format)
        {
            printf("Time taken to receive that messege: %f ms\n", time_used_inMS);
            printf("Speed: %f MB/s\n", speed);
        }
        if (format)
        {
            printf("%ld,%f,%f\n", run, time_used_inMS, (double)convertToMegaBytes(bytes_received) / (time_used_inMS / 1000));
        }
        if (!format)
            printf("Received %d bytes from the sender\n", bytes_received);
        run++;
    }
    if (!format)
        printf("Sender finished!\n");
    rudp_close_receiver(receiver);
    endPrints(&Times_list, &Speed_list, run, format);
    endFree(&Times_list, &Speed_list, buffer);
    return 0;
}