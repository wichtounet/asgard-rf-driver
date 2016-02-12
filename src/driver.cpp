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
#include <signal.h>

#include "RCSwitch.h"

namespace {

const std::size_t UNIX_PATH_MAX = 108;
const std::size_t gpio_pin = 2;
const std::size_t buffer_size = 4096;

// Configuration (this should be in a configuration file)
const char* server_socket_path = "/tmp/asgard_socket";
const char* client_socket_path = "/tmp/asgard_rf_socket";

//Buffers
char write_buffer[buffer_size + 1];
char receive_buffer[buffer_size + 1];

// The socket file descriptor
int socket_fd;

// The socket addresses
struct sockaddr_un client_address;
struct sockaddr_un server_address;

// The remote IDs
int source_id = -1;
int temperature_sensor_id = -1;
int humidity_sensor_id = -1;
int button_actuator_id = -1;

void stop(){
    std::cout << "asgard:rf: stop the driver" << std::endl;

    // Unregister the temperature sensor, if necessary
    if(temperature_sensor_id >= 0){
        auto nbytes = snprintf(write_buffer, buffer_size, "UNREG_SENSOR %d %d", source_id, temperature_sensor_id);
        sendto(socket_fd, write_buffer, nbytes, 0, (struct sockaddr *) &server_address, sizeof(struct sockaddr_un));
    }

    // Unregister the humidity sensor, if necessary
    if(temperature_sensor_id >= 0){
        auto nbytes = snprintf(write_buffer, buffer_size, "UNREG_SENSOR %d %d", source_id, humidity_sensor_id);
        sendto(socket_fd, write_buffer, nbytes, 0, (struct sockaddr *) &server_address, sizeof(struct sockaddr_un));
    }

    // Unregister the button actuator, if necessary
    if(temperature_sensor_id >= 0){
        auto nbytes = snprintf(write_buffer, buffer_size, "UNREG_ACTUATOR %d %d", source_id, button_actuator_id);
        sendto(socket_fd, write_buffer, nbytes, 0, (struct sockaddr *) &server_address, sizeof(struct sockaddr_un));
    }

    // Unregister the source, if necessary
    if(source_id >= 0){
        auto nbytes = snprintf(write_buffer, buffer_size, "UNREG_SOURCE %d", source_id);
        sendto(socket_fd, write_buffer, nbytes, 0, (struct sockaddr *) &server_address, sizeof(struct sockaddr_un));
    }

    // Unlink the client socket
    unlink(client_socket_path);

    // Close the socket
    close(socket_fd);
}

void terminate(int){
    stop();

    std::exit(0);
}


bool revoke_root(){
    if (getuid() == 0) {
        if (setgid(1000) != 0){
            std::cout << "asgard:rf: setgid: Unable to drop group privileges: " << strerror(errno) << std::endl;
            return false;
        }

        if (setuid(1000) != 0){
            std::cout << "asgard:rf: setgid: Unable to drop user privileges: " << strerror(errno) << std::endl;
            return false;
        }
    }

    if (setuid(0) != -1){
        std::cout << "asgard:rf: managed to regain root privileges, exiting..." << std::endl;
        return false;
    }

    return true;
}

void decode_wt450(unsigned long data){
    int house=(data>>28) & (0x0f);
    byte station=((data>>26) & (0x03))+1;
    int humidity=(data>>16)&(0xff);
    double temperature=((data>>8) & (0xff));
    temperature = temperature - 50;
    byte tempfraction=(data>>4) & (0x0f);

    double tempdecimal=((tempfraction>>3 & 1) * 0.5) + ((tempfraction>>2 & 1) * 0.25) + ((tempfraction>>1 & 1) * 0.125) + ((tempfraction & 1) * 0.0625);
    temperature=temperature+tempdecimal;
    temperature=(int)(temperature*10);
    temperature=temperature/10;

    //Note: House and station can be used to distinguish between different weather stations
    (void) house;
    (void) station;

    //Send the humidity to the server
    auto nbytes = snprintf(write_buffer, buffer_size, "DATA %d %d %d", source_id, humidity_sensor_id, humidity);
    sendto(socket_fd, write_buffer, nbytes, 0, (struct sockaddr *) &server_address, sizeof(struct sockaddr_un));

    //Send the temperature to the server
    nbytes = snprintf(write_buffer, buffer_size, "DATA %d %d %f", source_id, temperature_sensor_id, temperature);
    sendto(socket_fd, write_buffer, nbytes, 0, (struct sockaddr *) &server_address, sizeof(struct sockaddr_un));
}

void read_data(RCSwitch& rc_switch){
    if (rc_switch.available()) {
        int value = rc_switch.getReceivedValue();

        if (value) {
            if((rc_switch.getReceivedProtocol() == 1 || rc_switch.getReceivedProtocol() == 2) && rc_switch.getReceivedValue() == 1135920){
                //Send the event to the server
                auto nbytes = snprintf(write_buffer, buffer_size, "EVENT %d %d %d", source_id, button_actuator_id, 1);
                sendto(socket_fd, write_buffer, nbytes, 0, (struct sockaddr *) &server_address, sizeof(struct sockaddr_un));
            } else if(rc_switch.getReceivedProtocol() == 5){
                unsigned long value = rc_switch.getReceivedValue();
                decode_wt450(value);
            } else {
                printf("asgard:rf:received unknown value: %lu\n", rc_switch.getReceivedValue());
                printf("asgard:rf:received unknown protocol: %i\n", rc_switch.getReceivedProtocol());
            }
        } else {
            printf("asgard:rf:received unknown encoding\n");
        }

        rc_switch.resetAvailable();
    }
}

} //end of anonymous namespace

