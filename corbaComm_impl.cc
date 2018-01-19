#include <map>
#include <string>
#include <thread>
#include <vector>
#include <iostream>
#include <algorithm>
#include <array>
#include <chrono>
#include <sstream>
#include <cstring>
#include "corbaComm_impl.h"
#include "corbaComm.hh"
#include "cos.h"
#include "notify_impl.h"
#include "provider.h"
#include <omniORB4/omniZIOP.h>

static cc::CorbaCommImpl*  _impl;

static void consumeCallback(const CosN::StructuredEvent& event)
{
    const char*  check = (const char*)event.filterable_data[1].name;
    
    if (0 == std::strcmp(check, "offer services")) {
        ::_impl->trySetProviderInfo(event);
    }
    else if (0 == std::strcmp(check, "want services")) {
        ::_impl->tryPublishOfferService(event);
    }
    else {
        ::_impl->tryDispatchEvent(event);
    }
}

// command provider will call this special command, cmd == _hostId
// command requester to set provider info according to this command
//
static std::string cmdProviderResponse(const std::string& cmd,
                                       const std::string& param)
{
    std::istringstream strm(param);
    std::string command;
    getline(strm, command, ';');
    std::string provider;
    getline(strm, provider, ':');
    cc::CorbaCommImpl::Cmd2ProviderInfo info = {command, provider};
    ::_impl->trySetProviderInfo(info);
    return "";
}

cc::CorbaCommImpl::CorbaCommImpl(const char*  hostId,
                                 cc::Commands offerCommands,
                                 cc::Commands wantCommands,
                                 int argc, char* argv[]) 
                  : _pushSupplier{nullptr}
                  , _pushConsumer{nullptr}
                  , _orb{CORBA::ORB::_nil()}
                  , _poa{PortableServer::POA::_nil()}
                  , _nameCtx{CosNaming::NamingContext::_nil()} 
                  , _orbRunning{false}
{
    _hostId        = hostId;
    _offerCommands = offerCommands;
    _wantCommands  = wantCommands;

    // * * * * * * * * N O T E * * * * * * * *
    //
    // this ctor will throw exception
    // it means the host can't initialize CORBA enviroment,
    // and it never works
    // 
    // throw exception to crash the host process 
    // developers must figure out the root cause and fix any errors
    //
    // * * * * * * * * N O T E * * * * * * * *
    
    try {
        const char* options[][2] = {
            { "maxGIOPConnectionPerServer", "50" },
            { (const char*)0, (const char*)0 },
        };
        CORBA::PolicyList pl;
        omniZIOP::setGlobalPolicies(pl);
        _orb = CORBA::ORB_init(argc, argv, "omniORB4", options);
        CORBA::Object_var obj; 
        obj = _orb->resolve_initial_references("RootPOA");
        _poa = PortableServer::POA::_narrow(obj);

        obj = _orb->resolve_initial_references("NameService");
        _nameCtx = CosNaming::NamingContext::_narrow(obj);

        if (!initPushSupplier()) 
            throw cc::CorbaCommImpl::SupplierFailureException();

        if (!initPushConsumer())
            throw cc::CorbaCommImpl::ConsumerFailureException();

        // every app is a 'command provider', and offer a special command
        // which command name is identical to _hostId.
        // this command is to receive provider's response
        newProviderCorbaObject();
        onCmd(_hostId.c_str(), cmdProviderResponse); 
        if (_offerCommands.size() > 0 ) 
            publishOfferCommands(offerCommands);

        if (wantCommands.size() > 0)
            publishWantCommands(wantCommands);

        ::_impl = this;
    }
    catch (CORBA::TRANSIENT& ex) {
        std::cerr << "Caught CORBA::TRANSIENT, can't contact Name Service\n";
        throw ex;
    }
    catch (CORBA::SystemException& ex) {
        std::cerr << "Caught CORBA::SystemException: " << ex._name() << "\n";
        throw;
    }
    catch (CORBA::Exception& ex) {
        std::cerr << "Caught CORBA::Exception: " << ex._name() << "\n";
        throw;
    }
    catch (SupplierFailureException& ex) {
        std::cerr << "Can't initial push supplier.\n";
        throw ex;
    }
    catch (CorbaObjectImplFailure& ex) {
        std::cerr << "Can't create Provider CORBA object.\n";
        throw ex;
    }
    catch (...) {
        std::cerr << "Caught unknown exception.\n"
                  << "The process can't work, please figure out the reason.\n";
        // re-throw to crash the host process
        //
        throw;
    }
}

cc::CorbaCommImpl::~CorbaCommImpl() 
{
}

