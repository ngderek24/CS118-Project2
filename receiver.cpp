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

const int BUFFER_SIZE = 30;
const int HEADER_SIZE = 20;

void error(string msgString) {
    const char *msg = msgString.c_str();
    perror(msg);
    exit(0);
}

void setup(struct hostent *sender, struct sockaddr_in& senderAddr,
            char* argv[], int& sockfd, int& portno, string& filename) {
    // takes a string like "www.yahoo.com", 
    // and returns a struct hostent which contains information, 
    // as IP address, address type, the length of the addresses
    sender = gethostbyname(argv[1]); 
    if (sender == NULL) {
        fprintf(stderr,"ERROR, no such host\n");
        exit(0);
    }
    
    portno = atoi(argv[2]);
    filename = std::string(argv[3]);
    
    sockfd = socket(AF_INET, SOCK_DGRAM, 0); //create a new socket
    if (sockfd < 0) 
        error("ERROR opening socket");
    
    bzero((char *) &senderAddr, sizeof(senderAddr));
    senderAddr.sin_family = AF_INET; //initialize server's address
    bcopy((char *) sender->h_addr, (char *) &senderAddr.sin_addr.s_addr, sender->h_length);
    senderAddr.sin_port = htons(portno);
}

// build a header
string buildHeaders(int seqNum, int ackNum, int finFlag) {
    char header[HEADER_SIZE];
    sprintf(header, "%d\n%d\n%d\n", seqNum, ackNum, finFlag);
    
    return string(header);
}

bool isLastPacket(char* buffer) {
    int len = strlen(buffer);
    int count = 0;
    for (int i = 0; i < len; i++) {
        if (buffer[i] == '\n')
            count++;
        if (count == 2) {
            if (buffer[i+1] == '1')
                return true;
            else
                return false;
        }
    }
    return false;
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

// send ACK packet to sender
bool sendAckPacket(const int& socketfd, struct sockaddr_in* senderAddr, 
                const socklen_t& senderAddrLength, const int& ackNum) {
    string header = buildHeaders(-1, ackNum, 0);
    if (sendto(socketfd, header.c_str(), HEADER_SIZE, 0, 
                (struct sockaddr *) senderAddr, senderAddrLength) < 0)
        return false;
    else
        return true;
}

void printPacketInfo(const string& packetType, const int& num) {
    cout << packetType << " " << num << endl;
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
       fprintf(stderr,"usage %s hostname port\n", argv[0]);
       exit(0);
    }
    
    int sockfd; //Socket descriptor
    int portno, n;
    struct sockaddr_in senderAddr;
    socklen_t senderAddrLength = sizeof(senderAddr);
    struct hostent *sender; //contains tons of information, including the server's IP address

    char buffer[BUFFER_SIZE];
    string filename;
    
    setup(sender, senderAddr, argv, sockfd, portno, filename);
    
    if (sendto(sockfd, filename.c_str(), filename.size(), 0, 
            (struct sockaddr *) &senderAddr, senderAddrLength) < 0)
        error("sendto failed");
        
    FILE* fp = fopen(filename.c_str(), "w+");
    int bytesWritten = 0;
    
    while (1) { 
        int senderLength = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, 
                                (struct sockaddr *) &senderAddr, &senderAddrLength);
        buffer[senderLength] = 0;
        printPacketInfo("DATA", getSeqNum(buffer));
        
        if (senderLength > 0) {
            buffer[senderLength] = 0;
            printf("msg from sender: %s\n", buffer);
            
            bytesWritten = fwrite(buffer + HEADER_SIZE, 1, BUFFER_SIZE - HEADER_SIZE, fp);
            if (bytesWritten <= 0)
                error("ERROR writing to file");
                
            while (!sendAckPacket(sockfd, &senderAddr, senderAddrLength, getSeqNum(buffer) + 1))
                continue;
            printPacketInfo("ACK", getSeqNum(buffer) + 1);
        }
        
        if (isLastPacket(buffer)) {
            cout << "got last packet" << endl;
            break;
        }
    }
    
    close(sockfd);
    return 0;
}
