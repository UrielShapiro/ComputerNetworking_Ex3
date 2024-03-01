#include "rudp.h"
#include <sys/socket.h> // For the socket function
#include <arpa/inet.h>  // For the in_addr structure and the inet_pton function
#include <stdlib.h>     // For malloc and free
#include <stdio.h>      // For perror
#include <string.h>     // For memset
#include <unistd.h>     // For close
#include <sys/time.h>   // For struct timeval

// debug modes
// #define DEBUG
// #define CHECKSUM_DEBUG
#define TRUE 1
#define FALSE 0
// parameters of the protocol
#define ACK_TIMEOUT_US 100000
#define ACK_TIMEOUT_S 0
#define RECV_TIMEOUT_US 0
#define RECV_TIMEOUT_S 2
#define MAX_RETRIES 15

#define IP_HEADER_SIZE 20
#define UDP_HEADER_SIZE 8
// the maximum segment size that can be transmitted over RUDP under UDP under IP
#define MAX_SEGMENT_SIZE ((1 << 16) - 1 - IP_HEADER_SIZE - UDP_HEADER_SIZE - sizeof(rudp_header))

unsigned short calculate_checksum(void *data, unsigned int bytes)
{
#ifdef CHECKSUM_DEBUG
    printf(" checksum of data with size %d\n", bytes);
    printf("Data is: ");
    for (size_t i = 0; i < bytes; ++i)
        printf("%02x", ((char *)data)[i]);
    printf("\n");
#endif

    unsigned short *data_words = (unsigned short *)data;    // Data_words is a pointer to the data
    unsigned int sum = 0;

    while (bytes > 1)
    {
        sum += *(unsigned short *)data_words++;
        bytes -= 2;
    }

    /*  Add left-over byte, if exist */
    if (bytes > 0)
        sum += *(unsigned char *)data_words;

    /*  Fold 32-bit sum to 16 bits */
    while (sum >> 16)
        sum = (sum & 0xffff) + (sum >> 16);
#ifdef CHECKSUM_DEBUG
    printf("Calculated checksum is %hu\n", ~(unsigned short)sum);
#endif
    return ~(unsigned short)sum;    // Return the one's complement of the sum which is the checksum.
}

/**
 * Sets the checksum of a message
 */
void set_checksum(void *rudp_message)
{
#ifdef CHECKSUM_DEBUG
    printf("Setting checksum");
#endif
    rudp_header *header = (rudp_header *)rudp_message;
    header->checksum = 0;   // Set the checksum to 0, is used to calculate the checksum
    header->checksum = calculate_checksum(rudp_message, sizeof(rudp_header) + header->len); // Asign the checksum to the header
}

/**
 * Validates the checksum of a message
 */
