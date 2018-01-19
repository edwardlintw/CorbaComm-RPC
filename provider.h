#ifndef _PROVIDER_H
#define _PROVIDER_H
#include <map>
#include <string>
#include "corbaComm.hh"
#include "corbaComm.h"

class ProviderImpl: public POA_CorbaCommModule::Provider
{
public:
    ProviderImpl() { }
    virtual ~ProviderImpl() { }
    ProviderImpl(const ProviderImpl&) = delete;
    ProviderImpl(ProviderImpl&&) = delete;
    ProviderImpl& operator=(const ProviderImpl&) = delete;
    ProviderImpl& operator=(ProviderImpl&&) = delete;

public:
    // interface method(s)
    //
    char* execCmd(const char* cmd, const char* inData);

    // class ProviderImpl's method(s)
    //
    void onCmd(const char* cmd, 
               cc::CommandCallback_t cmdCallback);

private:
    typedef std::map<std::string, cc::CommandCallback_t> ProviderMap;
    ProviderMap _providerMap;
};
#endif
