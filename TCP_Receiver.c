#include <stdio.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h> // For struct timeval

#define TRUE 1
#define FALSE 0
/*
 * @brief The maximum number of clients that the receiver can handle.
 * @note The default maximum number of clients is 1.
 */
#define MAX_CLIENTS 1
#define RECV_TIMEOUT_US 100000
#define RECV_TIMEOUT_S 2

#define FIN "Closing connection"
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
void addToList(ArrayList *list, double num)
{
    if (list->size == list->capacity)
    {
        list->data = realloc(list->data, sizeof(double) * (list->capacity *= 2));
    }
    list->data[list->size++] = num;
}
/*
 * @brief Convert the bytes to MegaBytes.
 * @param bytes The amount of bytes.
 * @return The amount of MegaBytes.
 */
double convertToMegaBytes(size_t bytes)
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
 * @brief Close the sockets and print a message that they were closed.
 * @param sock The main receiver main socket.
 * @param client_sock The client socket.
 * @param format a boolean that says if how the output should be printed.
 * @param sender The sender's address.
 */
void CloseSockets(int *sock, int *client_sock, unsigned short format, struct sockaddr_in sender)
{
    close(*client_sock);
    if (!format)
        printf("Client %s:%d disconnected\n", inet_ntoa(sender.sin_addr), ntohs(sender.sin_port));
    close(*sock);
    if (!format)
        printf("Closing connection!\n");
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
    char *algo = NULL;
    unsigned short format = FALSE; // when true, print only the run #, runtime, and throughput (mb / s)
    while ((int)i < argc)          // Parsing the arguments passed to the program.
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
        else if (strcmp(argv[i], "-algo") == 0)
        {
            if (strcmp(argv[i + 1], "reno") != 0 && strcmp(argv[i + 1], "cubic") != 0)
            {
                printf("Invalid algorithm\n"); // If the algorithm is not reno or cubic, print an error message and return 1.
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
        printf("Invalid arguments\n");
        return 1;
    }
    if (!format)
    {
        printf("Port: %d\n", port);
        printf("Algorithm: %s\n", algo);
        printf("Auto Run: %d\n", format);
    }
    //---------------------------------------CONFIGURING SOCKETS------------------------------------------------
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
    // If the socket creation failed, print an error message and return 1.
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
    if (setsockopt(sock, IPPROTO_TCP, TCP_CONGESTION, algo, len) != 0) // Set the algorithm of congestion control the socket would use.
    {
        perror("setsockopt");
        return -1;
    }
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
        printf("Listening for incoming connections on port %d...\n", port);
    }
    //-----------------------CONFIGURING THE CLIENT SOCKET AND RECEIVING THE MESSAGES-------------------------
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

    struct timeval timeout;
    timeout.tv_sec = RECV_TIMEOUT_S;
    timeout.tv_usec = RECV_TIMEOUT_US;
    // Set a timeout for the receiver socket.
    if (setsockopt(client_sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0)
    {
        perror("Error setting timeout for the receiver socket\n");
        close(client_sock);
        close(sock);
        return 1;
    }

    // Print a message to the standard output to indicate that a new sender has connected.
    if (!format)
        fprintf(stdout, "Client %s:%d connected\n", inet_ntoa(sender.sin_addr), ntohs(sender.sin_port));

    size_t run = 0;       // will count the amount of packeges received.
    ArrayList Times_list; // A list to store the time it took to receive each message.
    Times_list.data = malloc(sizeof(double));
    Times_list.capacity = 1;
    Times_list.size = 0;

    ArrayList Speed_list; // A list to store the speed of each message received.
    Speed_list.data = malloc(sizeof(double));
    Speed_list.capacity = 1;
    Speed_list.size = 0;
    int input_size = 0;
    size_t sizeof_input;

    if(format) printf("Time (ms),Speed (MB/s)\n");
    while (!input_size)
    {
        // Before sending the packege, the sender will send the weight of the package in bytes.
        input_size = recv(client_sock, &sizeof_input, sizeof(sizeof_input), 0);
    }
    sizeof_input = ntohl(sizeof_input);
    if (!format)
        printf("Size of input: %ld bytes\n", sizeof_input);
    size_t buffer_size = sizeof_input; // The size of the buffer will be the size of the input.
    char *buffer = calloc(buffer_size, sizeof(char));
    unsigned short noEndMessage = TRUE; // Indicator if the end message was received.
    // The receiver's main loop.
    while (noEndMessage)
    {
        // A variable to store the amout of bytes received in each recv().
        int bytes_received;
        // Create a buffer to store the received message.
        // char *endmessage = buffer + strlen(buffer) - 18; // Before sender disconnects, endmessage will be "FIN".
        size_t total_of_bytes_received = 0;
        clock_t start, end;
        double time_used_inMS;
        start = clock();
        while (total_of_bytes_received < sizeof_input && strcmp(buffer, FIN) != 0)
        {
            // Receive a message from the sender and store it in the buffer.
            bytes_received = recv(client_sock, buffer, buffer_size, 0);

            // If the message receiving failed, print an error message and return 1.
            if (bytes_received < 0)
            {
                perror("Reached timeout");
                CloseSockets(&sock, &client_sock, format, sender);
                endFree(&Times_list, &Speed_list, buffer);
                return 1;
            }
            if (bytes_received == 0) // If the sender disconnected, break the loop.
            {
                noEndMessage = FALSE;
                break;
            }
            total_of_bytes_received += bytes_received;
        }
        end = clock();
        time_used_inMS = 1000 * (double)(end - start) / CLOCKS_PER_SEC; // Calculating the time it took for the message to be received.
        double speed = convertToSpeed(total_of_bytes_received, time_used_inMS);
        if (time_used_inMS > 0 && speed > 0 && noEndMessage)
        {
            // Add the time and speed to their respective lists.
            addToList(&Times_list, time_used_inMS);
            addToList(&Speed_list, speed);
        }
        if (!format)
        {
            printf("Time taken to receive that messege: %f ms\n", time_used_inMS);
            printf("Speed: %f MB/s\n", speed);
        }

        if (!format)
            printf("Received %ld bytes from the sender %s:%d\n", total_of_bytes_received, inet_ntoa(sender.sin_addr), ntohs(sender.sin_port));
        if (format && strcmp(buffer, FIN) != 0 && noEndMessage)
        {
            printf("%ld,%f,%f\n", run, time_used_inMS, (double)convertToMegaBytes(total_of_bytes_received) / (time_used_inMS / 1000));
        }
        if (noEndMessage)
        {
            run++; // Increment the run counter.
        }
        // If the received message is "Closing connection", close the sender's socket and return 0.
        if (strcmp(buffer, FIN) == 0)
        {
            noEndMessage = FALSE;
        }
        else if (!format)
        {
            printf("run: %ld\n", run);
        }
    }
    // Closing the socket, and printing the average time and speed before exiting the program.
    CloseSockets(&sock, &client_sock, format, sender);
    endPrints(&Times_list, &Speed_list, run - 1, format);
    endFree(&Times_list, &Speed_list, buffer);
    return 0;
}