#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdlib.h>
#include <unistd.h>
#include <iostream>
#include <cstdio>
#include <cstring>
#include <string>

using namespace std;

const int BUFFER_SIZE = 30;
const int HEADER_SIZE = 20;

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

// build a header
string buildHeaders(int seqNum, int ackNum, int finFlag) {
    string header = to_string(seqNum) + "\n" 
                    + to_string(ackNum) + "\n" 
                    + to_string(finFlag) + "\n";
    return header;
}

// build and send packet, return true if last packet
bool sendPacket(const int& socketfd, struct sockaddr_in* receiverAddr, 
                const socklen_t& receiverAddrLength, FILE* fp, 
                const int& seqNum, int& lastByteRead) {
    char packetBuffer[BUFFER_SIZE] = {0};
    char fileContent[BUFFER_SIZE - HEADER_SIZE] = {0};
    string headers;
    
    // read file into fileContent
    int bytesRead = fread(fileContent, 1, BUFFER_SIZE - HEADER_SIZE, fp);
    if (bytesRead <= 0) 
        error("ERROR reading file");
    lastByteRead += bytesRead;
    
    // reached end of file
    if (feof(fp)) {
        headers = buildHeaders(seqNum, -1, 1);
    } else {
        headers = buildHeaders(seqNum, -1, 0);
    }
    
    // copy header into packetBuffer
    strcpy(packetBuffer, headers.c_str());
    int headerSize = headers.size();
    memset(packetBuffer + headerSize, ' ', HEADER_SIZE - headerSize);
    
    strcpy(packetBuffer + HEADER_SIZE, fileContent);
    
    cout << packetBuffer << endl;
    
    if (sendto(socketfd, packetBuffer, HEADER_SIZE + bytesRead, 0, 
                (struct sockaddr *) receiverAddr, receiverAddrLength) < 0)
        error("sendto failed");
    
    if (feof(fp)) {
        return true;
    }
    
    rewind(fp);
    if (fseek(fp, lastByteRead, SEEK_CUR) != 0)
        error("ERROR seeking file");
        
    return false;
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
    
    char buffer[BUFFER_SIZE] = {0};
    int lastByteRead = 0;
    int bytesRead = 0;
 
    while (1) {
        receiverLength = recvfrom(socketfd, buffer, BUFFER_SIZE, 0, 
                            (struct sockaddr *) &receiverAddr, &receiverAddrLength);
        
        if (receiverLength > 0) {
            buffer[receiverLength] = 0;
            FILE *fp = fopen(buffer, "r");
            
            if (fp == NULL) {
                error("ERROR opening file");
            }
            
            rewind(fp);
            int seqNum = 1;
            string headers = "";
            char fileContent[BUFFER_SIZE - HEADER_SIZE];
            
            // send the file
            while (1) {
                if (sendPacket(socketfd, &receiverAddr, receiverAddrLength, fp, seqNum, lastByteRead))
                    break;
                seqNum++;
            }
            fclose(fp);
        }
    }
    
    // close sockets 
    close(socketfd);
    return 0;
}