int main(){
    RCSwitch rc_switch;

    //Run the wiringPi setup (as root)
    wiringPiSetup();

    rc_switch = RCSwitch();
    rc_switch.enableReceive(gpio_pin);

    //Drop root privileges and run as pi:pi again
    if(!revoke_root()){
       std::cout << "asgard:rf: unable to revoke root privileges, exiting..." << std::endl;
       return 1;
    }

    // Open the socket
    socket_fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if(socket_fd < 0){
        std::cerr << "asgard:rf: socket() failed" << std::endl;
        return 1;
    }

    // Init the client address
    memset(&client_address, 0, sizeof(struct sockaddr_un));
    client_address.sun_family = AF_UNIX;
    snprintf(client_address.sun_path, UNIX_PATH_MAX, client_socket_path);

    // Unlink the client socket
    unlink(client_socket_path);

    // Bind to client socket
    if(bind(socket_fd, (const struct sockaddr *) &client_address, sizeof(struct sockaddr_un)) < 0){
        std::cerr << "asgard:rf: bind() failed" << std::endl;
        return 1;
    }

    //Register signals for "proper" shutdown
    signal(SIGTERM, terminate);
    signal(SIGINT, terminate);

    // Init the server address
    memset(&server_address, 0, sizeof(struct sockaddr_un));
    server_address.sun_family = AF_UNIX;
    snprintf(server_address.sun_path, UNIX_PATH_MAX, server_socket_path);

    socklen_t address_length = sizeof(struct sockaddr_un);

    // Register the source
    auto nbytes = snprintf(write_buffer, buffer_size, "REG_SOURCE rf");
    sendto(socket_fd, write_buffer, nbytes, 0, (struct sockaddr *) &server_address, sizeof(struct sockaddr_un));

    auto bytes_received = recvfrom(socket_fd, receive_buffer, buffer_size, 0, (struct sockaddr *) &(server_address), &address_length);
    receive_buffer[bytes_received] = '\0';

    source_id = atoi(receive_buffer);

    std::cout << "asgard:rf: remote source: " << source_id << std::endl;

    // Register the temperature sensor
    nbytes = snprintf(write_buffer, buffer_size, "REG_SENSOR %d %s %s", source_id, "TEMPERATURE", "rf_weather");
    sendto(socket_fd, write_buffer, nbytes, 0, (struct sockaddr *) &server_address, sizeof(struct sockaddr_un));

    bytes_received = recvfrom(socket_fd, receive_buffer, buffer_size, 0, (struct sockaddr *) &(server_address), &address_length);
    receive_buffer[bytes_received] = '\0';

    temperature_sensor_id = atoi(receive_buffer);

    std::cout << "asgard:rf: remote temperature sensor: " << temperature_sensor_id << std::endl;

    // Register the humidity sensor
    nbytes = snprintf(write_buffer, buffer_size, "REG_SENSOR %d %s %s", source_id, "HUMIDITY", "rf_weather");
    sendto(socket_fd, write_buffer, nbytes, 0, (struct sockaddr *) &server_address, sizeof(struct sockaddr_un));

    bytes_received = recvfrom(socket_fd, receive_buffer, buffer_size, 0, (struct sockaddr *) &(server_address), &address_length);
    receive_buffer[bytes_received] = '\0';

    humidity_sensor_id = atoi(receive_buffer);

    std::cout << "asgard:rf: remote humidity sensor: " << temperature_sensor_id << std::endl;

    // Register the button actuator
    nbytes = snprintf(write_buffer, buffer_size, "REG_ACTUATOR %d %s", source_id, "rf_button_1");
    sendto(socket_fd, write_buffer, nbytes, 0, (struct sockaddr *) &server_address, sizeof(struct sockaddr_un));

    bytes_received = recvfrom(socket_fd, receive_buffer, buffer_size, 0, (struct sockaddr *) &(server_address), &address_length);
    receive_buffer[bytes_received] = '\0';

    button_actuator_id = atoi(receive_buffer);

    std::cout << "asgard:rf: remote button actuator: " << button_actuator_id << std::endl;

    //wait for events
    while(true) {
        read_data(rc_switch);

        //wait for 10 time units
        delay(10);
    }

    //Close the socket
    close(socket_fd);

    return 0;
}
