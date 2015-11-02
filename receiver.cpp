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

    const int BUFFER_SIZE = 1024;    
    char buffer[BUFFER_SIZE];
    string filename;
    
    setup(sender, senderAddr, argv, sockfd, portno, filename);
    
    string msg = "msg from receiver";
    if (sendto(sockfd, msg.c_str(), msg.size(), 0, 
            (struct sockaddr *) &senderAddr, senderAddrLength) < 0)
        error("sendto failed");
        
    int senderLength = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, 
                            (struct sockaddr *) &senderAddr, &senderAddrLength);
        
    if (senderLength > 0) {
        buffer[senderLength] = 0;
        printf("received message from sender: %s\n", buffer);
    }
    
    close(sockfd);
    return 0;
}