void cc::CorbaCommImpl::tryDispatchEvent(
                             const CosN::StructuredEvent& event) const
{
    const char*             param;
    const char*             ev;

    event.filterable_data[1].value >>= ev;
    event.remainder_of_body >>= param;

    auto itr = _subscribeMap.find(ev);
    if (itr != _subscribeMap.end()) {
        auto allCallbacks = itr->second;
        for (auto callback: allCallbacks) 
            (*callback)(ev, param); 
    }
}

void cc::CorbaCommImpl::trySetProviderInfo(const CosN::StructuredEvent& event)
{
    if (_wantCommands.size() > 0 ) {
        const char* cmd;
        event.filterable_data[1].value >>= cmd;
        if (std::find(_wantCommands.begin(), _wantCommands.end(), cmd)
            != _wantCommands.end())
        {
            const char* provider;
            event.filterable_data[0].value >>= provider;
            _providerInfoMap[cmd] = std::string(provider);
            clearObjReference(provider);
        }
        unblockedCmd(cmd);
    }
}

void cc::CorbaCommImpl::trySetProviderInfo(
                            const cc::CorbaCommImpl::Cmd2ProviderInfo& info)
{
    if (_wantCommands.size() > 0) {
        if (std::find(_wantCommands.begin(), _wantCommands.end(), info.first) 
            != _wantCommands.end()) 
        {
            _providerInfoMap[info.first] = info.second;
        }
        unblockedCmd(info.first);
    }
}

void cc::CorbaCommImpl::tryPublishOfferService(
                            const CosN::StructuredEvent& event) const
{
    if (_offerCommands.size() > 0) {
        const char* querier;
        event.filterable_data[0].value >>= querier;
        const char* cmd;
        event.filterable_data[1].value >>= cmd;
        auto itr = std::find(std::begin(_offerCommands), 
                             std::end(_offerCommands), cmd);

        // the provider will 'execCmd' to notify command requester
        //
        if (itr != std::end(_offerCommands)) {
            CosNaming::Name name;
            name.length(2);
            name[0].id   = "edwardlintw";
            name[0].kind = "com";
            name[1].id   = querier;
            name[1].kind = "provider";

            try {
                CORBA::Object_var obj          = resolveObjectReference(name);
                CorbaCommModule::Provider_ptr  providerRef  = 
                CorbaCommModule::Provider::_narrow(obj);
                std::string param;
                param.append(cmd).append(";").append(_hostId);
                providerRef->execCmd(querier, param.c_str());
            }
            catch (...) {
                ;
            }
        }
    }
}

cc::SID cc::CorbaCommImpl::onEvent(const char* topic,
                                   cc::EventCallback_t callback) 
{
    if (nullptr == topic|| 0 == std::strcmp(topic,""))
        return "";
    bool ok = false;
    auto which = _subscribeMap.find(topic);
    if (which == _subscribeMap.end()) {
        _subscribeMap[topic] = {callback};
        ok = true;
    }
    else {
        auto all = which->second;
        auto itr = std::find(std::begin(all), std::end(all), callback);
        if (itr == std::end(all)) {
            which->second.push_back(callback);
            ok = true;
        }
    }
    if (ok) {
        auto sid = genSID();
        _evtInfoMap[sid] = {topic, callback};
        return sid;
    }
    else {
        return "";
    }
}

void cc::CorbaCommImpl::detachEvent(const SID& sid)
{
    auto evtInfoItr = _evtInfoMap.find(sid);
    if (evtInfoItr != _evtInfoMap.end()) {
        auto topic= evtInfoItr->second.first;
        auto callback = evtInfoItr->second.second;
        auto which = _subscribeMap.find(topic);
        if (which != _subscribeMap.end()) {
            auto& callbacks = which->second;
            which->second.erase(
            std::remove(std::begin(callbacks), std::end(callbacks), callback));
        } 
        _evtInfoMap.erase(evtInfoItr);
    }
}

bool cc::CorbaCommImpl::pushEvent(const char* topic, 
                                  const char* param) const
{
    cc::CorbaCommImpl::Filters filters = {{
        std::make_pair(std::string("sender"),  _hostId),
        std::make_pair(std::string("command"), std::string(topic))
    }};
    // bridge to the other overloading 'pushEvent'
    //
    return pushEvent(topic, param, filters);
}

