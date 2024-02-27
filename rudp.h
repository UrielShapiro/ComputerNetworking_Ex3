#ifndef _RUDP_H_
#define _RUDP_H_

#include <stddef.h>

typedef struct {
    int sock;
    struct sockaddr receiver;
} rudp_sender;

typedef struct {
    int sock;
    struct sockaddr sender_address;
    socklen_t sender_address_size;
} rudp_receiver;

rudp_sender *rudp_open_sender(char *address, short port);
rudp_receiver *rudp_open_receiver(short port);

void rudp_close_sender(rudp_sender *sender);
void rudp_close_receiver(rudp_receiver *receiver);

void rudp_send(rudp_sender *sender, void *data, size_t size);

void *rudp_recv(rudp_receiver *receiver, size_t *size);

#endif // _RUDP_H_