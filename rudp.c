#include "rudp.h"
#include <sys/socket.h> // For the socket function
#include <arpa/inet.h>  // For the in_addr structure and the inet_pton function
#include <stdlib.h>     // For malloc and free
#include <stdio.h>      // For perror
#include <string.h>     // For memset
#include <unistd.h>     // For close

#define DEBUG 0

unsigned short calculate_checksum(void *data, unsigned int bytes)
{
    if (DEBUG) printf(" checksum of data with size %d\n", bytes);    //DEBUG
    if (DEBUG) printf("Data is: "); //DEBUG
    if (DEBUG) for (size_t i = 0; i < bytes; ++i) printf("%02x", ((char*)data)[i]);
    if (DEBUG) printf("\n");
    /* Compute Internet Checksum for "len" bytes
     *         beginning at location "data".
     */

    unsigned short *data_words = (unsigned short *)data;
    unsigned int sum = 0;

    while (bytes > 1)
    {
        /*  This is the inner loop */
        sum += *(unsigned short *) data_words++;
        bytes -= 2;
    }

    /*  Add left-over byte, if any */
    if (bytes > 0)
        sum += *(unsigned char *)data_words;

    /*  Fold 32-bit sum to 16 bits */
    while (sum >> 16)
        sum = (sum & 0xffff) + (sum >> 16);
    if (DEBUG) printf("Calculated checksum is %hu\n", ~(unsigned short) sum);   //DEBUG
    return ~(unsigned short)sum;
}

void set_checksum(void *rudp_message)
{
    if (DEBUG) printf("Setting");  //DEBUG
    rudp_header *header = (rudp_header *)rudp_message;
    header->checksum = 0;
    header->checksum = calculate_checksum(rudp_message, sizeof(rudp_header) + header->len);
}

int validate_checksum(void *rudp_message)
{
    if (DEBUG) printf("Validating");   //DEBUG
    return 0 == calculate_checksum(rudp_message, sizeof(rudp_header) + ((rudp_header *)rudp_message)->len);
}

rudp_sender *rudp_open_sender(char *address, unsigned short port)
{
    rudp_sender *this = malloc(sizeof(rudp_sender));

    this->sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (this->sock == -1)
    {
        perror("Error on socket creation at sender");
        free(this);
        return NULL;
    }

    struct sockaddr_in receiver_address;

    memset(&receiver_address, 0, sizeof(receiver_address));

    if (inet_pton(AF_INET, address, &receiver_address.sin_addr) <= 0)
    {
        perror("Error on address conversion at sender");
        close(this->sock);
        free(this);
        return NULL;
    }
    receiver_address.sin_family = AF_INET;
    receiver_address.sin_port = htons(port);

    this->peer_address = *(struct sockaddr *)&receiver_address;

    this->peer_address_size = sizeof(receiver_address);
    // TODO: socket options? timeout

    rudp_header syn_message; // we will send only the header
    syn_message.len = 0;
    syn_message.flags = SYN;
    set_checksum(&syn_message);

    if (sendto(this->sock, &syn_message, sizeof(syn_message), 0, &this->peer_address, this->peer_address_size) <= 0)
    {
        perror("Error sending SYN message at sender");
        close(this->sock);
        free(this);
        return NULL;
    }

    rudp_header ack_message;
    recv(this->sock, &ack_message, sizeof(ack_message), 0);
    if (!validate_checksum(&ack_message))
    {
        fprintf(stderr, "Error on ACK message at sender\n");
        close(this->sock);
        free(this);
        return NULL;
    }

    return this;
}