bool cc::CorbaCommImpl::pushEvent(
                           const char* topic, 
                           const char* param,
                           const cc::CorbaCommImpl::Filters& filters) const
{
    CosN::StructuredEvent* ev = new CosN::StructuredEvent;
    try {

        // setup event header (for filtering)
        //
        ev->header.fixed_header.event_type.domain_name = "";
        ev->header.fixed_header.event_type.type_name   = "";
        ev->header.variable_header.length(0);
        ev->filterable_data.length(filters.size());
        size_t  i = 0;
        for (const auto& filter : filters) {
            ev->filterable_data[i].name    = filter.first.c_str();
            ev->filterable_data[i].value <<= filter.second.c_str();
            ++i;
        }

        ev->remainder_of_body <<= param;
        _pushSupplier->push(*ev);

        delete ev;
        return true;
    }
    catch (...) {
        std::cerr << "send failure\n";
        delete ev;
        return false;
    }
}

std::string cc::CorbaCommImpl::execCmd(const char* cmd,
                                      const char* param)
{
    // lookup who is provider
    //
re_run:
    auto whois = _providerInfoMap.find(cmd);
    if (whois == _providerInfoMap.end()) {
        _wantCommands.push_back(cmd);

        auto itr = _syncMap.find(cmd);
        if (itr == _syncMap.end()) {
            _syncMap[cmd] = SyncObj();
            itr = _syncMap.find(cmd);
        }

        std::unique_lock<std::mutex>    lock(*itr->second._mutex);
        itr->second._cmdReady = false;

        std::thread ([this,cmd]() {
                        publishWantCommands({cmd}); 
                    }).detach();

        using namespace std::chrono_literals;
        itr->second._cv->wait_for(lock, 100ms, 
                                  [this,itr]() { 
                                      return itr->second._cmdReady; 
                                  });
        if (false == itr->second._cmdReady)
            return "";
        else {
            goto re_run;
        }
    }

    // lookup Provider's Object reference
    //
    std::string provider = whois->second;
    auto which = _objRefMap.find(provider);
    if (which != _objRefMap.end()) {
        try {
            CORBA::String_var ret;
            std::string       result;
            ret = 
            which->second->execCmd(cmd, param);

            result = (const char*)ret;
            return result;
        }
        catch (... ) {
            // can't reach target host (maybe host is down)
            //
            _objRefMap.erase(which);
            return "";
        }
    }
    else {
        CosNaming::Name name;
        name.length(2);
        name[0].id   = "edwardlintw";
        name[0].kind = "com";
        name[1].id   = provider.c_str();
        name[1].kind = "provider";

        CORBA::Object_var obj          = resolveObjectReference(name);
        CorbaCommModule::Provider_ptr  providerRef  = 
        CorbaCommModule::Provider::_narrow(obj);

        std::string result;
        try {
            CORBA::String_var   ret;
            ret = providerRef->execCmd(cmd, param);
            result = (const char*)ret;
            _objRefMap[provider] = providerRef;
        }
        catch (...) {
            result = "";
        }
        return result;
    }
}

void cc::CorbaCommImpl::onCmd(const char* cmd,
                              cc::CommandCallback_t func) 
{
    if (_hostId != cmd 
        &&
        std::find(std::begin(_offerCommands), 
                  std::end(_offerCommands), cmd) == std::end(_offerCommands)
       ) 
    {
        publishOfferCommands({cmd});
        _offerCommands.push_back(cmd);
    }

    auto which = _providerMap.find(cmd);
    if (which != _providerMap.end()) {
        // call more than once 'offerRequest' with the same 'cmd'
        // do nothing
        //
        return;
    }

    _providerMap[cmd] = func;
    _providerImpl->onCmd(cmd, func);
}

cc::SID cc::CorbaCommImpl::genSID() const
{
    using namespace std::chrono;
    auto now = high_resolution_clock::now();
    auto ns  = duration_cast<nanoseconds>(now.time_since_epoch()).count();
    std::stringstream ostrm;
    ostrm << ns;
    return ostrm.str();
}

void cc::CorbaCommImpl::unblockedCmd(const std::string& cmd)
{
    auto itr = _syncMap.find(cmd);
    if (itr != _syncMap.end())
    {
        std::lock_guard<std::mutex> lock(*itr->second._mutex);
        itr->second._cmdReady = true;
        itr->second._cv->notify_all();
    }
}

void cc::CorbaCommImpl::clearObjReference(const std::string& provider)
{
    auto itr = _objRefMap.find(provider);
    if (itr != _objRefMap.end())
        _objRefMap.erase(itr);
}

void cc::CorbaCommImpl::newProviderCorbaObject()
{
    CosNaming::Name name;
    name.length(2);
    name[0].id   = "edwardlintw";
    name[0].kind = "com";
    name[1].id   = _hostId.c_str();
    name[1].kind = "provider";

    // implement CORBA Object 'Provider'
    // to provide request service
    //
    if (!initProviderImpl(name))
        throw cc::CorbaCommImpl::CorbaObjectImplFailure();
}

