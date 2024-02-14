#include <stdio.h> // Standard input/output library
#include <arpa/inet.h> // For the in_addr structure and the inet_pton function
#include <sys/socket.h> // For the socket function
#include <unistd.h> // For the close function
#include <string.h> // For the memset function
#include <sys/time.h> // For tv struct

/*
 * @brief The UDP's server IP address to send the message to.
 * @note The default IP address is 127.0.0.1 (localhost)
*/
#define SERVER_IP "127.0.0.1"

/*
 * @brief The UDP's server port to send the message to.
 * @note The default port is 5061.
*/
#define SERVER_PORT 5061

/*
 * @brief The buffer size to store the received message.
 * @note The default buffer size is 1024.
*/
#define BUFFER_SIZE 1024

/*
 * @brief Defines the maximum wait time for recvfrom(2) call until the client drops
 * @note The default time is 2 seconds.
*/
#define MAX_WAIT_TIME 2

/*
 * @brief UDP Client main function.
 * @param None
 * @return 0 if the client runs successfully, 1 otherwise.
*/
int main(void) {
    // The variable to store the socket file descriptor.
    int sock = -1;

    // The variable to store the server's address.
    struct sockaddr_in server;

    // Create a message to send to the server.
    char *message = "Hello from client";

    // Create a buffer to store the received message.
    char buffer[BUFFER_SIZE] = {0};

    // Create a timeval struct to store the maximum wait time for the recvfrom(2) call.
    struct timeval tv = { MAX_WAIT_TIME, 0 };

    // Reset the server structure to zeros.
    memset(&server, 0, sizeof(server));

    // Try to create a UDP socket (IPv4, datagram-based, default protocol).
    sock = socket(AF_INET, SOCK_DGRAM, 0);

    // If the socket creation failed, print an error message and return 1.
    if (sock == -1)
    {
        perror("socket(2)");
        return 1;
    }

    // Convert the server's address from text to binary form and store it in the server structure.
    // This should not fail if the address is valid (e.g. "127.0.0.1").
    if (inet_pton(AF_INET, SERVER_IP, &server.sin_addr) <= 0)
    {
        perror("inet_pton(3)");
        close(sock);
        return 1;
    }

    // Set the server's address family to AF_INET (IPv4).
    server.sin_family = AF_INET;

    // Set the server's port to the defined port. Note that the port must be in network byte order,
    // so we first convert it to network byte order using the htons function.
    server.sin_port = htons(SERVER_PORT);

    // Since in UDP there is no connection, there isn't a guarantee that the server is up and running.
    // Therefore, we set a timeout for the recvfrom(2) call using the setsockopt function.
    // If the server does not respond within the timeout, the client will drop.
    // The timeout is set to 2 seconds by default.
    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&tv, sizeof(tv)) == -1)
    {
        perror("setsockopt(2)");
        close(sock);
        return 1;
    }

    fprintf(stdout, "Sending message to the server: %s\n", message);

    // Try to send the message to the server using the created socket and the server structure.
    int bytes_sent = sendto(sock, message, strlen(message), 0, (struct sockaddr *)&server, sizeof(server));

    // If the message sending failed, print an error message and return 1.
    // If no data was sent, print an error message and return 1. In UDP, this should not happen unless the message is empty.
    if (bytes_sent <= 0)
    {
        perror("sendto(2)");
        close(sock);
        return 1;
    }

    fprintf(stdout, "Sent %d bytes to the server!\n"
                    "Waiting for the server to respond...\n", bytes_sent);

    // The variable to store the server's address, that responded to the message.
    // Note that this variable is not used in the client, but it is required by the recvfrom function.
    // Also note that the target server might be different from the server that responded to the message.
    struct sockaddr_in recv_server;

    // The variable to store the server's address length.
    socklen_t recv_server_len = sizeof(recv_server);

    // Try to receive a message from the server using the socket and store it in the buffer.
    int bytes_received = recvfrom(sock, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&recv_server, &recv_server_len);

    // If the message receiving failed, print an error message and return 1.
    // If no data was received, print an error message and return 1. In UDP, this should not happen unless the message is empty.
    if (bytes_received <= 0)
    {
        perror("recvfrom(2)");
        close(sock);
        return 1;
    }

    // Ensure that the buffer is null-terminated, no matter what message was received.
    // This is important to avoid SEGFAULTs when printing the buffer.
    if (buffer[BUFFER_SIZE - 1] != '\0')
        buffer[BUFFER_SIZE- 1] = '\0';

    // Print the received message, and the server's address and port.
    fprintf(stdout, "Got %d bytes from %s:%d: %s\n", bytes_received, inet_ntoa(recv_server.sin_addr), ntohs(recv_server.sin_port), buffer);

    // Close the socket UDP socket.
    close(sock);

    fprintf(stdout, "Client finished!\n");

    // Return 0 to indicate that the client ran successfully.
    return 0;
}