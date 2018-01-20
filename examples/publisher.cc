#include <iostream>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <thread>
#include <string>
#include <corbaComm/corbaComm.h>
#include "cmd_event_def.h"

std::string getTemperature()
{
    char buf[80];
    sprintf(buf, "%d.%d", 30+rand()%10, rand()%10);
    return buf;
}

std::string getHumidity()
{
    char buf[80];
    sprintf(buf, "%d.%d", 40+rand()%10, rand()%10);
    return buf;
}

int main(int argc, char* argv[]) 
{
    cc::CorbaComm* cc = 
    cc::CorbaComm::connect("publisher", 
                           { }, { },            // both empty
                           argc, argv);

    while (1) {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        if (std::rand() % 2 == 0)
            cc->pushEvent(topicTemperature, getTemperature().c_str());
        else
            cc->pushEvent(topicHumidity, getHumidity().c_str());
    }

    return 0;
}
