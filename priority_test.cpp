// test priority queue

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

std::list<Msg> requests;

bool compare(Msg newM, Msg target) {
    if (newM.clock() < target.clock()) {
        return true;
    }
    else if (newM.clock() == target.clock()) {
        if (newM.pid() < target.pid()) {
            return true;
        }
    }
    return false;
}

int priority_push(Msg m){
    int status = 0;
    std::list<Msg>::iterator it;
    // push m to requests, sort requests on time stamp
    if (requests.empty()) { // Empty list
        requests.push_back(m);
    }
    else {
        for (it = requests.begin(); it != requests.end(); ++it) {
            if (compare(m, *it)) { 
                // Insert
                requests.insert(it, m);
                break;
            }
        }
        if (it == requests.end()) {
            // Insert at the end
            requests.push_back(m);
        }
    }
    return status;
}

int main(){

    Msg m1, m2, m3;
    m1.set_type(1);
    m1.set_pid(2);
    m1.set_clock(1);

    m2.set_type(2);
    m2.set_pid(1);
    m2.set_clock(2);

    m1.set_type(3);
    m1.set_pid(1);
    m1.set_clock(1);
    

    priority_push(m1);

    std::cout<<requests.front().type()<<std::endl;

    priority_push(m2);

    std::cout<<requests.front().type()<<" "<<requests.back().type()<<std::endl;

    priority_push(m3);

    for(auto i = requests.begin(); i != requests.end(); i++){
        std::cout<<i->type()<<" ";
    }
    std::cout<<std::endl;

    return 0;
}