bool cc::CorbaCommImpl::initPushConsumer()
{
    CosNCA::EventChannel_ptr        channel;

    channel = getOrCreateChannel();
    if (CORBA::is_nil(channel)) {
        std::cerr << "Can't create event channel.\n";
        return false;
    }

    try {
        CosN::EventTypeSeq  evs;
        evs.length(0);

        char  constraint[80];       // filter constraint
        std::sprintf(constraint, "$sender != '%s'", _hostId.c_str());

        _pushConsumer = 
        PushConsumer_i::create(_orb, channel, "Push Consumer", consumeCallback,
                               nullptr, &evs, constraint);

        if (!_pushConsumer) {
            std::cerr << "Can't construct push consumer.\n";
            return false;
        }

        CosNC::StructuredPushConsumer_var 
        pushConsumerRef = _pushConsumer->_this();

        _pushConsumer->_remove_ref();
        _pushConsumer->connect();

        if (!_orbRunning) {
            PortableServer::POAManager_var pman = _poa->the_POAManager();
            pman->activate();

            // since ORB::run() will block execution
            // make ORB::run() in detached thread
            // or this methond won't return
            //
            std::thread([&]() { _orb->run(); }).detach();
            _orbRunning = true;
        }
        return true;
    }
    catch (...) {
        // TODO: more concrete info here
        //
        std::cerr << "Catch unknown exception\n";
        return false;
    }
}

bool cc::CorbaCommImpl::initPushSupplier()
{
    CosNCA::EventChannel_ptr        channel;

    channel = getOrCreateChannel();
    if (CORBA::is_nil(channel)) {
        std::cerr << "Can't create event channel.\n";
        return false;
    }

    try {
        CosN::EventTypeSeq  evs;
        evs.length(0);
        
        _pushSupplier = 
        PushSupplier_i::create(_orb, channel, "Push Supplier",
                               nullptr, &evs, nullptr);

        if (!_pushSupplier) {
            std::cerr << "Can't construct push supplier.\n";
            return false;
        }

        CosNC::StructuredPushSupplier_var 
        pushSupplierRef = _pushSupplier->_this();

        _pushSupplier->_remove_ref();
        _pushSupplier->connect();

        if (!_orbRunning) {
            PortableServer::POAManager_var pman = _poa->the_POAManager();
            pman->activate();

            // since ORB::run() will block execution
            // make ORB::run() in detached thread
            // or this methond won't return
            //
            std::thread([&]() { _orb->run(); }).detach();
            _orbRunning = true;
        }
        return true;
    }
    catch (...) {
        // TODO: more concrete info here
        //
        std::cerr << "Catch unknown exception\n";
        return false;
    }
}

bool cc::CorbaCommImpl::initProviderImpl(const CosNaming::Name& name)
{
    try {
        Compression::CompressorIdLevelList ids;
        ids.length(1);
        ids[0].compressor_id = Compression::COMPRESSORID_ZLIB;
        ids[0].compression_level = 6;

        CORBA::PolicyList pl;
        pl.length(2);
        pl[0] = omniZIOP::create_compression_enabling_policy(1);
        pl[1] = omniZIOP::create_compression_id_level_list_policy(ids);

        PortableServer::POAManager_var pman = _poa->the_POAManager();
        PortableServer::POA_var poa = 
        _poa->create_POA("Custom POA", pman, pl);

        _providerImpl = new ProviderImpl();
        PortableServer::ObjectId_var 
        providerId = poa->activate_object(_providerImpl);
        CORBA::Object_var obj = _providerImpl->_this();
        
        return bindObjectToName(name, obj);
    }
    catch (CORBA::SystemException& ex) {
        std::cerr << "Caught CORBA::SystemException: " << ex._name() << ".\n";
        return false;
    }
    catch (CORBA::Exception& ex) {
        std::cerr << "Caught Exception: " << ex._name() << ".\n";
        return false;
    }
    catch (...) {
        std::cerr << "Caught unknown exception.\n";
        return false;
    }
}

