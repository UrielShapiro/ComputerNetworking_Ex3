#include <stdio.h>
#include <string.h>
#include "rudp.h"

int main(void)
{
    rudp_sender *sender = rudp_open_sender("127.0.0.1", 1025);
    if (!sender)
        return 1;
    printf("Created sender!\n");
    char message[1024];
    printf("Starting sending loop, send \"close\" to finish.\n");
    do
    {
        printf("Enter message: ");
        fgets(message, 1024, stdin);
        message[strcspn(message, "\n")] = '\0';
        printf("Attempting to send message: \"%s\"...\n", message);
        if (rudp_send(sender, message, strlen(message) + 1) < 0)
        {
            printf("rudp_send failed!\n");
        }
        else
        {
            printf("Sent message.\n");
        }
    } while (strcmp(message, "close") != 0);
    rudp_close_sender(sender);
    printf("Closed correctly\n");
    return 0;
}