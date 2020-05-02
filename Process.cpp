#include <iostream>
#include <queue>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <string.h>
#include <netdb.h> 
#include <errno.h>
#include <pthread.h>

#include "Msg.pb.h"

#define PPORT 8000

char* server_ip = "127.0.0.1";
int myport;
int sockfd;

std::queue<Msg> events;  // Share resource, protected
std::vector<Msg> clocks;
u_int cur_clock = 0;
u_int mypid;

pthread_t comm, proc;
pthread_mutex_t lock;


int safe_push(Msg m){
    int status = 0;
    pthread_mutex_lock(&lock);
    events.push(m);
    pthread_mutex_unlock(&lock);
    return status;
}


Msg safe_pop(){
    Msg m;
    pthread_mutex_lock(&lock);
    m = events.front();
    events.pop();
    pthread_mutex_unlock(&lock);
    return m;
}


void print_clocks(){
    std::cout<<"Process "<<mypid<<" "<<"print clock: ";
    for(auto i = clocks.begin(); i != clocks.end(); i++){
        std::cout<<i->clock()<<" ";
    }
    std::cout<<std::endl;
}



void *procThread(void* arg) {
    // Loop to check if the std::queue is empty. If not, pop the event and calculate the new clock value for it and process.
    Msg m;
    std::string msg_str;

    while(true){
        if(!events.empty()){
            //std::cout<<"Before: Size of queue = "<<events.size()<<std::endl;
            // Pop events
            m = safe_pop();
            //std::cout<<"After: Size of queue = "<<events.size()<<std::endl;
            // if it is local event
            if(m.type() == 0){
                //std::cout << "Local event: (\"" << m.text() << "\") of Process " << m.src() << "\n";
                cur_clock += 1;
                m.set_clock(cur_clock);
            }
            // if it is recv event
            else if(m.type() == 2){
                //std::cout << "Receive event: (\"" << m.text() << "\") from Process " << m.src() << "\n";
                if(m.clock() > cur_clock + 1){
                    cur_clock = m.clock() + 1;

                }else{
                    cur_clock += 1;
                }
                m.set_clock(cur_clock);
            }
            // if it is send event
            else if(m.type() == 1){
                //std::cout << "Send event: (\"" << m.text() << "\") to Process " << m.dst() << "\n";
                cur_clock += 1;
                m.set_clock(cur_clock);
                m.SerializeToString(&msg_str);
                if(send(sockfd, msg_str.c_str(), sizeof(Msg), 0) < 0){
                    std::cerr<<"Error: procThread failed to send the message!"<<std::endl;
                    exit(0);
                }
            }
            // push to clocks queue
            clocks.push_back(m);
        }
    }
}


void *commThread(void* arg) {
    // Loop to listen and receive from the socket. Add a receive event to the std::queue if received
    char buf[sizeof(Msg)];
    int to_read = sizeof(Msg), siz_read = 0;
    std::string msg_str;
    Msg m;

    while(true){
        // Receive message
        while(to_read != 0){
            siz_read = recv(sockfd, buf, sizeof(Msg), 0);
            if(siz_read < 0){
                std::cerr<<"Error: commThread failed to recv the message!"<<std::endl;
                exit(0);
            }
            to_read -= siz_read;
            msg_str.append(buf);
            bzero(buf, sizeof(buf));
        }

        // Cast std::string to Msg
        m.ParseFromString(msg_str);
        // Add a receive event to std::queue
        safe_push(m);
        to_read = sizeof(Msg);
    }
}


int main() {
   
    // Assign process id
    std::cout << "Process id: ";
    std::cin >> mypid;
    
    // Build a TCP socket connecting with the network process
    myport = PPORT + mypid;
    struct sockaddr_in server_address;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        printf("Socket creation failed.\n");
        exit(0);
    }

    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = inet_addr(server_ip);
    server_address.sin_port = htons(myport);

    if (connect(sockfd, (struct sockaddr*)&server_address, sizeof(server_address)) != 0) {
        printf("Error number: %d\n", errno);
        printf("The error message is %s\n", strerror(errno));
        printf("Connection with the server failed.\n");
        exit(0);
    }

    // Open communication thread and processing thread
    pthread_create(&comm, NULL, &commThread, NULL);
    pthread_create(&proc, NULL, &procThread, NULL);
    if(pthread_mutex_init(&lock, NULL) != 0) { 
        std::cerr<<"Error: mutex init has failed!"<<std::endl; 
        exit(0);
    } 

    // A while loop taking the user input
    int input;
    std::string txt;
    Msg m;
    u_int pid_dest;

    while(input != 3){
        
        std::cout<<"Choose  0)add local event  1)add send event  2)print clock  3)quit :";
        std::cin>>input;
        if(std::cin.fail()){
            std::cout<<"Illegal input! Abort."<<std::endl;
            exit(0);
        }

        // If print clocks
        if(input == 2){
            print_clocks();
            continue;
        }

        else if(input == 0 || input == 1){

            // Keep taking user input, creating events
            std::cout<<"Input event name/message: ";
            std::cin>>txt;

            // Create Message
            m.set_type(input);
            m.set_clock(0); // Default, waiting to be set when being processed
            m.set_text(txt);

            // If input a send event
            if(input == 1){
                std::cout<<"Input receiver Process id: ";
                std::cin>>pid_dest;
                m.set_dst(pid_dest);
                m.set_src(mypid);
            }

            // push event to std::queue
            safe_push(m);

            // clear buf
            m.Clear();
            txt.clear();

        }

        else if (input != 3) {
            std::cout<<"Invalid input! Please input again."<<std::endl;
        }

    }

    // Kill/Join proc and comm, terminate network
    m.Clear();
    m.set_type(3);
    m.set_clock(0);
    m.set_text("");
    send(sockfd, m.SerializeAsString().c_str(), sizeof(m), 0);

    pthread_kill(comm,SIGKILL);
    pthread_kill(proc,SIGKILL);
   
    return 0;
}