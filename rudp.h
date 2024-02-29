#ifndef _RUDP_H_
#define _RUDP_H_

#include <arpa/inet.h>  // For the in_addr structure and the inet_pton function

typedef struct {
    int sock;
    struct sockaddr peer_address;
    socklen_t peer_address_size;
} rudp_sender;

typedef struct {
    int sock;
    struct sockaddr peer_address;
    socklen_t peer_address_size;
} rudp_receiver;

/**
 * Opens an RUDP receiver and waits for a connection from a sender
 * Returns a pointer to the receiver, make sure to call rudp_close_receiver when done with it
 * Returns NULL on error
*/
rudp_receiver *rudp_open_receiver(unsigned short port);

/**
 * Opens an RUDP sender and connects to a receiver in the given address and port
 * Returns a pointer to the sender, make sure to call rudp_close_sender when done with it
 * Returns NULL on error, including no receiver accepting the connection
*/
rudp_sender *rudp_open_sender(char *address, unsigned short port);

/**
 * Send a FIN to the associated receiver and closes the connection
*/
void rudp_close_sender(rudp_sender *sender);

/**
 * Closes the RUDP receiver, any subsequent messages sent to it are not accepted
*/
void rudp_close_receiver(rudp_receiver *receiver);

/**
 * Sends <size> bytes stroed at <data> through the sender
 * Returns the number of bytes sent
 * Returns -1 on error
*/
int rudp_send(rudp_sender *sender, void *data, size_t size);

/**
 * Waits for a message from the sender, writes the message up to <size> bytes into <buffer>
 * Returns the number of bytes received
 * Returns -1 on close message received, if this happens you should call rudp_close_receiver
 * Returns -2 on error
*/
int rudp_recv(rudp_receiver *receiver, void *buffer, size_t size);

enum rudp_header_flags {
    SYN = 1<<0,
    ACK = 1<<1,
    FIN = 1<<2,
    MOR = 1<<3,
};

typedef struct {
    unsigned short len;     // size not including header
    char flags;
    unsigned short checksum;
    unsigned short segment_num;
} rudp_header;

#endif // _RUDP_H_