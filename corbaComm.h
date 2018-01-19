#ifndef _CORBA_COMM_H
#define _CORBA_COMM_H

#include <string>
#include <vector>

namespace cc {

// for Command Provider only
// when clients invoke a 'execCmd()', 
// command provider's callback will be invoked to provide data
//
typedef std::string (*CommandCallback_t)(const std::string& cmd, 
                                         const std::string& param);

// for subscribers only
// when any publishers push an event, 
// the 'CorbaComm' will invoke subscriber's callback
//
typedef void (*EventCallback_t)(const std::string& topic, 
                                const std::string& param);

// for event publisher, no need to connect a new type (both share the same cmds)
//
typedef std::vector<std::string>   Commands;

// for onEvent() and detachEvent() methods
// an SID (Subscription ID, is an ID returned by onEvent()
// if the host would like to unscribe an event, 
// calls detachEvent() with this ID
//
// if the hosts call onEvent with the 
// same 'event' and 'callback' more than once
// an empty SID return
//
typedef std::string  SID;

class CorbaCommImpl;

class CorbaComm {
public:
    virtual ~CorbaComm();
    static CorbaComm* 
    connect(const char* hostId,    // the host id, such as 'main', 'gui', 'usb'
                                   // can't be nullptr or empty string
           Commands offerCommands, // commands the host offers, can be empty
           Commands wantCommands,  // commands the host wants, can be empty
           int argc, char* argv[]);// additional argc/argv
                                   // to init exchange server

    // for subscriber, subscriber's 'onEvent()' will be invoked 
    // when an event arrives
    //
    virtual SID onEvent(const char* topic, EventCallback_t callback);
    virtual void detachEvent(const SID&);

    // for publisher to push an event 'topic'
    //
    virtual bool pushEvent(const char* topic, const char* param);

    // for hosts which request data from the other host, or
    // for hosts which ask the host do do some action
    //
    virtual std::string execCmd(const char* cmd, const char* param);

    // for hosts which offer the command 'cmd'
    // when a client request a command by 'execCmd()'
    // command provider's 'onCmd()' will be called
    //
    virtual void onCmd(const char* cmd, CommandCallback_t cmdCallback); 

    // Big-5 rule
    //
    CorbaComm(const CorbaComm&) = delete;
    CorbaComm(CorbaComm&&) = delete;
    CorbaComm& operator=(const CorbaComm&) = delete;
    CorbaComm& operator=(CorbaComm&&) = delete;

protected:    
    CorbaComm() = default;

private:
    CorbaComm(const char* hostId, 
              Commands offerCommands, 
              Commands wantCommands,
              int argc, 
              char* argv[]);
    static CorbaComm*     _ccserver;
    static CorbaCommImpl* _impl;
};

};  // namespace cc

#endif


