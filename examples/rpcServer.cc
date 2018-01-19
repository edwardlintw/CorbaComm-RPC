#include <iostream>
#include <cstring>
#include <cstdlib>
#include <thread>
#include <chrono>
#include <corbaComm/corbaComm.h>
#include "cmd_event_def.h"

cc::CorbaComm* _cc = nullptr;

std::string commandCallback(const std::string& cmd, const std::string& param)
{
    std::cout << "Hello, " << param << std::endl;
    return "OK";
}

int main(int argc, char* argv[])
{
    _cc = 
    cc::CorbaComm::connect("rpcServer",               // unique host Id
                           {cmdGetData, cmdSayHello}, // what rpcServer offers
                           { },                       // but want nothing
                           argc, argv);

    if (nullptr == _cc) {
        std::cerr << "Can't connect to CorbaComm.\n";
        return -1;
    }

    // use tradition callback
    //
    _cc->onCmd(cmdSayHello, &commandCallback);

    // or via C++11 lambda
    //
    _cc->onCmd(cmdGetData, 
               [](const std::string& cmd, const std::string& event) {
                   unsigned long len = std::strtoul(event.c_str(), nullptr, 10);
                   if (len > 100000UL)
                       len = 100000UL;
                   if (len < 100UL)
                       len = 100;
                   return std::string(len,'0');
               });

    while (1) 
        std::this_thread::sleep_for(std::chrono::seconds(10));

    return 0;
};
