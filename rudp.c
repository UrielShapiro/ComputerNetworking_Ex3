#include "rudp.h"
#include <sys/socket.h>

typedef struct {
    int sock;
    struct sockaddr receiver;
} rudp_sender;