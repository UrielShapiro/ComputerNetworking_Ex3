#include <stdio.h>
#include "rudp.h"

int main(void)
{
    rudp_receiver *receiver = rudp_open_receiver(1025);
    if (!receiver)
        return 1;
    printf("Created receiver!\n");
    char buffer[1024];
    printf("Attempting to receive message...\n");
    int recv_result;
    do
    {
        recv_result = rudp_recv(receiver, buffer, sizeof(buffer));
    } while (recv_result != -2 && recv_result != -1);
    if (recv_result == -1)
    {
        printf("Received: \"%s\"\n", buffer);
        rudp_close_receiver(receiver);
        printf("Sender closed the connection\n");
    }
    else
    {
        printf("Errored out\n");
    }
    return 0;
}