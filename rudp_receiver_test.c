#include <stdio.h>
#include "rudp.h"

int main(void)
{
    rudp_receiver *receiver = rudp_open_receiver(1025);
    if (!receiver)
        return 1;
    printf("Created receiver!\n");
    char buffer[1024] = { 0 };
    printf("Attempting to receive message...\n");
    int recv_result = 0;
    int recs_left = 10;
    do
    {
        recv_result = rudp_recv(receiver, buffer, sizeof(buffer));
        if (recv_result > 0) printf("Received: \"%s\"\n", buffer);
    } while (recv_result != -1 && recv_result != -2 && recs_left-->0);
    if (recv_result == -1)
    {
        printf("Sender closed the connection\n");
    }
    else
    {
        printf("Errored out\n");
    }
    rudp_close_receiver(receiver);
    return 0;
}