//=======================================================================
// Copyright (c) 2015 Baptiste Wicht
// Distributed under the terms of the MIT License.
// (See accompanying file LICENSE or copy at
//  http://opensource.org/licenses/MIT)
//=======================================================================

#include <iostream>
#include <cstdlib>
#include <cstdio>
#include <cstring>

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

int main(){
    //Open the socket
    auto socket_fd = socket(PF_UNIX, SOCK_STREAM, 0);
    if(socket_fd < 0){
        std::cout << "asgard:rf: socket() failed" << std::endl;
        return 1;
    }

    //Init the address
    struct sockaddr_un address;
    memset(&address, 0, sizeof(struct sockaddr_un));
    address.sun_family = AF_UNIX;
    snprintf(address.sun_path, UNIX_PATH_MAX, "/tmp/asgard_socket");

    //Connect to the server
    if(connect(socket_fd, (struct sockaddr *) &address, sizeof(struct sockaddr_un)) != 0){
        std::cout << "asgard:rf: connect() failed" << std::endl;
        return 1;
    }

    //TODO

    //Close the socket
    close(socket_fd);

    return 0;
}
