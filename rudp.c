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
    // for (size_t i = 0; i < bytes; ++i)
    //     printf("%02x", ((char *)data)[i]);
    printf("\n");
#endif
    /* Compute Internet Checksum for "len" bytes
     *         beginning at location "data".
     */

    unsigned short *data_words = (unsigned short *)data;
    unsigned int sum = 0;

    while (bytes > 1)
    {
        /*  This is the inner loop */
        sum += *(unsigned short *)data_words++;
        bytes -= 2;
    }

    /*  Add left-over byte, if any */
    if (bytes > 0)
        sum += *(unsigned char *)data_words;

    /*  Fold 32-bit sum to 16 bits */
    while (sum >> 16)
        sum = (sum & 0xffff) + (sum >> 16);
#ifdef CHECKSUM_DEBUG
    printf("Calculated checksum is %hu\n", ~(unsigned short)sum);
#endif
    return ~(unsigned short)sum;
}

/**
 * Sets the checksum of a message
 */
void set_checksum(void *rudp_message)
{
#ifdef CHECKSUM_DEBUG
    printf("Setting");
#endif
    rudp_header *header = (rudp_header *)rudp_message;
    header->checksum = 0;
    header->checksum = calculate_checksum(rudp_message, sizeof(rudp_header) + header->len);
}

/**
 * Validates the checksum of a message
 */
int validate_checksum(void *rudp_message)
{
#ifdef CHECKSUM_DEBUG
    printf("Validating");
#endif
    return 0 == calculate_checksum(rudp_message, sizeof(rudp_header) + ((rudp_header *)rudp_message)->len);
}