rudp_receiver *rudp_open_receiver(unsigned short port)
{
    rudp_receiver *this = malloc(sizeof(rudp_receiver));

    this->sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (this->sock == -1)
    {
        perror("Error on socket creation at receiver");
        free(this);
        return NULL;
    }

    int opt = 1;
    if (setsockopt(this->sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        perror("Error while setting reuse address option");
        close(this->sock);
        free(this);
        return NULL;
    }

    struct sockaddr_in my_address = { 0 };
    my_address.sin_addr.s_addr = INADDR_ANY;
    my_address.sin_family = AF_INET;
    my_address.sin_port = htons(port);

    if (bind(this->sock, (struct sockaddr *)&my_address, sizeof(my_address)) < 0)
    {
        perror("Error while binding socket");
        close(this->sock);
        free(this);
        return NULL;
    }

    this->peer_address_size = sizeof(this->peer_address);

    rudp_header syn_message;

    if (recvfrom(this->sock, &syn_message, sizeof(syn_message), 0, &this->peer_address, &this->peer_address_size) < 0)
    {
        perror("Error trying to receive SYN message from sender at receiver");
        close(this->sock);
        free(this);
        return NULL;
    }
    if (!validate_checksum(&syn_message))
    {
        fprintf(stderr, "Error in SYN message checksum\n");
        close(this->sock);
        free(this);
        return NULL;
    }

    rudp_header ack_message; // we will send only the header
    ack_message.len = 0;
    ack_message.flags = ACK;
    set_checksum(&ack_message);

    if (sendto(this->sock, &ack_message, sizeof(ack_message), 0, &this->peer_address, this->peer_address_size) <= 0)
    {
        perror("Error sending ACK message at receiver");
        close(this->sock);
        free(this);
        return NULL;
    }

    // TODO: verify syn message and extract info from it

    return this;
}

void rudp_close_sender(rudp_sender *this)
{
    rudp_header close_message;
    close_message.len = 0;
    close_message.flags = FIN;
    set_checksum(&close_message);
    if (sendto(this->sock, &close_message, sizeof(close_message), 0, &this->peer_address, this->peer_address_size) < 0)
    {
        perror("Error sending close message");
    }
    close(this->sock);
    free(this);
}

void rudp_close_receiver(rudp_receiver *this)
{
    char *close_message = "close";
    sendto(this->sock, close_message, strlen(close_message), 0, &this->peer_address, this->peer_address_size);
    close(this->sock);
    free(this);
}

int rudp_send(rudp_sender *this, void *data, size_t size)
{
    char *message = malloc(sizeof(rudp_header) + size);
    memcpy(message + sizeof(rudp_header), data, size);
    rudp_header *header = (rudp_header *)message;
    header->len = size; 
    header->flags = 0;
    set_checksum(message);
    size_t total_sent = 0;
    size_t message_size = size + sizeof(rudp_header);
    while (total_sent < message_size)
    {
        int sent = sendto(this->sock, message + total_sent, message_size - total_sent, 0, &this->peer_address, this->peer_address_size);
        if (sent < 0)
        {
            perror("Error sending message");
            return 0;
        }
        total_sent += sent;
    }
    free(message);
    rudp_header ack_message;
    recv(this->sock, &ack_message, sizeof(ack_message), 0);
    if (!validate_checksum(&ack_message))
    {
        fprintf(stderr, "Error in ACK checksum\n");
    }
    return total_sent;
}

int rudp_recv(rudp_receiver *this, void *buffer, size_t size)
{
    char *message_buffer = malloc(sizeof(rudp_header) + size);
    int received = recv(this->sock, message_buffer, size, 0);
    if (received < 0)
    {
        perror("Error receiving");
        return -1;
    }
    if (!validate_checksum(message_buffer))
    {
        fprintf(stderr, "Error in message checksum\n");
    }
    memcpy(buffer, message_buffer + sizeof(rudp_header), size);
    free(message_buffer);
    rudp_header ack_message;
    ack_message.len = 0;
    ack_message.flags = ACK;
    set_checksum(&ack_message);
    if (sendto(this->sock, &ack_message, sizeof(ack_message), 0, &this->peer_address, this->peer_address_size) < 0)
    {
        perror("Error sending ack message");
    }
    return received - sizeof(rudp_header);
}