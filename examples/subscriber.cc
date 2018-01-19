#include <iostream>
#include <chrono>
#include <thread>
#include <string>
#include <corbaComm/corbaComm.h>
#include "cmd_event_def.h"

cc::CorbaComm* _cc = nullptr;

void eventCallback(const std::string&, const std::string& param)
{
    std::cout << "current humidity: " << param << "%" << std::endl;
}

int main(int argc, char* argv[]) 
{
    _cc = 
    cc::CorbaComm::connect("subscriber",
                           { }, { },            // both empty
                           argc, argv);

    if (nullptr == _cc) {
        std::cerr << "Can't connect to CorbaComm.\n";
        return -1;
    }

    _cc->onEvent(topicHumidity, &eventCallback);
    _cc->onEvent(topicTemperature,
                 [](const std::string&, const std::string& param) {
                     std::cout << "current temperature: " << param << std::endl;
                 });

    while (1) {
        std::this_thread::sleep_for(std::chrono::seconds(10));
    }

    return 0;
}
