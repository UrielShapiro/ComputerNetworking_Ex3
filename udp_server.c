#include <stdio.h> // Standard input/output library
#include <arpa/inet.h> // For the in_addr structure and the inet_pton function
#include <sys/socket.h> // For the socket function
#include <unistd.h> // For the close function
#include <string.h> // For the memset function

/*
 * @brief The UDP's server port.
 * @note The default port is 5060.
*/
#define SERVER_PORT 5061

/*
 * @brief The buffer size to store the received message.
 * @note The default buffer size is 1024.
*/
#define BUFFER_SIZE 1024

/*
 * @brief UDP Server main function.
 * @param None
 * @return 0 if the server runs successfully, 1 otherwise.
*/
int main(void) {
    // The variable to store the socket file descriptor.
    int sock = -1;

    // The variable to store the server's address.
    struct sockaddr_in server;

    // The variable to store the client's address.
    struct sockaddr_in client;

    // Stores the client's structure length.
    socklen_t client_len = sizeof(client);

    // Create a message to send to the client.
    char *message = "Good morning, Vietnam\n";

    // Get the message length.
    int messageLen = strlen(message) + 1;

    // The variable to store the socket option for reusing the server's address.
    int opt = 1;

    // Reset the server and client structures to zeros.
    memset(&server, 0, sizeof(server));
    memset(&client, 0, sizeof(client));

    // Try to create a UDP socket (IPv4, datagram-based, default protocol).
    sock = socket(AF_INET, SOCK_DGRAM, 0);

    if (sock == -1)
    {
        perror("socket(2)");
        return 1;
    }

    // Set the socket option to reuse the server's address.
    // This is useful to avoid the "Address already in use" error message when restarting the server.
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        perror("setsockopt(2)");
        close(sock);
        return 1;
    }

    // Set the server's address to "0.0.0.0" (all IP addresses on the local machine).
    server.sin_addr.s_addr = INADDR_ANY;

    // Set the server's address family to AF_INET (IPv4).
    server.sin_family = AF_INET;

    // Set the server's port to the specified port. Note that the port must be in network byte order.
    server.sin_port = htons(SERVER_PORT);

    // Try to bind the socket to the server's address and port.
    if (bind(sock, (struct sockaddr *)&server, sizeof(server)) < 0)
    {
        perror("bind(2)");
        close(sock);
        return 1;
    }

    fprintf(stdout, "Listening on port %d...\n", SERVER_PORT);

    // The server's main loop.
    while (1)
    {
        // Create a buffer to store the received message.
        char buffer[BUFFER_SIZE] = {0};

        fprintf(stdout, "Waiting for a message from a client...\n");

        // Receive a message from the client and store it in the buffer.
        int bytes_received = recvfrom(sock, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&client, &client_len);

        // If the message receiving failed, print an error message and return 1.
        // If the amount of received bytes is 0, the client sent an empty message, so we ignore it.
        if (bytes_received < 0)
        {
            perror("recvfrom(2)");
            close(sock);
            return 1;
        }

        // Ensure that the buffer is null-terminated, no matter what message was received.
        // This is important to avoid SEGFAULTs when printing the buffer.
        if (buffer[BUFFER_SIZE - 1] != '\0')
            buffer[BUFFER_SIZE- 1] = '\0';

        fprintf(stdout, "Received %d bytes from the client %s:%d: %s\n", bytes_received, inet_ntoa(client.sin_addr), ntohs(client.sin_port), buffer);

        // Send back a message to the client.
        int bytes_sent = sendto(sock, message, messageLen, 0, (struct sockaddr *)&client, client_len);

        // If the message sending failed, print an error message and return 1.
        // We do not need to check for 0 bytes sent, as if the client disconnected, we would have already closed the socket.
        if (bytes_sent < 0)
        {
            perror("sendto(2)");
            close(sock);
            return 1;
        }
        
        fprintf(stdout, "Sent %d bytes to the client %s:%d!\n", bytes_sent, inet_ntoa(client.sin_addr), ntohs(client.sin_port));
    }

    return 0;
}