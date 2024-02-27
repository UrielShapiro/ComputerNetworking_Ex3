#ifndef _RUDP_H_
#define _RUDP_H_

#include <stddef.h>
#include <sys/socket.h> // For the socket function
#include <arpa/inet.h> // For the in_addr structure and the inet_pton function

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

rudp_sender *rudp_open_sender(char *address, unsigned short port);
rudp_receiver *rudp_open_receiver(unsigned short port);

void rudp_close_sender(rudp_sender *sender);
void rudp_close_receiver(rudp_receiver *receiver);

int rudp_send(rudp_sender *sender, void *data, size_t size);

int rudp_recv(rudp_receiver *receiver, void *buffer, size_t size);

enum rudp_header_flags {
    SYN = 1<<0,
    ACK = 1<<1,
    FIN = 1<<2,
};

typedef struct {
    unsigned short len;     // size not including header
    char flags;
    unsigned short checksum;
} rudp_header;

#endif // _RUDP_H_