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
 * @brief Opens an RUDP receiver and waits for a connection from a sender.
 * Listens on the given port. and accepts the first connection.
 * Make sure to call rudp_close_receiver on the result to close and free
 * @return a pointer to the receiver or NULL on error
 * @note The default maximum number of clients is 1.
*/
rudp_receiver *rudp_open_receiver(unsigned short port);

/**
 * @brief Opens an RUDP sender and connects to a receiver in the given address and port
 * Make sure to call rudp_close_sender on the result to close and free
 * @return a pointer to the sender or NULL on error
*/
rudp_sender *rudp_open_sender(char *address, unsigned short port);

/**
*/


/**
 * @brief Closes the RUDP sender, Send a FIN to the associated receiver and closes the connection
*/
void rudp_close_sender(rudp_sender *sender);

/**
 * @brief Closes the RUDP receiver, Sends a FIN to the associated sender and closes the connection
*/
void rudp_close_receiver(rudp_receiver *receiver);

/**
 * @brief Sends "size" bytes stored at "data" through the sender ("this")
 * @return the number of bytes sent, 
 * @return -1 on error
*/
int rudp_send(rudp_sender *sender, void *data, size_t size);

/**
 * @brief Waits for a message from the sender, writes the message up to "size" bytes into "buffer"
 * @return the number of bytes received, 
 * @return -1 on close message received, 
 * @return -2 on error
*/
int rudp_recv(rudp_receiver *receiver, void *buffer, size_t size);

// ENUM for bit masks in the flag field of rudp_header
enum rudp_header_flags {
    SYN = 1<<0,
    ACK = 1<<1,
    FIN = 1<<2,
    MOR = 1<<3,
};
// The struct of the header of the RUDP protocol
typedef struct {
    unsigned short len;     // size not including header
    char flags;
    unsigned short checksum;
    unsigned short segment_num;
} rudp_header;

#endif // _RUDP_H_