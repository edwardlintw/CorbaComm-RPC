#ifndef _CORBA_COMM_IMPL_H
#define _CORBA_COMM_IMPL_H
#include <map>
#include <utility>
#include <vector>
#include <array>
#include <mutex>
#include <condition_variable>
#include <string.h>
#include "corbaComm.hh"
#include "corbaComm.h"
#include "cos.h"
#include "notify_impl.h"
#include "provider.h"

namespace cc {

class CorbaCommImpl {
public:
    typedef std::pair<std::string, std::string> Cmd2ProviderInfo;
    struct SupplierFailureException { };
    struct ConsumerFailureException { };
    struct CorbaObjectImplFailure   { };
    ~CorbaCommImpl();
    void tryDispatchEvent(const CosN::StructuredEvent&) const;
    void trySetProviderInfo(const CosN::StructuredEvent&);
    void trySetProviderInfo(const Cmd2ProviderInfo&);
    void tryPublishOfferService(const CosN::StructuredEvent&) const;

    // Big-5 rules
    CorbaCommImpl() = delete;
    CorbaCommImpl(const CorbaCommImpl&) = delete;
    CorbaCommImpl(CorbaCommImpl&&) = delete;
    CorbaCommImpl& operator=(const CorbaCommImpl&) = delete;
    CorbaCommImpl& operator=(CorbaCommImpl&&) = delete;

private:
    friend class CorbaComm;
    typedef std::pair<std::string, std::string> Filter;
    typedef std::array<Filter,2>                Filters;
    //  CorbaCommImpl
    //
    CorbaCommImpl(const char* hostId,
                  Commands    offerCommands,
                  Commands    wantCommands,
                  int argc, char* argv[]);
    SID  onEvent(const char* topic, EventCallback_t callback);
    void detachEvent(const SID&);
    bool pushEvent(const char* topic, const char* param) const;
    bool pushEvent(const char* topic, const char* param, 
                   const Filters& filters) const;
    std::string execCmd(const char* cmd, const char* param);
    void onCmd(const char* cmd, CommandCallback_t cmdCallback);

    SID  genSID() const;
    void unblockedCmd(const std::string&);
    void clearObjReference(const std::string&);

    // CORBA
    //
    void newProviderCorbaObject();
    bool initPushConsumer();
    bool initPushSupplier();
    bool initProviderImpl(const CosNaming::Name&);
    bool bindObjectToName(const CosNaming::Name&, CORBA::Object_ptr);
    CosNCA::EventChannel_ptr getOrCreateChannel();
    CORBA::Object_ptr resolveObjectReference(const CosNaming::Name&) const;
    
    // CorbaCommImpl
    //
    void publishCommandsType(const Commands&, std::string) const;
    void publishOfferCommands(const Commands&) const;
    void publishWantCommands(const Commands&) const;

    typedef std::vector<EventCallback_t>             AllCallbacks;
    typedef std::map<std::string, AllCallbacks>      SubscribeMap;
    typedef std::pair<std::string, EventCallback_t>  EvtInfo;
    typedef std::map<std::string, EvtInfo>           EvtInfoMap;
    typedef std::map<std::string, CommandCallback_t> ProviderMap;

    // for host which wants to understand who is request provider
    //
    typedef std::map<std::string, std::string>  ProviderInfoMap;
    typedef std::map<std::string, CorbaCommModule::Provider_ptr> ObjRefMap;

    std::string     _hostId;
    Commands        _offerCommands;
    Commands        _wantCommands;
    SubscribeMap    _subscribeMap;
    EvtInfoMap      _evtInfoMap;
    ProviderMap     _providerMap;
    ProviderInfoMap _providerInfoMap;
    ObjRefMap       _objRefMap;

    // CORBA
    //
    PushSupplier_i*                           _pushSupplier;
    PushConsumer_i*                           _pushConsumer;
    CORBA::ORB_var                            _orb;
    PortableServer::POA_var                   _poa;
    CosNaming::NamingContext_var              _nameCtx;
    PortableServer::Servant_var<ProviderImpl> _providerImpl;
    bool                                      _orbRunning;

    // for 'lazy command routing' sync
    //
    struct SyncObj {
        std::unique_ptr<std::mutex>              _mutex;
        std::unique_ptr<std::condition_variable> _cv;
        bool                                     _cmdReady;
        SyncObj() {
            _mutex    = std::make_unique<std::mutex>();
            _cv       = std::make_unique<std::condition_variable>();
            _cmdReady = false;
        }
    };
    std::map<std::string, SyncObj>  _syncMap;

    const std::string _channelName = "EventChannel";
    const std::string _factoryName = "ChannelFactory";
};

};  // namespace cc

#endif

