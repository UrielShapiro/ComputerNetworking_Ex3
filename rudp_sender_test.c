#include <stdio.h>
#include <string.h>
#include "rudp.h"

int main(void)
{
    rudp_sender *sender = rudp_open_sender("127.0.0.1", 1025);
    if (!sender) return 1;
    printf("Created sender!\n");
    char *message = "Hello through RUDP!";
    printf("Attempting to send message: \"%s\"...\n", message);
    rudp_send(sender, message, strlen(message));
    printf("Sent message.\n");
    return 0;
}