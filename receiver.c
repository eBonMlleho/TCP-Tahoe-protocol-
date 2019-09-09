#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "ccitt16.h"

int socketConnect(int sock, char server_IP[])
{
    int port = 7495;
    struct sockaddr_in remote;

    // Setup remote connection
    remote.sin_addr.s_addr = inet_addr(server_IP); 
    remote.sin_family = PF_INET;
    remote.sin_port = htons(port);

    // Connection
    return connect(sock, (struct sockaddr *)&remote, sizeof(remote));
}

int main(int argc, char *argv[])
{
    int sock;
    unsigned char packet[6] = {0};
    char serverIP[100] = {0};
    unsigned char ack[2] = {0};
    int received;
    int sn, highestSn = -1, expected = -1;

    if(argc != 2)
    {
        fprintf(stderr, "Need server IP address!\n");
        return EXIT_FAILURE;
    }
    else
    {
        strcpy(serverIP, argv[1]);
    }

    // Creating the socket
    if((sock = socket(PF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("Failed to create Socket!");
        return EXIT_FAILURE;
    }
    else
    {
         fprintf(stdout, "Sucessfully created the socket!\n");

    }

    // Connection to remote server
    if(socketConnect(sock, serverIP) < 0)
    {
        perror("Remote connection failed!");
        return EXIT_FAILURE;
    }
    else
    {
        fprintf(stdout, "Sucessfully connected to the server!\n");

    }
    //Listen for data
    while(1)
    {
        received = recv(sock, packet, 6 , 0);

         if((packet[1] | packet[0] << 8) <= 0)
            printf("receiving the ending packet!\n");
        else
            printf("Receiving %d: %d %d %d %d ('%c' '%c')\n", (packet[1] | packet[0] << 8), packet[0], packet[1], packet[2], packet[3], packet[2], packet[3]);

        if(received==0)
        {
            break;
        }
        else if(received!= 6)
        {
            perror("DID not receive data!");
            return EXIT_FAILURE;
        }
        else
        {
            if(calculate_CCITT16(packet, 6, CHECK_CRC) == CRC_CHECK_SUCCESSFUL)
            {
                //Using sleep to wait for sometime to send ack back to sender
                sleep(1);

                sn = (packet[1] | packet[0] << 8);

                if(sn > highestSn)
                    highestSn = sn;

                // Sequence number test

                // if packet is not recieved
                if(sn <= 0){
                    ack[0] = 0;
                    ack[1] = 0;
                // if the packet recieved has error
                }
                else if(sn == expected || expected == -1)
                {
                    ack[0] = (highestSn + 1) >> 8;
                    ack[1] = (highestSn + 1) & 0xff;

                    expected = highestSn + 1;
                }// If the packet is as expected
                else
                {
                    ack[0] = expected >> 8;
                    ack[1] = expected & 0xff;
                }

                printf("ACK: %d\n", (ack[1] | ack[0] << 8));
                // Sending back ACK to server
                if(send(sock, ack, 2, 0) < 0)
                {
                    perror("Sending ACK failure");
                    return EXIT_FAILURE;
                }

                if(sn <= 0)
                    break;
            }
        }
    }

    fprintf(stdout, "Sucessfully received data from server!\n");


    // Closing the socket
    if(close(sock) < 0)
    {
        perror("Failed Closing the socket!");
        return EXIT_FAILURE;
    }


    fprintf(stdout, "Sucessfully closed the socket!\n");

    return EXIT_SUCCESS;

}