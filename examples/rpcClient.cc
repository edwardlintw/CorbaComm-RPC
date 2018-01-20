#include <iostream>
#include <string>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <thread>
#include <corbaComm/corbaComm.h>
#include "cmd_event_def.h"

int main(int argc, char* argv[])
{
    cc::CorbaComm* cc = 
    cc::CorbaComm::connect("rpcClient",               // unique host Id
                           { },                       // not a command provider
                           {cmdGetData, cmdSayHello}, // but want these cmds
                           argc, argv);

    std::srand((unsigned)time(nullptr));
    char        buf[80];
    std::string returnData;
    const char* names[] = { "Alice", "Bob", "Charles", "Daniel", "Edward" };
    while (1) {
        std::this_thread::sleep_for(std::chrono::seconds(2));

        int reqSize = rand()%100000;
        std::sprintf(buf, "%d", reqSize);
        returnData = cc->execCmd(cmdGetData, buf);
        if (returnData == "")
            std::cout << "Command can't be routed to command provider.\n";
        else
            std::cout << "request size: " << reqSize 
                      << ", return size: " << returnData.length() << std::endl;

        cc->execCmd(cmdSayHello,
                    names[rand() % (sizeof names / sizeof names[0])]);
    }

    return 0;
}
