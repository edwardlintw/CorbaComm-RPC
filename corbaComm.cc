#include <string>
#include "corbaComm.h"
#include "corbaComm_impl.h"

cc::CorbaComm*     cc::CorbaComm::_ccserver = nullptr;
cc::CorbaCommImpl* cc::CorbaComm::_impl     = nullptr;

cc::CorbaComm::~CorbaComm() 
{
    delete cc::CorbaComm::_impl;
}

// static
cc::CorbaComm* cc::CorbaComm::connect(const char* hostId,
                                      cc::Commands offerCommands,
                                      cc::Commands wantCommands,
                                      int argc, 
                                      char* argv[]) 
{
    if (nullptr == _ccserver) {
        cc::CorbaComm::_ccserver = new cc::CorbaComm(hostId,
                                                     offerCommands,
                                                     wantCommands,
                                                     argc, argv);
    }
    return cc::CorbaComm::_ccserver;
}

cc::SID cc::CorbaComm::onEvent(const char* topic,
                               cc::EventCallback_t callback)
{
    return cc::CorbaComm::_impl->onEvent(topic, callback);
}

void cc::CorbaComm::detachEvent(const cc::SID& sid)
{
    cc::CorbaComm::_impl->detachEvent(sid);
}

bool cc::CorbaComm::pushEvent(const char* topic, 
                              const char* param)
{
    return cc::CorbaComm::_impl->pushEvent(topic, param);
}

std::string cc::CorbaComm::execCmd(const char* cmd, const char* param)
{
    return cc::CorbaComm::_impl->execCmd(cmd, param);
}

void cc::CorbaComm::onCmd(const char* cmd,
                          cc::CommandCallback_t cmdCallback)
{
    cc::CorbaComm::_impl->onCmd(cmd, cmdCallback);
}

//private
cc::CorbaComm::CorbaComm(const char* hostId,
                         cc::Commands offerCommands,
                         cc::Commands wantCommands,
                         int argc, char* argv[]) 
{
    cc::CorbaComm::_impl = 
    new cc::CorbaCommImpl(hostId,
                          offerCommands,
                          wantCommands,
                          argc, argv);
}


