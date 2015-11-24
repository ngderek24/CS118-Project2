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
#include <signal.h>
#include <poll.h>

using namespace std;

const int BUFFER_SIZE = 900;
const int HEADER_SIZE = 20;
int WINDOW_SIZE;
const int TIMEOUT = 2;
volatile bool TIMEOUT_FLAG = false;

void error(string msgString) {
    const char *msg = msgString.c_str();
    perror(msg);
    exit(0);
}

void handleTimeout(int sig) {
    TIMEOUT_FLAG = true;
    cout << "timeout!" << endl;
}

// setup the connection between server and client
void setup(char* argv[], int &socketfd, struct sockaddr_in &senderAddr, double& lossProb, double& corruptionProb) {
    // open socket
    socketfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (socketfd < 0)
        error("ERROR opening socket");
    
    // process input arguments
    int portNum = atoi(argv[1]);
    WINDOW_SIZE = atoi(argv[2]);
    lossProb = atof(argv[3]);
    corruptionProb = atof(argv[4]);
    
    // zero out bytes of senderAddr
    bzero((char *) &senderAddr, sizeof(senderAddr));
    
    // set senderAddr properties
    senderAddr.sin_family = AF_INET;
    senderAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    senderAddr.sin_port = htons(portNum);
    
    // bind socket to IP address and port number
    if (bind(socketfd, (struct sockaddr *) &senderAddr, sizeof(senderAddr)) < 0)
        error("ERROR on binding");
        
    // specify timeout handler
    signal(SIGALRM, handleTimeout);
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
    int bytesRead = fread(fileContent, 1, BUFFER_SIZE - HEADER_SIZE - 1, fp);
    if (bytesRead < 0) 
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
    //packetBuffer[HEADER_SIZE + bytesRead] = 'X';
    
    if (feof(fp)) {
        cout << "reached eof" << endl;
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

void printWindow(const char window[][BUFFER_SIZE]) {
    for (int i = 0 ; i < WINDOW_SIZE; i++) {
        for (int j = 0; j < BUFFER_SIZE; j++) {
            cout << window[i][j];
        }
        cout << endl;
    }
}

bool sendNextPacket(bool& sentLastPacket, char window[][BUFFER_SIZE], int& windowHead, 
                    FILE* fp, int& seqNum, int& lastByteRead, int& lastSeqNum, int& expectedAck, 
                    const bool& isLostPacket, const bool& isCorruptedPacket,
                    const int& socketfd, struct sockaddr_in* receiverAddr, const socklen_t& receiverAddrLength, char* readBuffer) {
    
    int ackNum = getAckNum(readBuffer);
    
    if (ackNum == lastSeqNum) {
        cout << "got last ACK!" << endl;
        cout << "not lost, not corrupted" << endl;
        return true;
    }
    else {
        if (isLostPacket)
            cout << "lost, ";
        else
            cout << "not lost, ";
        
        if (isCorruptedPacket)
            cout << "corrupted" << endl;
        else
            cout << "not corrupted" << endl;
    }
    
    if (!isLostPacket && !isCorruptedPacket && (ackNum >= expectedAck)) {
        // received last ACK, done!
        //if (ackNum == lastSeqNum) 
          //  return true;
    
        //cout << "got ACK " << ackNum << " expected ACK " << expectedAck << endl;
        
        alarm(TIMEOUT);
        
        int numPacketsToSend = ackNum - expectedAck + 1;
        for (int i = 0; i < numPacketsToSend; i++) {
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
    }
    
    return false;
}

bool isCorrupted(const double& corruptionProb) {
    int randomNum = rand() % 100 + 1;
    int corruptionPercent = corruptionProb * 100;
    
    return (randomNum <= corruptionPercent);
}

bool isLost(const double& lossProb) {
    int randomNum = rand() % 100 + 1;
    int lossPercent = lossProb * 100;
    
    return (randomNum <= lossPercent);
}

// extracts sequence number from a packet buffer
int getSeqNum(const char* buffer) {
    int len = strlen(buffer);
    char tempBuffer[HEADER_SIZE];
    int seqNum = -1;
    
    for (int i = 0; i < len; i++) {
        if (buffer[i] == '\n') {
            strncpy(tempBuffer, buffer, i);
            seqNum = atoi(tempBuffer);
            break;
        }
    }
    
    return seqNum;
}

void sendInitialWindow(char window[][BUFFER_SIZE], int windowTail,
                        const int& socketfd, struct sockaddr_in* receiverAddr, const socklen_t& receiverAddrLength) {
    // send the packet in the current window
    for (int i = 0; i < windowTail; i++) {
        while (!sendPacket(socketfd, receiverAddr, receiverAddrLength, window[i]))
            continue;
        printPacketInfo("DATA", getSeqNum(window[i]));
        
        // set the timer
        alarm(TIMEOUT);
    }
}

void sendCurrentWindow(char window[][BUFFER_SIZE], int windowHead,
                        const int& socketfd, struct sockaddr_in* receiverAddr, const socklen_t& receiverAddrLength, const int& lastSeqNum) {
    // send the packet in the current window
    for (int i = 0; i < WINDOW_SIZE; i++) {
        while (!sendPacket(socketfd, receiverAddr, receiverAddrLength, window[(windowHead + i) % WINDOW_SIZE]))
            continue;
            
        int curSeqNum = getSeqNum(window[(windowHead + i) % WINDOW_SIZE]);
        printPacketInfo("DATA", curSeqNum);
        
        // set the timer
        alarm(TIMEOUT);
        
        //cout << "lastSeqNum: " << lastSeqNum << endl;
        if (curSeqNum == lastSeqNum - 1)
            break;
    }
}

int main(int argc, char *argv[]) {
    //TODO: change number of arguments required
    if (argc != 5) {
        error("Please pass in arguments");
    }   
    
    int socketfd, receiverLength;
    struct sockaddr_in senderAddr, receiverAddr;
    socklen_t receiverAddrLength = sizeof(receiverAddr);
    double lossProb;
    double corruptionProb;
    
    // setup
    setup(argv, socketfd, senderAddr, lossProb, corruptionProb);
    
    while (1) {
        char readBuffer[BUFFER_SIZE] = {0};
        int bytesRead = 0;
        // receive filename
        receiverLength = recvfrom(socketfd, readBuffer, BUFFER_SIZE, 0, 
                            (struct sockaddr *) &receiverAddr, &receiverAddrLength);
        
        if (receiverLength > 0) {
            readBuffer[receiverLength] = 0;
            FILE *fp = fopen(readBuffer, "r");
            
            if (fp == NULL) {
                error("ERROR opening file");
            }
            
            // initialize socket timer
            struct timeval tv;
            tv.tv_sec = TIMEOUT;
            if (setsockopt(socketfd, SOL_SOCKET, SO_RCVTIMEO, &tv, (socklen_t) sizeof(tv)) < 0)
                perror("ERROR: getsockopt timeout");
            
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
                    windowTail++;
                    seqNum++;
                    lastSeqNum = seqNum;
                    //cout << "lastSeqNum: " << lastSeqNum << endl;
                    break;
                }
                seqNum++;
                windowTail++;
            }
            
            //cout << "printing initial window" << endl; 
            //printWindow(window);
            
            sendInitialWindow(window, windowTail, socketfd, &receiverAddr, receiverAddrLength);
            
            // listen for ACKs and send subsequent packets
            while (1) {
                // receive ACK packet
                memset(readBuffer, 0, BUFFER_SIZE);
                ackMsgLength = recvfrom(socketfd, readBuffer, HEADER_SIZE, 0, (struct sockaddr *) &receiverAddr, &receiverAddrLength);
                if (ackMsgLength <= 0) {
                    if (TIMEOUT_FLAG) {
                        sendCurrentWindow(window, windowHead, socketfd, &receiverAddr, receiverAddrLength, lastSeqNum);
                    }
                    continue;
                } 
                
                printPacketInfo("ACK", getAckNum(readBuffer));
                
                bool isCorruptedPacket = isCorrupted(corruptionProb);
                bool isLostPacket = isLost(lossProb);
                
                // packet ACKed successfully, move window onto next packet
                bool receivedLastACK = sendNextPacket(sentLastPacket, window, windowHead, 
                                                        fp, seqNum, lastByteRead, lastSeqNum, expectedAck, 
                                                        isLostPacket, isCorruptedPacket,
                                                        socketfd, &receiverAddr, receiverAddrLength, readBuffer);
                                                        
                if (receivedLastACK) {
                    // turn off alarm
                    alarm(0);
                    break;
                }
            }
            
            cout << "done sending file!" << endl;
            
            fclose(fp);
            //if (setsockopt(socketfd, SOL_SOCKET, SO_RCVTIMEO, &tv, (socklen_t) sizeof(tv)) < 0)
              //  perror("ERROR: getsockopt timeout");
        }
    }
    
    // close sockets 
    close(socketfd);
    return 0;
}
