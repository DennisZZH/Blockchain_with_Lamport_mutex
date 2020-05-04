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
#include <list>

#include "Msg.pb.h"

#define PPORT 8000

char* server_ip = "127.0.0.1";
int myport;
int sockfd;

std::queue<Msg> events;
std::list<Msg> requests;
std::list<Msg> blockchain;
u_int32_t cur_clock = 0;
u_int32_t mypid;
int balance = 10;
int num_reply = 0;

pthread_t comm, proc;
pthread_mutex_t queue_lock, clock_lock;


bool isMePrioty(){
    // Check if the front of requests is of mypid
    // STUB
    return false;
}


int priority_push(Msg m){
    int status = 0;
    // push m to requests, sort requests on time stamp
    // STUB
    return status;
}

int safe_increment(int time){
    int status = 0;
    pthread_mutex_lock(&clock_lock);
    cur_clock += time;
    pthread_mutex_unlock(&clock_lock);
    return status;
}

int safe_push(Msg m){
    int status = 0;
    pthread_mutex_lock(&queue_lock);
    events.push(m);
    pthread_mutex_unlock(&queue_lock);
    return status;
}

Msg safe_pop(){
    Msg m;
    pthread_mutex_lock(&queue_lock);
    m = events.front();
    events.pop();
    pthread_mutex_unlock(&queue_lock);
    return m;
}

void print_balance(){
    std::cout<<"Process "<<mypid<<" "<<"balance: "<<balance<<std::endl;;
}

void print_blockchain(){
    std::cout<<"Process "<<mypid<<" "<<"print blockchain: ";
    for(auto i = blockchain.front(), i != blockchain.end(); i++){
        std::cout<<"(P"<<i->pid()<<", P"<<i->dst()<<", $"<<i->amt()<<"), ";
    }
    std::cout<<std::endl;
}

void *procThread(void* arg) {
    Msg m, n, temp;
    std::string msg_str;

    while(true){
        m.Clear();
        n.Clear();
        temp.Clear();
        msg_str.clear();

        if(!events.empty()){

            m = safe_pop();

            if(m.type() == 0){      // Transfer
               
                if(balance >= m.amt()){
                
                    n.set_type(1);      // Prepare to send requests
                    safe_increment(1);
                    n.set_clock(cur_clock);
                    n.set_pid(mypid);
                    n.set_IR(0);
                    n.set_dst(m.dst());
                    n.set_amt(m.amt());
                    n.SerializedToString(&msg_str);
                    if(send(sockfd, msg_str.c_str(), sizeof(Msg), 0) < 0){
                        std::cerr<<"Error: procThread failed to send the message!"<<std::endl;
                        exit(0);
                    }
                    requests.push_back(n);
                }else{
                    std::cout<<"FAILURE: insufficient balance!"<<endl;
                }

            }
        
            else if(m.type() == 1){     // Request
                
                priority_push(m);

                n.set_type(2);          // Prepare to send a reply
                safe_increment(1);
                n.set_clock(cur_clock);
                n.set_dst(m.pid());
                n.SerializedToString(&msg_str);
                if(send(sockfd, msg_str.c_str(), sizeof(Msg), 0) < 0){
                    std::cerr<<"Error: procThread failed to send the message!"<<std::endl;
                    exit(0);
                }

            }

            else if(m.type() == 2){     // Reply

                if(++num_reply < 2){
                   // wait for more reply
                }else{
                    if(isMePrioty()){
                        temp = requests.pop_front();

                        balance -= temp.amt();

                        n.set_type(3);      // Prepare to send a broadcast
                        safe_increment(1);
                        n.set_clock(cur_clock);
                        n.set_pid(mypid);
                        n.set_dst(temp.dst());
                        n.set_amt(temp.amt());

                        blockchain.push_back(n);

                        n.SerializedToString(&msg_str);
                        if(send(sockfd, msg_str.c_str(), sizeof(Msg), 0) < 0){
                            std::cerr<<"Error: procThread failed to send the message!"<<std::endl;
                            exit(0);
                        }

                        n.Clear();
                        msg_str.clear();
                        n.set_type(4);      // Prepare to send releases
                        safe_increment(1);
                        n.set_clock(cur_clock);
                        n.SerializedToString(&msg_str);
                        if(send(sockfd, msg_str.c_str(), sizeof(Msg), 0) < 0){
                            std::cerr<<"Error: procThread failed to send the message!"<<std::endl;
                            exit(0);
                        }

                    }
                    num_reply = 0;
                }
            }

            else if(m.type() == 3){     // Broadcast
                blockchain.push_back(m);
                if(mypid == m.pid()){
                    balance += m.amt();
                }
            }

            else if(m.type() == 4){     // Release

                requests.pop_front();

                if(!requests.empty()){

                    if(requests.front().get_pid() == mypid){
                        
                        temp = requests.pop_front();

                        balance -= temp.amt();

                        n.set_type(3);      // Prepare to send a broadcast
                        safe_increment(1);
                        n.set_clock(cur_clock);
                        n.set_pid(mypid);
                        n.set_dst(temp.dst());
                        n.set_amt(temp.amt());

                        blockchain.push_back(n);

                        n.SerializedToString(&msg_str);
                        if(send(sockfd, msg_str.c_str(), sizeof(Msg), 0) < 0){
                            std::cerr<<"Error: procThread failed to send the message!"<<std::endl;
                            exit(0);
                        }
                    }

                }

            }
          
        }
    }
}


void *commThread(void* arg) {
    char buf[sizeof(Msg)];
    int to_read = sizeof(Msg), siz_read = 0;
    std::string msg_str;
    Msg m;

    while(true){
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

        m.ParseFromString(msg_str);
        safe_push(m);
        if(m.clock() >= cur_clock){
            safe_increment(m.clock() + 1);
        }else{
            safe_increment(1);
        }

        m.Clear();
        msg_str.clear();
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
    if(pthread_mutex_init(&queue_lock, NULL) != 0) { 
        std::cerr<<"Error: queue lock mutex init has failed!"<<std::endl; 
        exit(0);
    }
    if(pthread_mutex_init(&clock_lock, NULL) != 0) { 
        std::cerr<<"Error: clock lock mutex init has failed!"<<std::endl; 
        exit(0);
    } 

    int input;
    uint32_t rid, amt;
    Msg m;

    while(input != 3){
        
        std::cout<<"Choose  0)Add transfer event  1)Print balance  2)Print blockchain  3)Quit :";
        std::cin>>input;
        if(std::cin.fail()){
            std::cout<<"Illegal input! Abort."<<std::endl;
            exit(0);
        }

        if(input == 1){
            print_balance();
        }

        else if(input == 2){
            print_blockchain();
        }

        else if(input == 0){

            std::cout<<"Input recipient PID: ";
            std::cin>>rid;

            std::cout<<"Input transfer amount: ";
            std::cin>>amt;

            m.set_type(0);
            m.set_dst(rid);
            m.set_amt(amt);

            safe_push(m);
            safe_increment(1);

            m.Clear();
        }

        else{
            std::cout<<"Invalid input! Please input again."<<std::endl;
        }

    }

    // Kill/Join proc and comm, terminate network
    m.Clear();
    m.set_type(5);
    send(sockfd, m.SerializeAsString().c_str(), sizeof(m), 0);

    pthread_kill(comm,SIGKILL);
    pthread_kill(proc,SIGKILL);
   
    return 0;
}