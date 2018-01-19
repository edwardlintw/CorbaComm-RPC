#include <map>
#include <string>
#include "provider.h"

char* ProviderImpl::execCmd(const char* cmd, const char* inData)
{
    std::string             result;
    auto                    which = _providerMap.find(std::string(cmd));

    if (which != _providerMap.end() && nullptr != which->second)
        result = (*which->second)(cmd, inData);
    else
        result = "";

    return CORBA::string_dup(result.c_str());
}

void ProviderImpl::onCmd(const char* cmd, 
                       cc::CommandCallback_t cmdCallback)
{
    _providerMap[std::string(cmd)] = cmdCallback;
}