rudp_sender *rudp_open_sender(char *address, unsigned short port)
{
    rudp_sender *this = malloc(sizeof(rudp_sender));

    this->sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (this->sock == -1)
    {
        perror("Error on socket creation at open_sender");
        free(this);
        return NULL;
    }

    struct sockaddr_in receiver_address = {0};

    memset(&receiver_address, 0, sizeof(receiver_address));

    if (inet_pton(AF_INET, address, &receiver_address.sin_addr) <= 0)
    {
        perror("Error on address conversion at open_sender");
        close(this->sock);
        free(this);
        return NULL;
    }
    receiver_address.sin_family = AF_INET;
    receiver_address.sin_port = htons(port);

    this->peer_address = *(struct sockaddr *)&receiver_address;

    this->peer_address_size = sizeof(receiver_address);

    struct timeval timeout;
    timeout.tv_sec = ACK_TIMEOUT_S;
    timeout.tv_usec = ACK_TIMEOUT_US;
    if (setsockopt(this->sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0)
    {
        perror("Error setting timeout at open_sender");
        close(this->sock);
        free(this);
        return NULL;
    }

    rudp_header syn_message = {0}; // we will send only the header
    syn_message.len = 0;
    syn_message.flags = SYN;
    set_checksum(&syn_message);

    unsigned int remaining_tries = MAX_RETRIES;
    int successful = 0;
    for (remaining_tries = MAX_RETRIES; remaining_tries > 0 && !successful; --remaining_tries)
    {
#ifdef DEBUG
        printf("Attempting SYN, remaining: %d\n", remaining_tries);
#endif
        if (sendto(this->sock, &syn_message, sizeof(syn_message), 0, &this->peer_address, this->peer_address_size) <= 0)
        {
            perror("Error sending SYN message at open_sender");
            close(this->sock);
            free(this);
            return NULL;
        }

        rudp_header ack_message = {0};
        if (recv(this->sock, &ack_message, sizeof(ack_message), 0) < 0)
        {
            perror("Error receiving ACK at open_sender");
            continue;
        }
        if (!validate_checksum(&ack_message))
        {
            fprintf(stderr, "Error on ACK message at open_sender\n");
            continue;
        }
        // if we made it here we were successful
        successful = 1;
    }

    if (remaining_tries == 0)
    {
        fprintf(stderr, "Ran out of retries to send SYN at open_sender\n");
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
        perror("Error on socket creation at open_receiver");
        free(this);
        return NULL;
    }

    int reuse = 1;
    if (setsockopt(this->sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0)
    {
        perror("Error while setting reuse address option at open_receiver");
        close(this->sock);
        free(this);
        return NULL;
    }

    struct sockaddr_in my_address = {0};
    my_address.sin_addr.s_addr = INADDR_ANY;
    my_address.sin_family = AF_INET;
    my_address.sin_port = htons(port);

    if (bind(this->sock, (struct sockaddr *)&my_address, sizeof(my_address)) < 0)
    {
        perror("Error while binding socket at open_receiver");
        close(this->sock);
        free(this);
        return NULL;
    }

    this->peer_address_size = sizeof(this->peer_address);

    rudp_header syn_message = {0};

    if (recvfrom(this->sock, &syn_message, sizeof(syn_message), 0, &this->peer_address, &this->peer_address_size) < 0)
    {
        perror("Error trying to receive SYN message from sender at open_receiver");
        close(this->sock);
        free(this);
        return NULL;
    }
    if (!validate_checksum(&syn_message))
    {
        fprintf(stderr, "Error in SYN message checksum at open_receiver\n");
        close(this->sock);
        free(this);
        return NULL;
    }
    if (!(syn_message.flags & SYN))
    {
        fprintf(stderr, "Error in SYN message flags at open_receiver\n");
        close(this->sock);
        free(this);
        return NULL;
    }

    rudp_header ack_message = {0}; // we will send only the header
    ack_message.len = 0;
    ack_message.flags = ACK | SYN;
    set_checksum(&ack_message);

    if (sendto(this->sock, &ack_message, sizeof(ack_message), 0, &this->peer_address, this->peer_address_size) <= 0)
    {
        perror("Error sending SYN-ACK message at open_receiver");
        close(this->sock);
        free(this);
        return NULL;
    }

    return this;
}

void rudp_close_sender(rudp_sender *this)
{
    rudp_header close_message = {0};
    close_message.len = 0;
    close_message.flags = FIN;
    set_checksum(&close_message);
    unsigned int remaining_tries;
    int successful = 0;
    for (remaining_tries = MAX_RETRIES; remaining_tries > 0 && !successful; --remaining_tries)
    {
#ifdef DEBUG
        printf("Attempting FIN, remaining: %d\n", remaining_tries);
#endif
        if (sendto(this->sock, &close_message, sizeof(close_message), 0, &this->peer_address, this->peer_address_size) < 0)
        {
            perror("Error sending close message at close_sender");
            continue;
        }

        rudp_header ack_message = {0};
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

        if (!(ack_message.flags & (FIN | ACK)))
        {
            fprintf(stderr, "Error in FIN-ACK flags at close_sender\n");
            continue;
        }

        successful = 1;
    }

    if (remaining_tries == 0)
    {
        fprintf(stderr, "Ran out of retries to send FIN\n");
    }

    close(this->sock);
    free(this);
}

void rudp_close_receiver(rudp_receiver *this)
{
    close(this->sock);
    free(this);
}

int rudp_send_segment(rudp_sender *this, void *data, size_t size, unsigned short segment_num, int more)
{
    char *message = malloc(sizeof(rudp_header) + size);
    memcpy(message + sizeof(rudp_header), data, size);
    rudp_header *header = (rudp_header *)message;
    memset(header, 0, sizeof(rudp_header));
    header->len = size;
    header->flags = (more ? MOR : 0);
    header->segment_num = segment_num;
    set_checksum(message);

    size_t message_size = size + sizeof(rudp_header);

    unsigned int remaining_tries;
    int successful = 0;
    for (remaining_tries = MAX_RETRIES; remaining_tries > 0 && !successful; --remaining_tries)
    {
#ifdef DEBUG
        printf("Attempting send, remaining: %d\n", remaining_tries);
#endif
        int sent = sendto(this->sock, message, message_size, 0, &this->peer_address, this->peer_address_size);
        if (sent < 0)
        {
#ifdef DEBUG
            fprintf(stderr, "message_size = %zu\n", message_size);
#endif
            perror("Error sending message at send_segment");
            continue;
        }
        rudp_header ack_message;
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

        if (!(ack_message.flags & ACK))
        {
            fprintf(stderr, "Error in ACK message flags at send_segment\n");
            continue;
        }

        if (ack_message.segment_num != segment_num)
        {
            fprintf(stderr, "Error: received ACK for different segment");
            continue;
        }

        // if we made it here we were successful
        successful = 1;
    }
    free(message);
    if (remaining_tries == 0)
        return -1;
    return size;
}

int rudp_send(rudp_sender *this, void *data, size_t size)
{
    char *data_bytes = (char *)data;
    size_t total_sent = 0;
    unsigned short segment_num = 0;
    while (size > total_sent)
    {
        size_t segment_size = size - total_sent;
        int more = 0;
        if (segment_size > MAX_SEGMENT_SIZE)
        {
            segment_size = MAX_SEGMENT_SIZE;
            more = 1;
        }

        int bytes_sent = rudp_send_segment(this, data_bytes + total_sent, segment_size, segment_num, more);

        if (bytes_sent < 0)
            return -1;

        total_sent += bytes_sent;
        segment_num += 1;
    }

    return total_sent;
}

int rudp_recv(rudp_receiver *this, void *buffer, size_t size)
{
    // set no timeout for first message
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

    char *buffer_bytes = (char *)buffer;
    char *segment_buffer = malloc(sizeof(rudp_header) + MAX_SEGMENT_SIZE);
    int more = 1;
    size_t total_received = 0;
    unsigned short expected_segment_num = 0;
    while (more)
    {
        int received = recv(this->sock, segment_buffer, sizeof(rudp_header) + MAX_SEGMENT_SIZE, 0);

        if (expected_segment_num == 0) // only after first message
        {
            // set timeout for subsequent messages
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

        if (received < 0)
        {
            perror("Error receiving at recv");
            free(segment_buffer);
            return -2;
        }

        if ((unsigned int)received < sizeof(rudp_header))
        {
            fprintf(stderr, "Error at recv: Received too few bytes\n");
            free(segment_buffer);
            return -2;
        }

        if (!validate_checksum(segment_buffer))
        {
            fprintf(stderr, "Error in message checksum at recv\n");
            free(segment_buffer);
            return -2;
        }

        rudp_header *header = (rudp_header *)segment_buffer;
        more = header->flags & MOR;

        rudp_header ack_message = {0};
        ack_message.len = 0;
        ack_message.flags = ACK;
        ack_message.segment_num = header->segment_num;
        if (header->flags & FIN)
        {
#ifdef DEBUG
            printf("Received FIN message!\n");
#endif
            ack_message.flags |= FIN;
        }
        if (header->flags & SYN)
        {
#ifdef DEBUG
            printf("Received extra SYN message!\n");
#endif
            ack_message.flags |= SYN;
        }
        set_checksum(&ack_message);
        unsigned int remaining_tries;
        int successful = 0;
        for (remaining_tries = MAX_RETRIES; remaining_tries > 0 && !successful; --remaining_tries)
        {
            if (sendto(this->sock, &ack_message, sizeof(ack_message), 0, &this->peer_address, this->peer_address_size) < 0)
            {
                perror("Error sending ACK message at recv");
                continue;
            }
            successful = 1;
#ifdef DEBUG
            printf("sent ack for segment number %hu\n", expected_segment_num);
#endif
        }

        if (header->flags & FIN)
        {
            free(segment_buffer);
            return -1;
        }

        if (total_received > size)
        {
            free(segment_buffer);
            fprintf(stderr, "Fatal error: total_received > size: %zu > %zu", total_received, size);
            exit(1);
        }
        if (expected_segment_num == header->segment_num) // otherwise duplicate message
        {
#ifdef DEBUG
            printf("Got valid segment!\n");
#endif
            memcpy(buffer_bytes + total_received, segment_buffer + sizeof(rudp_header), received - sizeof(rudp_header));
            total_received += received - sizeof(rudp_header);
            expected_segment_num += 1;
        }
    }

    free(segment_buffer);

    return total_received;
}