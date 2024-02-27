#include "rudp.h"
#include <sys/socket.h> // For the socket function
#include <arpa/inet.h> // For the in_addr structure and the inet_pton function

#define SYN_MESSAGE_SIZE 1024

rudp_sender *rudp_open_sender(char *address, unsigned short port)
{
    rudp_sender *this = malloc(sizeof(rudp_sender));

    this->sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (this->sock == -1)
    {
        perror("Error on socket creation at sender");
        free(this);
        return NULL;
    }

    struct sockaddr_in receiver_address;
    memset(&receiver_address, 0, sizeof(receiver_address));

    if (inet_pton(AF_INET, address, &receiver_address.sin_addr) <= 0)
    {
        perror("Error on address conversion at sender");
        close(this->sock);
        free(this);
        return NULL;
    }
    receiver_address.sin_family = AF_INET;
    receiver_address.sin_port = htons(port);
    // TODO: socket options? timeout
    char *syn_message = "hello"; //TODO: change

    sendto(this->sock, syn_message, strlen(syn_message), 0, &this->this, sizeof(this->this));

    return this;
}

rudp_receiver *rudp_open_receiver(unsigned short port)
{
    rudp_receiver *this = malloc(sizeof(rudp_receiver));

    this->sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (this->sock == -1)
    {
        perror("Error on socket creation at receiver");
        free(this);
        return NULL;
    }

    int opt = 1;
    if (setsockopt(this->sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        perror("Error while setting reuse address option");
        close(this->sock);
        free(this);
        return NULL;
    }

    struct sockaddr_in my_address;
    memset(&my_address, 0, sizeof(my_address));
    my_address.sin_addr.s_addr = INADDR_ANY;
    my_address.sin_family = AF_INET;
    my_address.sin_port = htons(port);

    if (bind(this->sock, &my_address, sizeof(my_address)) < 0)
    {
        perror("Error while binding socket");
        close(this->sock);
        free(this);
        return NULL;
    }

    char buffer[SYN_MESSAGE_SIZE];

    int syn_message_size = recvfrom(this->sock, buffer, SYN_MESSAGE_SIZE, 0, &this->sender_address, &this->sender_address_size);
    if (syn_message_size < 0)
    {
        perror("Error trying to receive SYN message from sender at receiver");
        close(this->sock);
        free(this)
        return NULL;
    }
}