int validate_checksum(void *rudp_message)
{
#ifdef CHECKSUM_DEBUG
    printf("Validating");
#endif
    return 0 == calculate_checksum(rudp_message, sizeof(rudp_header) + ((rudp_header *)rudp_message)->len); // Return true if the checksum is valid. i.e is equal to 0.
}
/**
 * Opens an RUDP sender and connects to a receiver in the given address and port
 * Returns a pointer to the sender.
 * Returns NULL on error, including no receiver accepting the connection.
*/
rudp_sender *rudp_open_sender(char *address, unsigned short port)
{
    rudp_sender *this = malloc(sizeof(rudp_sender));    // Allocate memory for the sender

    this->sock = socket(AF_INET, SOCK_DGRAM, 0);    // Create a socket for the sender
    if (this->sock == -1)                           // If the socket creation fails
    {
        perror("Error on socket creation at open_sender");
        free(this);
        return NULL;
    }

    struct sockaddr_in receiver_address = {0};  // Zero out the receiver address

    // Convert the address to a binary representation
    if (inet_pton(AF_INET, address, &receiver_address.sin_addr) <= 0)   // If the address is invalid, return NULL.
    {
        perror("Error on address conversion at open_sender");
        close(this->sock);
        free(this);
        return NULL;
    }
    receiver_address.sin_family = AF_INET;      // Set the address family to IPv4.
    receiver_address.sin_port = htons(port);    // Set the port number to the given port.

    this->peer_address = *(struct sockaddr *)&receiver_address;   // Save the address of the receiver in the sender socket.

    this->peer_address_size = sizeof(receiver_address);

    struct timeval timeout;
    timeout.tv_sec = ACK_TIMEOUT_S;
    timeout.tv_usec = ACK_TIMEOUT_US;
    // Set the timeout for the sender socket.
    if (setsockopt(this->sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) // If the timeout setting fails, return NULL.
    {
        perror("Error setting timeout at open_sender");
        close(this->sock);
        free(this);
        return NULL;
    }

    rudp_header syn_message = {0}; // We will send only the header, therefore we initialize the message to 0.
    syn_message.len = 0;
    syn_message.flags = SYN;    // Set the SYN flag to 1 for the SYN message.
    set_checksum(&syn_message); // Set the checksum of the message.

    unsigned int remaining_tries;  //Initialize a variable to store the maximum number of retries.
    int successful = FALSE;
    for (remaining_tries = MAX_RETRIES; remaining_tries > 0 && !successful; --remaining_tries)  // Loop until the maximum number of retries is reached or the connection is successful.
    {
#ifdef DEBUG
        printf("Attempting SYN, remaining: %d\n", remaining_tries);
#endif
        // Send the SYN message to the receiver.
        if (sendto(this->sock, &syn_message, sizeof(syn_message), 0, &this->peer_address, this->peer_address_size) <= 0)
        {
            perror("Error sending SYN message at open_sender");
            close(this->sock);
            free(this);
            return NULL;
        }

        rudp_header ack_message = {0};  // We will receive only the header, therefore we initialize the message to 0.
        if (recv(this->sock, &ack_message, sizeof(ack_message), 0) < 0) // Receive the ACK message from the receiver and store it in the ack_message.
        {
            perror("Error receiving ACK at open_sender");
            continue;
        }
        if (!validate_checksum(&ack_message))
        {
            fprintf(stderr, "Error on ACK message at open_sender\n");
            continue;
        }
        if (!(ack_message.flags & (ACK | SYN)))
        {
            fprintf(stderr, "Error in ACK message flags at open_sender\n");
            continue;
        }
        // if we made it here we were successful
        successful = TRUE;
    }

    if (remaining_tries == 0)   // If the maximum number of retries is reached, a connection was not established.
    {
        fprintf(stderr, "Ran out of retries to send SYN at open_sender\n");
        close(this->sock);
        free(this);
        return NULL;
    }

    return this;
}
/*
    * Opens an RUDP receiver and waits for a connection from a sender.
    * Listens on the given port. and accepts the first connection.
    * Returns a pointer to the receiver.
    * Returns NULL on error
*/
rudp_receiver *rudp_open_receiver(unsigned short port) 
{
    rudp_receiver *this = malloc(sizeof(rudp_receiver));

    this->sock = socket(AF_INET, SOCK_DGRAM, 0);    // Create a socket for the receiver
    if (this->sock == -1)
    {
        perror("Error on socket creation at open_receiver");
        free(this);
        return NULL;
    }

    int reuse = TRUE;   // Set the reuse option to true, so we could reuse the address.
    if (setsockopt(this->sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0)
    {
        perror("Error while setting reuse address option at open_receiver");
        close(this->sock);
        free(this);
        return NULL;
    }

    struct sockaddr_in my_address = {0};        // Initialize the receiver address to 0.
    my_address.sin_addr.s_addr = INADDR_ANY;
    my_address.sin_family = AF_INET;            // Set the address family to IPv4.
    my_address.sin_port = htons(port);          // Set the port number to the given port.

    // Bind the socket to the receiver address.
    if (bind(this->sock, (struct sockaddr *)&my_address, sizeof(my_address)) < 0)
    {
        perror("Error while binding socket at open_receiver");
        close(this->sock);
        free(this);
        return NULL;
    }

    this->peer_address_size = sizeof(this->peer_address);

    rudp_header syn_message = {0};  // We will receive only the header, therefore we initialize the message to 0.
    // Receive the SYN message from the sender.
    if (recvfrom(this->sock, &syn_message, sizeof(syn_message), 0, &this->peer_address, &this->peer_address_size) < 0)
    {
        perror("Error trying to receive SYN message from sender at open_receiver");
        close(this->sock);
        free(this);
        return NULL;
    }
    if (!validate_checksum(&syn_message))   // Validate the checksum of the received message.
    {
        fprintf(stderr, "Error in SYN message checksum at open_receiver\n");
        close(this->sock);
        free(this);
        return NULL;
    }
    if (!(syn_message.flags & SYN)) // Check if the message is a SYN message.
    {
        fprintf(stderr, "Error in SYN message flags at open_receiver\n");
        close(this->sock);
        free(this);
        return NULL;
    }

    rudp_header ack_message = {0}; // we will read only the header, therefore we initialize the message to 0.
    ack_message.len = 0;
    ack_message.flags = ACK | SYN;  // Send a messeage with ACK and SYN flags.
    set_checksum(&ack_message);     // Set the checksum of the message.

    // Send the SYN ACK message to the sender.
    if (sendto(this->sock, &ack_message, sizeof(ack_message), 0, &this->peer_address, this->peer_address_size) <= 0)
    {
        perror("Error sending SYN-ACK message at open_receiver");
        close(this->sock);
        free(this);
        return NULL;
    }

    return this;    // Return the receiver address.
}

/*
    * Send a FIN to the associated receiver and closes the connection.
*/
void rudp_close_sender(rudp_sender *this)
{
    rudp_header close_message = {0};    // We will read only the header, therefore we initialize the message to 0.
    close_message.len = 0;
    close_message.flags = FIN;          // Set the FIN flag to 1 for the FIN message.
    set_checksum(&close_message);       // Set the checksum of the message.
    unsigned int remaining_tries;
    int successful = FALSE;
    for (remaining_tries = MAX_RETRIES; remaining_tries > 0 && !successful; --remaining_tries)
    {
#ifdef DEBUG
        printf("Attempting FIN, remaining: %d\n", remaining_tries);
#endif
        // Send the FIN message to the receiver.
        if (sendto(this->sock, &close_message, sizeof(close_message), 0, &this->peer_address, this->peer_address_size) < 0)
        {
            perror("Error sending close message at close_sender");
            continue;
        }

        rudp_header ack_message = {0};
        // Wait to receive the ACK message from the receiver.
        if (recv(this->sock, &ack_message, sizeof(ack_message), 0) < 0)
        {
            perror("Error receiving FIN-ACK at close_sender");
            continue;
        }

        if (!validate_checksum(&ack_message))
        {
            fprintf(stderr, "Error in FIN-ACK checksum at close_sender\n");
            continue;
        }
        // Validate that the message is a FIN ACK message.
        if (!(ack_message.flags & (FIN | ACK)))
        {
            fprintf(stderr, "Error in FIN-ACK flags at close_sender\n");
            continue;
        }

        successful = TRUE; // If we made it here we were successful
    }

    if (remaining_tries == 0)   // If the maximum number of retries is reached, the connection was not closed and an error will be printed.
    {
        fprintf(stderr, "Ran out of retries to send FIN\n");
    }

    close(this->sock);
    free(this);
}

/*
    * Closes the RUDP receiver.
*/
void rudp_close_receiver(rudp_receiver *this)
{
    close(this->sock);
    free(this);
}
/*
    * Sends one segment of data to the receiver.
    * Can send up to MAX_SEGMENT_SIZE bytes at a time.
    * Is used in rudp_send to divide a message into smaller segments.
    * Returns the number of bytes sent.
*/
int rudp_send_segment(rudp_sender *this, void *data, size_t size, unsigned short segment_num, int more, char *message_buffer)
{
    memcpy(message_buffer + sizeof(rudp_header), data, size);  // Copy the data to the message.
    rudp_header *header = (rudp_header *)message_buffer;       // Initialize the header of the message.
    memset(header, 0, sizeof(rudp_header));             // Initialize the header to 0.
    header->len = size;                                 // Set the length of the message to the size of the data.
    header->flags = (more ? MOR : 0);                   // Set the MOR flag to 1 if there is more data to send.
    header->segment_num = segment_num;                  // Set the header's segment number to the given segment number.
    set_checksum(message_buffer);                              // Set the checksum of the message.

    size_t message_size = size + sizeof(rudp_header);

    unsigned int remaining_tries;
    int successful = FALSE;
    for (remaining_tries = MAX_RETRIES; remaining_tries > 0 && !successful; --remaining_tries)
    {
#ifdef DEBUG
        printf("Attempting send, remaining: %d\n", remaining_tries);
#endif
        // Send the message to the receiver.
        int sent = sendto(this->sock, message_buffer, message_size, 0, &this->peer_address, this->peer_address_size);
        if (sent < 0)
        {
#ifdef DEBUG
            fprintf(stderr, "message_size = %zu\n", message_size);
#endif
            perror("Error sending message at send_segment");
            continue;
        }
        if ((size_t) sent != message_size)
        {
            fprintf(stderr, "Didn't send whole message at send_segment\n");
            continue;
        }
        rudp_header ack_message;    // Initialize the ack_message to store the received ACK message.
        if (recv(this->sock, &ack_message, sizeof(ack_message), 0) < 0)
        {
            perror("Error receiving ACK at send_segment");
            continue;
        }
        if (!validate_checksum(&ack_message))
        {
            fprintf(stderr, "Error in ACK checksum at send_segment\n");
            continue;
        }

        if (!(ack_message.flags & ACK)) // Validate that the message is an ACK message.
        
        {
            fprintf(stderr, "Error in ACK message flags at send_segment\n");
            continue;
        }

        if (ack_message.segment_num != segment_num) // Validate that the segment number of the ACK message is the same as the segment number of the sent message.
        {
            fprintf(stderr, "Error: received ACK for different segment");
            continue;
        }

        // if we made it here we were successful
        successful = TRUE;
    }
    if (remaining_tries == 0)   // If the maximum number of retries is reached, the message was not sent and an error will be printed.
        return -1;
    return size;    // Return the size of the message in bytes.
}

int rudp_send(rudp_sender *this, void *data, size_t size)
{
    char *message_buffer = malloc(sizeof(rudp_header) + size); // Preallocate memory for the messages that will be sent (+space for header), to prevent allocating on every segment
    char *data_bytes = (char *)data;    // Convert the pointer of the data to char*.
    size_t total_sent = 0;
    unsigned short segment_num = 0;
    while (size > total_sent)   // Loop until the entire message is sent.
    {
        size_t segment_size = size - total_sent;    // Set the segment size to the remaining size of the message.
        int more = FALSE;
        // If the segment size is greater than the maximum segment size, set the segment size to the maximum segment size and set the more flag to 1.
        // This is done to divide the message into smaller segments.
        if (segment_size > MAX_SEGMENT_SIZE)        
        {
            segment_size = MAX_SEGMENT_SIZE;
            more = TRUE;
        }
        // Send the segment to the receiver.
        int bytes_sent = rudp_send_segment(this, data_bytes + total_sent, segment_size, segment_num, more, message_buffer);
        // If the segment was not sent, return -1.
        if (bytes_sent < 0)
        {
            free(message_buffer);
            return -1;  // Error, send_segment will print the details if the error.
        }

        total_sent += bytes_sent;   // Add the number of bytes sent to the total number of bytes sent.
        segment_num += 1;           // Increment the segment number.
    }

    free(message_buffer);
    return total_sent;              // Return the total number of bytes sent.
}
/*
    * Waits for a message from the sender.
    * Returns the number of bytes received.
    * Returns -1 on close message received.
    * Returns -2 on error.
*/
int rudp_recv(rudp_receiver *this, void *buffer, size_t size)
{
    // Set no timeout for first message
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
    if (setsockopt(this->sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0)
    {
        perror("Error setting timeout at open_receiver");
        close(this->sock);
        free(this);
        return -2;
    }

    char *buffer_bytes = (char *)buffer;    // Convert the pointer of the buffer to char*.
    char *segment_buffer = malloc(sizeof(rudp_header) + MAX_SEGMENT_SIZE);  // Allocate memory for the buffer which would include the segmet received.
    int more = TRUE;
    size_t total_received = 0;
    unsigned short expected_segment_num = 0;
    while (more)
    {
        // Receive the segment from the sender.
        int received = recv(this->sock, segment_buffer, sizeof(rudp_header) + MAX_SEGMENT_SIZE, 0);

        if (expected_segment_num == 0) // Only after first message
        {
            // Set timeout for subsequent messages
            timeout.tv_sec = RECV_TIMEOUT_S;
            timeout.tv_usec = RECV_TIMEOUT_US;
            if (setsockopt(this->sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0)
            {
                perror("Error setting timeout at open_receiver");
                free(segment_buffer);
                return -2;
            }
        }

#ifdef DEBUG
        printf("received = %d\n", received);
#endif

        if (received < 0)   // An error occurred while receiving the message.
        {
            perror("Error receiving at recv");
            free(segment_buffer);
            return -2;
        }

        if ((unsigned int)received < sizeof(rudp_header))   // If the received message is less than the size of the header, An error occurred.
        {
            fprintf(stderr, "Error at recv: Received too few bytes\n");
            free(segment_buffer);
            return -2;
        }

        if (!validate_checksum(segment_buffer)) // Validate the checksum of the received message and return an error if the checksum is invalid.
        {
            fprintf(stderr, "Error in message checksum at recv\n");
            free(segment_buffer);
            return -2;
        }

        // Initialize the header of the message. It will point to the header of the segment buffer because the header is the first part of the segment buffer.
        rudp_header *header = (rudp_header *)segment_buffer;    
        more = header->flags & MOR;     // Set the header more value to true.

        rudp_header ack_message = {0};  //Initialize an ACK message to send to the sender (only the header matters so we initialize the message to 0)
        ack_message.len = 0;
        ack_message.flags = ACK;        // Set the ACK flag to 1 for the ACK message.
        ack_message.segment_num = header->segment_num;  // Set the segment number of the ACK message to the segment number of the received message. Such that each ACK corresponds to a segment.
        if (header->flags & FIN)        // If the received message is a FIN message, set the FIN flag of the ACK message to 1.
        {
#ifdef DEBUG
            printf("Received FIN message!\n");
#endif
            ack_message.flags |= FIN;   // Set the FIN flag of the ACK message to 1 (set the flag to the OR result with FIN).
        }
        if (header->flags & SYN)        // If the received message is a SYN message, set the SYN flag of the ACK message to 1.
        {
#ifdef DEBUG
            printf("Received extra SYN message!\n");
#endif
            ack_message.flags |= SYN;   
        }
        set_checksum(&ack_message);
        unsigned int remaining_tries;
        int successful = FALSE;
        for (remaining_tries = MAX_RETRIES; remaining_tries > 0 && !successful; --remaining_tries)  // Loop until the maximum number of retries is reached or the message is sent.
        {
            // Send the ACK/FIN/SYN message to the sender.
            if (sendto(this->sock, &ack_message, sizeof(ack_message), 0, &this->peer_address, this->peer_address_size) < 0)
            {
                perror("Error sending ACK message at recv");
                continue;
            }
            successful = TRUE; // If we made it here we were successful
#ifdef DEBUG
            printf("sent ack for segment number %hu\n", expected_segment_num);
#endif
        }

        if (header->flags & FIN)    // After sending the FIN message, close the receiver and return -1 (which represent end of connection).
        {
            free(segment_buffer);
            return -1;
        }

        if (total_received > size)  // If the total number of bytes received is greater than the size of the buffer, print an error and exit.
        {
            free(segment_buffer);
            fprintf(stderr, "Fatal error: total_received > size: %zu > %zu", total_received, size);
            exit(1);
        }
        if (expected_segment_num == header->segment_num) // Otherwise duplicate message
        {
#ifdef DEBUG
            printf("Got valid segment!\n");
#endif
            // Copy the received message to the buffer in the (+total_received) position.
            memcpy(buffer_bytes + total_received, segment_buffer + sizeof(rudp_header), received - sizeof(rudp_header));
            total_received += received - sizeof(rudp_header);   // Increment the total number of bytes received by the size of the received message.
            expected_segment_num += 1;      // Increment the expected segment number by 1.
        }
    }

    free(segment_buffer);   // After finishing the loop, free the segment buffer.

    return total_received;  // Return the total number of bytes received.
}