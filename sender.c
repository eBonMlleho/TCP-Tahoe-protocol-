#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/types.h>

#include "AddCongestion.h"
#include "ccitt16.h"

//Slowstart and Congestion avoidance
enum {SS, CA};
//Bind the socket 
int bindSocket(int sock)
{

    int clientPort = 7495;
    struct sockaddr_in  remote;

    remote.sin_family = PF_INET;
    remote.sin_addr.s_addr = htonl(INADDR_ANY);
    remote.sin_port = htons(clientPort);

    // Binding
    return bind(sock, (struct sockaddr *)&remote, sizeof(remote));
}
//Sending the packet to the reciever
void sendPacket(int clisock, int sequenceNum, unsigned char *packet, char *buffer, double BER, int *finish)
{
    int tmp;
    short int crc;

    packet[0] = sequenceNum >> 8;
    packet[1] = sequenceNum & 0xff;

    //data from file
    if(sequenceNum >= 1000)
    {
        tmp = (sequenceNum - 1000) * 2;
        packet[2] = buffer[tmp];
        packet[3] = buffer[tmp + 1];

        if(packet[2] == 0 || packet[3] == 0)
            *finish = 1;

        //crc
        crc = calculate_CCITT16(packet, 6-2, GENERATE_CRC);
        packet[4] = crc >> 8;
        packet[5] = crc & 0xff;

        //null terminated
        packet[6] = '\0';
        
        //adding congestion
        AddCongestion((char *)packet, BER);
    }

    printf("sending data:  %d: %d %d %d %d ('%c' '%c')\n", sequenceNum, packet[0], packet[1],
           packet[2], packet[3], packet[2], packet[3]);
	
    //send data
    if(send(clisock, packet, 6, 0) < 0)
    {
        perror("Sending packet failure");
        exit(EXIT_FAILURE);
    }
    else
    {
        fprintf(stdout,"Successfully sent the packet!\n");
    }
}
//Driver function
int main(int argc,char *argv[])
{
    int servsock,clisock;
    struct sockaddr_in client;
    socklen_t client_len;

    char ipAddress[100];
    char fileName[100];

    double BER;

    char buffer[1024];

    unsigned char packet[7];
    unsigned char ack[2];
    int acks[10000];

    int sequenceNum = 1000;
    int recieved;
    double timeOne = 0.0, timeTwo;
    int i;
    
    int count = 0;
    double cwnd = 1.0;
    int leftBoundCwnd = sequenceNum;
    double ssthresh = 16.0;
    int state = SS;
    int finish = 0;
    unsigned int new_ack = 0;

    srand(0);
    //check for all the arguments to start the program!
    if(argc == 4)
    {
        strcpy(ipAddress, argv[1]);
        strcpy(fileName, argv[2]);
        BER = atof(argv[3]);
    }
    else
    {
        fprintf(stderr, "Not all arguments are present!\n");
        return EXIT_FAILURE;
    }

    //Open a file

    FILE *file = fopen(fileName, "r");

    if(file == NULL)
    {
        fprintf(stderr, "Failed to open the input file!\n");
        return EXIT_FAILURE;
    }
    else
    {
        fprintf(stdout,"Successfully opened the file\n");
    }

    fgets(buffer, 1024, file);

    if(fclose(file) < 0)
    {
        perror("Closing the input file failed!\n");
        return EXIT_FAILURE;
    }
    else
    {
        fprintf(stdout, "Sucessfully closed the input file\n");

    }

     // Creating a socket
    if((servsock = socket(PF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("Socket creation failure");
        return EXIT_FAILURE;
    }
    else
    {
        fprintf(stdout, "Sucessfully created a socket!\n");
    }
    // Binding
    if(bindSocket(servsock) < 0)
    {
        perror("Socket binding failure");
        return EXIT_FAILURE;
    }
    else
    {
        fprintf(stdout, "Sucessfully binded the socket!\n");        
    }

    // Listening for connections
    if(listen(servsock, 10) < 0)
    {
        perror("Socket listening failure");
        return EXIT_FAILURE;
    }
    else
    {
        fprintf(stdout, "Sucessfully listening for connections!\n");
    }

    // connection from the receiver
    client_len = sizeof(client);
    if((clisock = accept(servsock, (struct sockaddr *)&client, &client_len)) < 0)
    {
        perror("Connection failure!");
        return EXIT_FAILURE;
    }
    else
    {
         fprintf(stdout, "Successfully connected to reciever!\n");
    }

    for(i = 0; i < 10000; i++)
        acks[i] = -1;

    int print = 0;

    //loop over the entire input string
    while(finish != 2) 
    {        
        if(finish)
            sequenceNum = 0;

        if(!print){
            printf("Sequence Number: %d, leftBoundCongestion: %d, cwnd: %d\n", sequenceNum, leftBoundCwnd,(int) cwnd);
            print = 1;
        }
        
        if(finish != 2 && acks[sequenceNum - 1000] < 0 && sequenceNum <= leftBoundCwnd + cwnd - 1)
        {
            timeOne = clock() * 1000 / CLOCKS_PER_SEC;
            sendPacket(clisock, sequenceNum, packet, buffer, BER, &finish);
            acks[sequenceNum - 1000] = 0;
            sequenceNum++;
        }

        recieved = recv(clisock, ack, 2, MSG_DONTWAIT);
        timeTwo = clock() * 1000 / CLOCKS_PER_SEC;

        if(recieved == 0) //error
        {
            finish = 2;
            break;
        }

        // If error is not detected in recieving the packet
        // acknowledgement recieved in packets of size 2 charachters
        else if(recieved == 2) //ack
        {
            leftBoundCwnd = sequenceNum;
            print = 0;
            
            timeTwo = 0;
            
            new_ack = (ack[1] | ack[0] << 8);
                
            printf("ACK: %d\n", new_ack);
            new_ack--;
                
            acks[new_ack - 1000]++;
            // if new acknowledgement is negative
            if(new_ack <= 0){
                finish = 2;
                break;
            }

            // if no dup ack not recieved i.e. acks is < 3
            if(acks[new_ack - 1000] < 3) 
            {   
                // go into slow start
                if(state == SS)
                {
                    // increment the c window by 1
                    cwnd += 1.0;
                    // increment until the c window becomes greater than ssthresh
                    // if cwnd is greater than ssthresh
                    if(cwnd >= ssthresh)
                        // change state to Congestion Avoidance
                        state = CA;
                }
                // If the state is Congestion Avoidance
                else if(state == CA)
                {
                    // Increment c window by a factor of 1/cwnd
                    cwnd += 1 / (floor(cwnd));
                }
            }

            // if dup ack recieved i.e. new ack is greater than 3
            else 
            {
                printf("3rd dup ack %d!\n", new_ack + 1);
                
                // retransmit 
                sequenceNum = new_ack + 1;
                acks[sequenceNum - 1000] = -1;
                acks[new_ack - 1000] = -1;

                // set state to slow start again once the retransmission is done
                state = SS;
                // set ssthresh to half of the cwdn
                ssthresh = cwnd / 2;
                // set the c window to 1.0 after the retransmission is done 
                cwnd = 1.0;
            }

            // check the timeout
            if(timeTwo >= timeOne + 3.0 * 1000)
            {
                printf("timeout!\n");

                acks[sequenceNum - 1000] = -1;
                
                // set the state
                state = SS;
                // set the ssthresh
                ssthresh = cwnd / 2;
                // set c window
                cwnd = 1.0;

            }
        }

        // next packet
        count++;
    }

    // Closing the socket
    if(close(clisock) < 0)
    {
        perror("closing socket failure");
        return EXIT_FAILURE;
    }
    else
    {
        fprintf(stdout, "Sucessfully closed the connection!!\n");

    }

    // Closing the socket
    if(close(servsock) < 0)
    {
        perror("socket failure!");
        return EXIT_FAILURE;
    }
    else
    {
        fprintf(stdout, "Sucessfully closed the socket!!\n");

    }



}