#include <iostream>
#include <string>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <thread>
#include <corbaComm/corbaComm.h>
#include "cmd_event_def.h"

std::string getTemperature()
{
    char buf[80];
    sprintf(buf, "%d.%d", 30+rand()%10, rand()%10);
    return buf;
}

int main(int argc, char* argv[])
{
    cc::CorbaComm* cc = 
    cc::CorbaComm::connect("rwClient",  // unique host Id
                           { },         // late command routing
                           { },
                           argc, argv);

    std::string temperature;
    temperature = getTemperature();
    cc->execCmd(cmdWriteTemperature, temperature.c_str());
    while (1) {
        std::this_thread::sleep_for(std::chrono::seconds(2));
        if (std::rand() % 2 == 0) {
            temperature = getTemperature();
            std::cout << "set temperature " << temperature << " to server\n";
            cc->execCmd(cmdWriteTemperature, temperature.c_str());
        }
        else
            std::cout << "temperature: " << cc->execCmd(cmdReadTemperature, "")
                      << std::endl;
    }

    return 0;
}
