#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdlib.h>
#include <strings.h>
#include <iostream>
#include <ctime>
#include <cstdio>
#include <cstring>
#include <unistd.h>

using namespace std;

void error(string msgString) {
    const char *msg = msgString.c_str();
    perror(msg);
    exit(0);
}

// setup the connection between server and client
void setup(int &socketfd, struct sockaddr_in &senderAddr, int portNum) {
    // open socket
    socketfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (socketfd < 0)
        error("ERROR opening socket");
    
    // zero out bytes of senderAddr
    bzero((char *) &senderAddr, sizeof(senderAddr));
    
    // set senderAddr properties
    senderAddr.sin_family = AF_INET;
    senderAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    senderAddr.sin_port = htons(portNum);
    
    // bind socket to IP address and port number
    if (bind(socketfd, (struct sockaddr *) &senderAddr, sizeof(senderAddr)) < 0)
        error("ERROR on binding");
}

// handles request accordingly
void handleRequest(int socketfd) {
    int result;
    char filename[128];
    bzero(filename, 128);

    // read client request
    result = read(socketfd, filename, 128);
    if (result < 0) 
        error("ERROR reading from socket");

    // if no file specified, just display a message
    if (strlen(filename) == 0) {
        string response = "Invalid filename";
        result = write(socketfd, response.c_str(), response.size());
        
        if (result < 0) 
            error("ERROR writing to socket");
    }
    else {
        // open file for reading
        FILE *fp = fopen(filename, "r");
        if (fp == NULL) {
            string response = "File Not Found";
            result = write(socketfd, response.c_str(), response.size());
            if (result < 0) 
                error("ERROR writing to socket");
            //cout << "ERROR opening file" << endl;
        }
        else {
            // write file to buffer
            const int BUFFER_SIZE = 2000000;
            char writeBuffer[BUFFER_SIZE] = { 0 };
            int bytesRead = fread(writeBuffer, 1, BUFFER_SIZE, fp);
            
            if (bytesRead > 0) {
                // send response to client
                result = write(socketfd, writeBuffer, BUFFER_SIZE);
                if (result < 0) 
                    error("ERROR writing to socket");
            }
            else {
                string response = "Error Reading File";
                result = write(socketfd, response.c_str(), response.size());
                if (result < 0) 
                    error("ERROR writing to socket");
            }
        }
        fclose(fp);
    }
}

int main(int argc, char *argv[]) {
    if (argc == 1) {
        error("Please pass in arguments");
    }   
    
    int socketfd, portNum, receiverLength;
    struct sockaddr_in senderAddr, receiverAddr;
    socklen_t receiverAddrLength = sizeof(receiverAddr);
    
    // get port number and setup
    portNum = atoi(argv[1]);
    setup(socketfd, senderAddr, portNum);
    
    const int BUFFER_SIZE = 1024;
    char buffer[BUFFER_SIZE] = {0};
 
    while (1) {
        receiverLength = recvfrom(socketfd, buffer, BUFFER_SIZE, 0, 
                            (struct sockaddr *) &receiverAddr, &receiverAddrLength);
        
        if (receiverLength > 0) {
            buffer[receiverLength] = 0;
            printf("received message: %s\n", buffer);

            if (sendto(socketfd, "hello", 5, 0, 
                        (struct sockaddr *) &receiverAddr, receiverAddrLength) < 0)
                error("sendto failed");
        }
    }
    
    // close sockets 
    close(socketfd);
    return 0;
}
