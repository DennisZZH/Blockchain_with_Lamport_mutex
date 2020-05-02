// To compile:  protoc -I=. --cpp_out=. ./Msg.proto
//              c++ -std=c++11 Network.cpp Msg.pb.cc -o Network `pkg-config --cflags --libs protobuf`
#include <cstdlib>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <string.h>
#include <netdb.h> 
#include <errno.h>
#include "Msg.pb.h"
#include <sys/time.h>	// for gettimeofday() 

struct argus {
    int procNum;
    int *sockfd;
};

void *manageProcesses(void* args) {
    // Manage two other processes using this function
    // args: 1. sockfd: the listening socket for the corresponding process
    argus* argu = (argus *)args;
    int procNum = argu->procNum;
    int cur_sockfd = argu->sockfd[procNum];
    int r;
    srand(time(0));
    struct timeval start, end;


    // A while loop receiving from the corresponding process
    bool quit = false;
    char buffer[sizeof(Msg)];
    int read_size, sizeleft;
    while (!quit) {
        //std::cout << "Waiting and receving the message from process " << procNum + 1 << "...";
        sizeleft = sizeof(Msg);
        std::string strMessage;
        while (sizeleft != 0) {
            if ((read_size = recv(cur_sockfd, buffer, sizeof(buffer), 0)) < 0) {
                std::cerr << "Failed receving from process " << procNum + 1 << "\n";
                exit(0);
            }
            strMessage.append(buffer);
            sizeleft -= read_size;
            bzero(buffer, sizeof(buffer));
        }
        //std::cout << "Done!\n";
        Msg m;
        m.ParseFromString(strMessage);

        // Close the socket if type == 0
        if (m.type() == 3) {
            std::cout << "Process " << procNum + 1 << " exits.\n";
            quit = true;
        }

        else {
            // Determine the dst socket
            int send_socket;
            if (m.dst() == 1) {
                send_socket = argu->sockfd[0];
            }
            else if (m.dst() == 2) {
                send_socket = argu->sockfd[1];
            }
            else if (m.dst() == 3) {
                send_socket = argu->sockfd[2];
            }

            m.set_type(2);
            strMessage = m.SerializeAsString();

            // Send the message
            int send_size = 0;
            std::cout << "Waiting to send the message from " << m.src() << " to " << m.dst() << "......"<<std::endl;
            
            r = rand() % 5 + 1;
            gettimeofday(&start, NULL);
            while(true){
                gettimeofday(&end, NULL);
                long seconds = (end.tv_sec - start.tv_sec);
                if(seconds >= r) break;
                //std::cout<<"seconds = "<<seconds<<std::endl;
            }
        

            if ((send_size = send(send_socket, strMessage.c_str(), sizeof(Msg), 0)) < 0) {
                std::cerr << "Failed\n";
                exit(0);
            }
            std::cout << "Done!\n";
        }
    }
    close(cur_sockfd);
    return NULL;
}

int main(){
    struct sockaddr_in addresses[3];
    int addrlen[3];

    // Create 3 socket file descriptors for 3 processes
    int sockets[3];
    std::cout << "Creating the sockets...";
    for (int i = 0; i < 3; i++) {
        if ((sockets[i] = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
            std::cerr << "Failed creating " << i + 1 << " socket!\n";
            exit(0);
        }
    }
    std::cout << "Done!\n";

    // Bind all 3 sockets
    int port = 8001;
    std::cout << "Binding the sockets...";
    for (int i = 0; i < 3; i++) {
        addresses[i].sin_family = AF_INET;
        addresses[i].sin_addr.s_addr = INADDR_ANY;
        addresses[i].sin_port = htons(port + i);
        if (bind(sockets[i], (struct sockaddr *)(addresses + i), sizeof(addresses[i]))) {
            std::cerr << "Bind failed on " << i + 1 << " socket!\n";
            exit(0);
        }
    }
    std::cout << "Done!\n";

    // 3 sockets listening
    std::cout << "Listening...";
    for (int i = 0; i < 3; i++) {
        if (listen(sockets[i], 3) < 0) {
            std::cerr << "Failed on listening " << i + 1 << " socket.\n";
        }
    }
    std::cout << "Done!\n";

    // Connect with 3 processes
    int new_sockets[3];
    std::cout << "Connecting.";
    for (int i = 0; i < 3; i++) {
        addrlen[i] = sizeof(addresses[i]);
        if ((new_sockets[i] = accept(sockets[i], (struct sockaddr *)(addresses + i), (socklen_t *)(addrlen + 1))) < 0) {
            std::cerr << "Failed accepting process " << i + 1 << "\n";
            printf("Error number: %d\n", errno);
            printf("The error message is %s\n", strerror(errno));
            printf("Local socket connection with the server failed.\n");
            exit(errno);
        }
        std::cout << ".";
    }
    std::cout << "Done!\n";

    // Create two other threads to handle messages from process 2 and 3
    pthread_t tid[2];
    argus procArgu[3];
    for (int i = 0; i < 2; i++) {
        procArgu[i + 1].sockfd = new_sockets;
        procArgu[i + 1].procNum = i + 1;
        pthread_create(tid + i, NULL, manageProcesses, (void*)(&procArgu[i+1]));
    }

    // Call function for main thread
    procArgu[0].sockfd = new_sockets;
    procArgu[0].procNum = 0;
    manageProcesses((void*)(&procArgu[0]));

    // pthread_join for other two threads
    for (int i = 0; i < 2; i++) {
        pthread_join(tid[i], NULL);
    }

    std::cout << "The network processing is exiting...\n";
    return 0;
}