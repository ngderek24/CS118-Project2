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
#include <ctime>

using namespace std;

const int BUFFER_SIZE = 30;
const int HEADER_SIZE = 20;
int WINDOW_SIZE;

void error(string msgString) {
    const char *msg = msgString.c_str();
    perror(msg);
    exit(0);
}

// setup the connection between server and client
void setup(char* argv[], int &socketfd, struct sockaddr_in &senderAddr, double& corruptionProb) {
    // open socket
    socketfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (socketfd < 0)
        error("ERROR opening socket");
    
    // process input arguments
    int portNum = atoi(argv[1]);
    WINDOW_SIZE = atoi(argv[2]);
    corruptionProb = atof(argv[3]);
    
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
    //memset(packetBuffer, ' ', BUFFER_SIZE);
    strcpy(packetBuffer, headers.c_str());
    int headerSize = headers.size();
    memset(packetBuffer + headerSize, ' ', BUFFER_SIZE - headerSize);
    
    strcpy(packetBuffer + HEADER_SIZE, fileContent);
    packetBuffer[HEADER_SIZE + bytesRead] = 0;
    
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

void printPacketInfo(const string& packetType, const int& num) {
    cout << packetType << " " << num << endl;
}

bool sendNextPacket(bool& sentLastPacket, char window[][BUFFER_SIZE], int& windowHead, 
                    FILE* fp, int& seqNum, int& lastByteRead, int& lastSeqNum, int& expectedAck,
                    const int& socketfd, struct sockaddr_in* receiverAddr, const socklen_t& receiverAddrLength, char* readBuffer) {
    // received last ACK, done!
    if (getAckNum(readBuffer) == lastSeqNum)
        return true;
    
    if (getAckNum(readBuffer) == expectedAck) {
        // if not done, build and send next packet
        if (!sentLastPacket) {
            sentLastPacket = buildPacket(window[windowHead], fp, seqNum, lastByteRead);
            //cout << "sending: " << window[windowHead] << endl;
    
            while (!sendPacket(socketfd, receiverAddr, receiverAddrLength, window[windowHead]))
                continue;
        
            printPacketInfo("DATA", seqNum);
            seqNum++;
    
            //handle last packet
            if (sentLastPacket) {
                lastSeqNum = seqNum;
            }
        }
    
        // increment windowHead
        windowHead  = (windowHead + 1) % WINDOW_SIZE;
        expectedAck++;
    }
    
    return false;
}

bool isCorrupted(const double& corruptionProb) {
    int randomNum = rand() % 100 + 1;
    int corruptionPercent = (int) corruptionProb * 100;
    
    return (randomNum <= corruptionPercent);
}

int main(int argc, char *argv[]) {
    if (argc == 1) {
        error("Please pass in arguments");
    }   
    
    int socketfd, receiverLength;
    struct sockaddr_in senderAddr, receiverAddr;
    socklen_t receiverAddrLength = sizeof(receiverAddr);
    double corruptionProb;
    
    // setup
    setup(argv, socketfd, senderAddr, corruptionProb);
    
    char readBuffer[BUFFER_SIZE] = {0};
    int bytesRead = 0;
 
    while (1) {
        // receive filename
        receiverLength = recvfrom(socketfd, readBuffer, BUFFER_SIZE, 0, 
                            (struct sockaddr *) &receiverAddr, &receiverAddrLength);
        
        if (receiverLength > 0) {
            readBuffer[receiverLength] = 0;
            FILE *fp = fopen(readBuffer, "r");
            
            if (fp == NULL) {
                error("ERROR opening file");
            }
            
            rewind(fp);
            int lastByteRead = 0;
            int seqNum = 1;
            char writeBuffer[BUFFER_SIZE];
            int ackMsgLength = 0;
            
            char window[WINDOW_SIZE][BUFFER_SIZE];
            int windowHead = 0;
            int windowTail = 0;
            int expectedAck = 2;
            
            bool sentLastPacket = false;
            int lastSeqNum = 0;
            
            // fill the initial window
            for (int i = 0; i < WINDOW_SIZE; i++) {
                sentLastPacket = buildPacket(window[i], fp, seqNum, lastByteRead);
                if (sentLastPacket) {
                    windowTail = i;
                    lastSeqNum = seqNum;
                    break;
                }
                seqNum++;
                windowTail++;
            }
            
            // send the initial packets
            for (int i = 0; i < windowTail; i++) {
                while (!sendPacket(socketfd, &receiverAddr, receiverAddrLength, window[i]))
                    continue;
                printPacketInfo("DATA", i + 1);
            }
            
            // listen for ACKs and send subsequent packets
            while (1) {
                // receive ACK packet
                ackMsgLength = recvfrom(socketfd, readBuffer, HEADER_SIZE, 0, (struct sockaddr *) &receiverAddr, &receiverAddrLength);
                if (ackMsgLength <= 0) {
                    cout << "ERROR receiving empty ACK packet" << endl;
                    continue;
                } 
                printPacketInfo("ACK", getAckNum(readBuffer));
                
                bool isCorruptedPacket = isCorrupted(corruptionProb);
                
                // packet ACKed successfully, move window onto next packet
                bool receivedLastACK = sendNextPacket(sentLastPacket, window, windowHead, 
                                                        fp, seqNum, lastByteRead, lastSeqNum, expectedAck,
                                                        socketfd, &receiverAddr, receiverAddrLength, readBuffer);
                                                        
                if (receivedLastACK)
                    break;
            }
            
            cout << "done sending file!" << endl;
            
            fclose(fp);
        }
    }
    
    // close sockets 
    close(socketfd);
    return 0;
}
