#include <stdio.h>
#include "rudp.h"

int main(void)
{
    rudp_receiver *receiver = rudp_open_receiver(1025);
    if (!receiver) return 1;
    printf("Created receiver!\n");
    char buffer[1024];
    printf("Attempting to receive message...\n");
    rudp_recv(receiver, buffer, sizeof(buffer));
    printf("Received: \"%s\"\n", buffer);
}