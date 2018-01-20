#include <iostream>
#include <cstring>
#include <cstdlib>
#include <thread>
#include <chrono>
#include <corbaComm/corbaComm.h>
#include <shared_mutex>
#include "cmd_event_def.h"

cc::CorbaComm* _cc = nullptr;
std::shared_timed_mutex _mutex;
std::string _temperature = "0";

std::string readCallback(const std::string& cmd, const std::string& param)
{
    std::shared_lock<std::shared_timed_mutex> lock(_mutex);
    return _temperature;
}

std::string writeCallback(const std::string& cmd, const std::string& param)
{
    std::unique_lock<std::shared_timed_mutex> lock(_mutex);
    _temperature = param;
    std::cout << "set temperature: " << _temperature << std::endl;
    return "OK";
}

int main(int argc, char* argv[])
{
    _cc = 
    cc::CorbaComm::connect("rpcServer",  // unique host Id
                           { },          // late command routing
                           { },
                           argc, argv);

    _cc->onCmd(cmdReadTemperature, &readCallback);
    _cc->onCmd(cmdWriteTemperature, &writeCallback);


    while (1) 
        std::this_thread::sleep_for(std::chrono::seconds(10));

    return 0;
};
