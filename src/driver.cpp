//=======================================================================
// Copyright (c) 2015-2016 Baptiste Wicht
// Distributed under the terms of the MIT License.
// (See accompanying file LICENSE or copy at
//  http://opensource.org/licenses/MIT)
//=======================================================================

#include "asgard/driver.hpp"

#include "RCSwitch.h"

namespace {

// Configuration
std::vector<asgard::KeyValue> config;

// The driver connection
asgard::driver_connector driver;

// The remote IDs
int source_id = -1;
int temperature_sensor_id = -1;
int humidity_sensor_id = -1;
int button_actuator_id = -1;

void stop(){
    std::cout << "asgard:rf: stop the driver" << std::endl;

    asgard::unregister_sensor(driver, source_id, temperature_sensor_id);
    asgard::unregister_sensor(driver, source_id, humidity_sensor_id);
    asgard::unregister_actuator(driver, source_id, button_actuator_id);
    asgard::unregister_source(driver, source_id);

    // Unlink the client socket
    unlink(asgard::get_string_value(config, "rf_client_socket_path").c_str());

    // Close the socket
    close(driver.socket_fd);
}

void terminate(int){
    stop();

    std::exit(0);
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

    asgard::send_data(driver, source_id, temperature_sensor_id, temperature);
    asgard::send_data(driver, source_id, humidity_sensor_id, humidity);
}

void read_data(RCSwitch& rc_switch){
    if (rc_switch.available()) {
        int value = rc_switch.getReceivedValue();

        if (value) {
            if((rc_switch.getReceivedProtocol() == 1 || rc_switch.getReceivedProtocol() == 2) && rc_switch.getReceivedValue() == 1135920){
                //Send the event to the server
                asgard::send_event(driver, source_id, button_actuator_id, std::to_string(1));
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
    rc_switch.enableReceive(asgard::get_int_value(config, "rf_gpio_pin"));

    //Drop root privileges and run as pi:pi again
    if(!asgard::revoke_root()){
       std::cout << "asgard:rf: unable to revoke root privileges, exiting..." << std::endl;
       return 1;
    }
    
    // Load the configuration file
    asgard::load_config(config);

    // Open the connection
    if(!asgard::open_driver_connection(driver, asgard::get_string_value(config, "rf_client_socket_path").c_str(), asgard::get_string_value(config, "server_socket_path").c_str())){
        return 1;
    }

    //Register signals for "proper" shutdown
    signal(SIGTERM, terminate);
    signal(SIGINT, terminate);

    // Register the source and sensors
    source_id = asgard::register_source(driver, "rf");
    temperature_sensor_id = asgard::register_sensor(driver, source_id, "TEMPERATURE", "rf_weather");
    humidity_sensor_id = asgard::register_sensor(driver, source_id, "HUMIDITY", "rf_weather");
    button_actuator_id = asgard::register_actuator(driver, source_id, "rf_button");

    //wait for events
    while(true) {
        read_data(rc_switch);

        //wait for 10 time units
        delay(10);
    }

    stop();

    return 0;
}
