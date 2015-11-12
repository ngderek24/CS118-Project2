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
    char header[HEADER_SIZE];
    sprintf(header, "%d\n%d\n%d\n", seqNum, ackNum, finFlag);
    
    return string(header);
}

// send a packet that was built with buildPacket
bool sendPacket(const int& socketfd, struct sockaddr_in* receiverAddr, 
                const socklen_t& receiverAddrLength, const char* packetBuffer) {
                
    if (sendto(socketfd, packetBuffer, strlen(packetBuffer), 0, 
                (struct sockaddr *) receiverAddr, receiverAddrLength) < 0)
        return false;
    else 
        return true;    
}

// build a packet, return true if last packet
bool buildPacket(char* packetBuffer, FILE* fp, const int& seqNum, int& lastByteRead) {
    memset(packetBuffer, 0, BUFFER_SIZE);
    char fileContent[BUFFER_SIZE - HEADER_SIZE] = {0};
    string headers;
    bool returnVal = false;
    
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
    packetBuffer[HEADER_SIZE + bytesRead] = 0;
    
    //sendPacket(socketfd, receiverAddr, receiverAddrLength, packetBuffer);
    
    if (feof(fp)) {
        returnVal = true;
    }
    
    rewind(fp);
    if (fseek(fp, lastByteRead, SEEK_CUR) != 0)
        error("ERROR seeking file");
        
    return returnVal;
}

// get acknowledgement number from packet header
int getAckNum(const char* buffer) {
    char tempBuffer[HEADER_SIZE];
    int len = strlen(buffer);
    int firstNewline = -1;
    int count = 0;
    int ackNum = 0;
    for (int i = 0; i < len; i++) {
        if (buffer[i] == '\n')
            count++;
        if (count == 1 && firstNewline == -1)
            firstNewline = i;
        if (count == 2) {
            strncpy(tempBuffer, buffer + firstNewline + 1, i - firstNewline - 1);
            ackNum = atoi(tempBuffer);
            break;
        } 
    }
    return ackNum;
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
            int lastByteRead = 0;
            int seqNum = 1;
            char packetBuffer[BUFFER_SIZE];
            int ackMsgLength = 0;
            
            // send the file
            while (1) {
                bool isLastPacket = buildPacket(packetBuffer, fp, seqNum, lastByteRead);
                
                while(1) {
                    if (!sendPacket(socketfd, &receiverAddr, receiverAddrLength, packetBuffer))
                        continue;
                    
                    // receive ACK packet
                    ackMsgLength = recvfrom(socketfd, buffer, HEADER_SIZE, 0, (struct sockaddr *) &receiverAddr, &receiverAddrLength);
                    
                    // current packet ACKed successfully, move onto next packet
                    if (getAckNum(buffer) == seqNum + 1) {
                        seqNum++;
                        break;
                    }
                }
                
                if (isLastPacket)
                    break;
            }
            fclose(fp);
        }
    }
    
    // close sockets 
    close(socketfd);
    return 0;
}