bool cc::CorbaCommImpl::bindObjectToName(const CosNaming::Name& objName,
                                         CORBA::Object_ptr obj)
{
    try {
        CosNaming::NamingContext_var  ctx = _nameCtx;
        CosNaming::Name               ctxName;
        auto                          length = objName.length();

        // from the beginning to last 2 (id,kind) pair are context
        //
        for (CORBA::ULong i = 0; i < length-1; ++i) {
            try {
                ctxName.length(1);
                ctxName[0].id   = objName[i].id;
                ctxName[0].kind = objName[i].kind;
                ctx = ctx->bind_new_context(ctxName);
            }
            catch (CosNaming::NamingContext::AlreadyBound& ex) {
                // this it is a context (already exists)
                // resolve and use the old one
                //
                CORBA::Object_var obj = ctx->resolve(ctxName);
                ctx = CosNaming::NamingContext::_narrow(obj);
                if (CORBA::is_nil(ctx)) {
                    std::cerr << "Failed to bind name context\n";
                    return 0;
                }
            }
        }   
        // the last 1 (id,kind) pair is a object
        //
        try {
            ctxName.length(1);
            ctxName[0].id   = objName[length-1].id;
            ctxName[0].kind = objName[length-1].kind;
            ctx->bind(ctxName, obj);
        }
        catch (CosNaming::NamingContext::AlreadyBound& ex) {
            // already exists, bind new one (discard old one)
            // this approach is good for 
            // either 'transient' or 'persistant' server objects
            //
            ctx->rebind(ctxName, obj);
        }
    }
    catch (CORBA::TRANSIENT& ex) {
        std::cerr << "Caught CORAB::TRANSIENT, can't contact Name Server\n";
        return 0;
    }
    catch (CORBA::SystemException& ex) {
        std::cerr << "Caught CORBA::SystemException: " 
                  << ex._name() << std::endl;
        return 0;
    }
    catch (...) {
        std::cerr << "Caught unknown exception\n";
        return 0;
    }
    return 1;
}

CosNCA::EventChannel_ptr cc::CorbaCommImpl::getOrCreateChannel()
{
    CosNCA::EventChannel_ptr channel = CosNCA::EventChannel::_nil();
    CosNaming::Name          name;

    // resolve from Name Server
    //
    name.length(1);
    name[0].id   = _channelName.c_str();
    name[0].kind = _channelName.c_str();

    try {
        CORBA::Object_var   obj = _nameCtx->resolve(name);

        channel = CosNCA::EventChannel::_narrow(obj);
    }
    catch (...) {
        // just do nothing, try creating EventChannel later
        //
    }

    // already exists
    //
    if (!CORBA::is_nil(channel))
        return channel;

    // try to create a new one
    //
    CosNCA::EventChannelFactory_ptr factory; 

    name.length(1);
    name[0].id   = _factoryName.c_str();
    name[0].kind = _factoryName.c_str();

    try {
        CORBA::Object_var obj = _nameCtx->resolve(name);
        factory = CosNCA::EventChannelFactory::_narrow(obj);
    }
    catch (...) {
        factory = CosNCA::EventChannelFactory::_nil();
    }

    if (CORBA::is_nil(factory)) {
        return channel;
    }

    CosNCA::ChannelID   channelId;
    try {
        CosN::QoSProperties   qosProp;
        CosN::AdminProperties adminProp;

        qosProp.length(0);
        adminProp.length(0);

        channel = factory->create_channel(qosProp, adminProp, channelId);
    }
    catch (...) {
        // TODO: more concrete info
        //
    }

    return channel;
}

CORBA::Object_ptr cc::CorbaCommImpl::resolveObjectReference(
                            const CosNaming::Name& name) const
{
    try {
        return _nameCtx->resolve(name);
    }
    catch (CosNaming::NamingContext::NotFound&) {
        std::cerr << "Context not found\n";
    }
    catch (CORBA::TRANSIENT&) {
        std::cerr << "Caught CORBA::TRANSIENT, unable to contact NameService\n";
    }
    catch (CORBA::SystemException& ex) {
        std::cerr << "Caught CORBA::SystemException: " << ex._name() << "\n";
    }
    catch (...) {
        std::cerr << "Caught unknown exception\n";
    }
    return CORBA::Object::_nil();
}

void cc::CorbaCommImpl::publishCommandsType(const cc::Commands& services,
                                            std::string type) const
{
    for (const auto& cmd : services) {
        cc::CorbaCommImpl::Filters filters = {{
            std::make_pair(std::string("sender"), _hostId),
            std::make_pair(type, cmd)
        }};
        pushEvent("-1", "", filters);
    }
}

void cc::CorbaCommImpl::publishOfferCommands(
                             const cc::Commands& services) const
{
    publishCommandsType(services, "offer services");
}

void cc::CorbaCommImpl::publishWantCommands(
                             const cc::Commands& services) const
{
    publishCommandsType(services, "want services");
}

