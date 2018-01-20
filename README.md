# CorbaComm, an RPC library based on CORBA by Leveraging omniORB and omniNotify

# Table of Contents
1. [Abstract](#1-abstract)    
   [What is CorbaComm](#what-is-corbacomm)   
   [About Corba](#about-corba)    
2. [Build Prerequisite](#2-build-prerequisite)
3. [How to Build CorbaComm](#3-how-to-build-corbacomm)
4. [C++ Class And Methods](#4-c-class-and-methods)
5. [Running Examples](#5-running-examples)
6. [Command Routing](#6-command-routing)   
   [Early Command Routing](#early-command-routing)   
   [Late Command Routing](#late-command-routing)   
7. [Multithreading Considerations](#7-multithreading-considerations)
8. [Distributed RPC](#8-distributed-rpc)

## 1. Abstract

### What is `CorbaComm`
`CorbaComm` is a C++ RPC library based on `CORBA` middleware; the intent is to hide complicated `CORBA` knowledge and details to make this library as easy as possible. The implementation fully depends on two `CORBA` implementations, [omniORB](http://omniorb.sourceforge.net/) and [omniNotify](http://omninotify.sourceforge.net/).

The `CorbaComm` library offers two types of communication model:

* **`Peer to Peer RPC Invocation`**
* **`Publish and Subscribe Messaging Model`**

#### Peer to Peer RPC Invocation

`Peer to Peer RPC Invocation` means to invoke a direct RPC-call. A client requests a command to a server directly for asking data, or ask the server to do something.   

#### Publish and Subscribe Messaging Model.

`Publish and Subscribe Messaging Model` is for a `publisher` to publish events (data, message) to any `subscribers` via `message queue`. Publishers are loosely coupled to subscribers, and need not even know of their existence. With the topic being the focus, publishers and subscribers are allowed to remain ignorant of system topology.   

For more information, please refer to [Publish-Subscribe Wiki](https://en.wikipedia.org/wiki/Publish%E2%80%93subscribe_pattern).

#### Quick CorbaComm Examples:

* **Initialize CorbaComm**   
The simplest way to use and inialize `CorbaComm` is a one-liner code:

```
#include <iostream>
#include <corbaComm/corbaComm.h>

int main(int argc, char* argv[])
{
    cc::CorbaComm* cc = 
    cc::CorbaComm::connect("hostId", { }, { }, argc, argv);
    ...
}
```

The above `"hostId"` is the unique `id`; each hosts(process) should have an `unique id` in the system.   
The 2nd and 3rd arguments are both empty, you can ignore these two now, detail usage and information will be in the later paragraph.   

* **Peer to Peer RPC Invocation**

**Client Example:**  

Read [rpcClient.cc](https://github.com/edwardlintw/CorbaComm-RPC/blob/master/examples/rpcClient.cc) for full example.

```
int main(int argc, char* argv[])
{
    cc::CorbaComm* cc = ....;

    cc->execCmd("sayHello", "Alice");
    ...
}
```
`"sayHello"` is string literal command; it is user-defined. This command will be routed to the server (aka command provder) which provides `"sayHello"` command.    
If no hosts providing command `"sayHello"`, an empty string `""` returned.


**Server Example:**   

Read [rpcServer.cc](https://github.com/edwardlintw/CorbaComm-RPC/blob/master/examples/rpcServer.cc) for full example.

```
int main(int argc, char* argv[])
{
    cc::CorbaComm* cc = ...;
    
    cc->onCmd("sayHello",
              [](const std::string&, const std::string& param) {
                  std::cout << "Hello, " << param << std::endl;
                  return "OK";
              });
    ...
}
```
The above code demonstrates C++11 `lambda` as `callback function`. When a host `execCmd( )`, the provider's `callback` will be called to responds a `command`.

* **`Publish and Subscribe Messaging Model`**   

**Publisher Example:**   

Read [publisher.cc](https://github.com/edwardlintw/CorbaComm-RPC/blob/master/examples/publisher.cc) for full example.

```
std::string getTemperature()
{
    char buf[80];
    sprintf(buf, "%d.%d", 30+rand()%10, rand()%10);
    return buf;
}

std::string getHumidity()
{
    char buf[80];
    sprintf(buf, "%d.%d", 40+rand()%10, rand()%10);
    return buf;
}

int main(int argc, char* argv[]) 
{
    cc::CorbaComm* cc = ...;

    while (1) {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        if (std::rand() % 2 == 0)
            cc->pushEvent("topicTemperature", getTemperature().c_str());
        else
            cc->pushEvent("topicHumidity", getHumidity().c_str());
    }

    return 0;
}
`````

**Subscriber Example:**

Read [subscriber.cc](https://github.com/edwardlintw/CorbaComm-RPC/blob/master/examples/subscriber.cc) for full example.

`````
cc::CorbaComm* _cc = nullptr;

void eventCallback(const std::string&, const std::string& param)
{
    std::cout << "current humidity: " << param << "%" << std::endl;
}

int main(int argc, char* argv[]) 
{
    _cc = ...;

    // use traditional `callback`
    // 
    _cc->onEvent(topicHumidity, &eventCallback);

    // use C++11 lambda as callback
    //
    _cc->onEvent(topicTemperature,
                 [](const std::string&, const std::string& param) {
                     std::cout << "current temperature: " << param << std::endl;
                 });

    while (1) {
        std::this_thread::sleep_for(std::chrono::seconds(10));
    }

    return 0;
}
`````

#### About CORBA

The `CorbaComm` lib hides any `CORBA` knowledge/information, if you are really interested in this topic, please take a look and visit the followings to have a basic understanding of `CORBA` technology.
* [OMG Welcome to CORBA](http://www.corba.org)
* [OMG CORBA FAQ](http://www.corba.org/faq.htm)
* [CORBA on wiki](https://en.wikipedia.org/wiki/Common_Object_Request_Broker_Architecture)
* [omniORB, my favarite CORBA implementation, including CORBA Name Service omniNames](http://omniorb.sourceforge.net/)
* [omniNotify, CORBA Notification Service Based on omniORB](http://omninotify.sourceforge.net/)

## 2. Build Prerequisite

`CorbaComm` lib implementation depends `omniORB` and `omniNotify`. Before building `CorbaComm`, you must build and install both `omniORB` and `omniNotify` first. Then run `Name Service, omniNames` from `omniORB` and `Notification Service, notifd` from `omniNotify`.    

* to build `omniORB`, please refer to [omniORB website](http://omniorb.sourceforge.net/).

* to build `omniNotify`, please refer to [omniNotify website](http://omninotify.sourceforge.net/). However, `omniNotify` is now an inactive open source project and can't be built for 64-bit architecture; if you would like to target 64-bit, please also refer to [my omniNotify 64-bit patch](https://github.com/edwardlintw/omniNotify).

After building `omniORB` and `omniNotify` successfully, please run Nameing Service `omniNames` first, then launch Notification Service `notifd`; both run in background.

You can use `ps` and `grep` to check both processes running.

```
ps ax | grep omniNames
ps ax | grep notidf
```

If they are not running, here the quick guides. In general, you might need `root` permission to run both process.    
If you never run Name Service `omniNames` before, you must specify `-start` flag; for later runs, `-start` is not needed.  

For Linux:

```
sudo -i
LD_LIBRARY_PATH=/usr/local/lib omniNames -start
```

For Mac:
```
sudo -i
DYLD_LIBRARY_PATH=/usr/local/lib omniNames -start
````

If you see console outputs something without error, you can kill `omniNames` by Ctrl-C (^C), then run in background.   

For Linux:

```
sudo -i
LD_LIBRARY_PATH=/usr/local/lib omniNames&
```

For Mac:

```
sudo -i
DYLD_LIBRARY_PATH=/usr/local/lib omniNames&
````

Now it's time to launch Notification Service `notifd`

For Linux:

```
sudo -i
LD_LIBRARY_PATH=/usr/local/lib notifd&
```

For Mac:

```
sudo -i
DYLD_LIBRARY_PATH=/usr/local/lib notifd&
````

The above library search path `/usr/local/lib` is `omniORB/omniNotify` libraries' installation path. If you install `omniORB/omniNotify` libraries to other path, you must alter the above commands.   

Please visit origin website and omniORB's/omniNotify's README for more detail and concrete information.

## 3. How to Build CorbaComm

Currently `CorbaComm` is built and tested under `Linux` and `Mac` only, you may need to modify `Makefile` in order to build for other platforms. `CorbaComm` has been tested under `Ubuntu Linux 32/64-bit`, `Raspberry Pi3 32-bit` and `Mac OSX 32/64-bit`.     

To build `CorbaComm`, clone this repository first, then simply `make` to build the library.

```
cd CorbaComm-RPC
make
```

If there's no error, you can install library and header files by `make install`.

```
sudo make install
```

By default, the library will be installed to `/usr/local/lib/` and headers to `/usr/local/include`.

After installing `CorbaComm`, you can build `examples/` and learn how to write a `CorbaComm` programs.

```
cd examples
make
````

## 4. C++ Class And Methods

As the above description, the intent of this library is to make things simple. There are only one C++ class and 6 public methods exposed in this library, as below:

#### C++ Namespace And Class

There only one public C++ class `CorbaComm` is in the `cc` namespace.
```
namespace cc {

class CorbaComm {

};

};
```

#### Typedef's

```
typedef std::string (*CommandCallback_t)(const std::string& cmd,
                                         const std::string& param);

Description: The command callback for command provider's onCmd( ).
```

```
typedef void (*EventCallback_t)(const std::string& topic,
                                const std::string& param);

Description: The event callback for subscriber's onEvent( ).
```

```
typedef std::vector<std::string> Commands;

Description: Used as method ::connect()'s second and third parameters;   
for a command provider, it means the provider offers specified commands;   
for a command requester, it means the requester want specified commands.    
However, for command provider and requester, both parameters can be empty, too.   
This is all about `command routing`, detail will be described later. 
```

```
typedef std::string SID;
Description: Used for onEvent(); please refer to ::onEvent() method.
```

#### Public Methods
```
static CorbaComm* connect(const char* hostId,
                          Commands offerCommands,
                          Commands wantCommands,
                          int argc,
                          char* argv[]);

Description: to initialize CorbaComm.
Parameters : const char* hostId, the unique host Id, can't be empty, nor nullptr. This Id must be unique among all apps which are based on CorbaComm.
             Commands offerCommands, for command providers, it means the provider to offer these commands; it can be empty, too. Detail will be described later.
             Commands wantCommands, for command requester, it means the requester want these commands; it can be empty, too. Detail will be described later.
             int argc, main()'s argc, just pass it in. 
             char* argv[], main()'s argv, just pass it in.
Return     : CorbaComm*, a C++ pointer to CorbaComm object. Use this for more operaions on CorbaComm.             

```                        

```
std::string execCmd(const char* cmd, const char* param);
Description: to execute(request) a command, aka to make a direct-RPC call.
Parameters : const char* cmd, the command to request, can't be empty or nullptr
             const char* param, the command parameter, can't be nullptr.
Return     : std::string, what command provider responds. If this is an empty string "", it general means there's no provider to respond this command.
```

```
void onCmd(const char* cmd, CommandCallback_t callback);
Description: for command providers, when a client requester executes a command, this callback is called.
Parameters : const char* cmd, what the provider offers.
             CommandCallback_t callback, the callback function.
Return     : N/A
```

```
bool pushEvent(const char* topic, const char* param);
Description: push an event with `topic` to CORBA Notification server.
             If you use `omniNotify` like me, the server is `notifd` process.
Parameters : const char* topic, topic identifier.
             conat char* param, event parameter, aka `message`
Return     : bool, true if this event pushed to notification server;
                   false otherwise
```

```
SID onEvent(const char* topic, EventCallback_t callback);
Description: This method is used for subscribing events by topic `topic`;
             When publisher push an event with topic `topic`, the `callback` function is called.
Parameters : const char* topic, event `topic` subscribe
Return     : an unique `Subscription ID`; the subscriber can call
             `detachEvent()` with this value to unsubscribe an event.
```

```
void detachEvent(const SID& sid);
Description: To unsubscribe an event
Parameters : const SID&, the `subscription id` returned by `onEvent()`
Return     : N/A
```
## 5. Running Examples

There are [six examples](https://github.com/edwardlintw/CorbaComm-RPC/tree/master/examples) available for your study, understanding and reference.   

* [rpcClient.cc](https://github.com/edwardlintw/CorbaComm-RPC/tree/master/examples/rpcClient.cc) and [rpcServer.cc](https://github.com/edwardlintw/CorbaComm-RPC/tree/master/examples/rpcServer.cc) demonstrate basic RPC-call.   
* [publisher.cc](https://github.com/edwardlintw/CorbaComm-RPC/tree/master/examples/publisher.cc) and [subscriber.cc](https://github.com/edwardlintw/CorbaComm-RPC/tree/master/examples/subscriber.cc) demonstrate publish-subscribe messaging model communication.
* [rwClient.cc](https://github.com/edwardlintw/CorbaComm-RPC/tree/master/examples/rwClient.cc) and [rwServer.cc](https://github.com/edwardlintw/CorbaComm-RPC/tree/master/examples/rwServer.cc) demonstrate how shared resources must be procteded.

Befor you can start running these examples, please make sure Name Service `omniNaems` and Notification Service `notifd` are running without any issues.

## 6. Command Routing

If you have read [rpcClient.cc](https://github.com/edwardlintw/CorbaComm-RPC/blob/master/examples/rpcClient.cc) and [rpcServer.cc](https://github.com/edwardlintw/CorbaComm-RPC/blob/master/examples/rpcServer.cc) examples, you may experience how simple and easy they are. The client will never know who and where the server is, even without any knowledge/information about `IP`, `hostname`, or `port`, but it does make an RPC-invocation successfully. Actually, the `call-path` is routed automatically by `CorbaComm`. The mechanism is called `Command Routing`.

The `CorbaComm` support two types of `Command Routing`, they are:
* **Early Command Routing**
* **Late Command Routing**

### Early Command Routing

`Early Command Routing` is done immediately after you launch a client or a server process.   

If you want `CorbaComm` to do `Early Command Routing`, your code must explicitly specify the second (offer commands) and/or the third (want commands) parameters of `connect()` methods, like [rpcClient.cc](https://github.com/edwardlintw/CorbaComm-RPC/blob/master/examples/rpcClient.cc) and [rpcServer.cc](https://github.com/edwardlintw/CorbaComm-RPC/blob/master/examples/rpcServer.cc) do.   

Once the `call-path` is determined, the `CorbaComm` will cache this path, no more routing tasks is needed for later invocations.

### Late Command Routing

`Late Command Routing` means the call-path is routed on demand. If a server doen't specify the second parameter (offer commands) of `connect()`, or a client doesn't specify the third pamameter (want commands), the `call-path` is routed when a client makes the first attempt of invocation.    

Like `Early Command Routing`, once the call-path is determined, it'll be cached and no more routing tasks is needed for later invocations.

Sometimes `Late Command Routing` is refered to `Lazy Command Routing`, since the devlopers are `lazy` to specify the second and third parameters of `connect()`, leave them both empty.  

[rwClient.cc](https://github.com/edwardlintw/CorbaComm-RPC/tree/master/examples/rwClient.cc) and [rwServer.cc](https://github.com/edwardlintw/CorbaComm-RPC/tree/master/examples/rwServer.cc) are examples of `Late Command Routing`; you'll see the second and the third paramters of `connect()` from both source codes are empty. Thanks to `Late Command Routing`, they do work.

## 7. Multithreading Considerations

All `CorbaComm` apps are multithreading, although you don't see any clues from source code, such as [examples/subscriber.cc](https://github.com/edwardlintw/CorbaComm-RPC/blob/master/examples/subscriber.cc).    

The rationale is `CorbaComm` is developed base on `CORBA`, all the `Callback` functions, such as `onEvent()'` and `onCmd()'` callbacks are invoked by a `CORBA thread`.     

That's the reason why there are a busy-while-loop in [examples/subscriber.cc](https://github.com/edwardlintw/CorbaComm-RPC/blob/master/examples/subscriber.cc) but the process still can receives `events`; since the callback is called on another thread, not on any threads you created expcilitly.

Whatever resources that `Callbacks` of `onEvent()` or `onCmd` access to implies the resources could be read/written concurrently. If there's no protection mechanism, the resources tend to be corrupted.

[rwClient.cc](https://github.com/edwardlintw/CorbaComm-RPC/tree/master/examples/rwClient.cc) and [rwServer.cc](https://github.com/edwardlintw/CorbaComm-RPC/tree/master/examples/rwServer.cc) are simple examples to demonstrate how shared resources are protected.

## 8. Distributed RPC

`CorbaComm` supports `Distributed RPC`. Before you deploy `CorbaComm` apps to distributed environment, you'd better understand what `CORBA Naming Service` is.      

The concept of `CORBA Naming Service` is similar to `DNS`. If I ask you "do you know what is the `IP address` of `github.com`", I guess your answer is `No`, me neither.   

`IP address` is annoying, to remember `IP address` of any website is non-sense. This is the reason why `DNS` was invented. `DNS`, the Domain Name Server, resolves `IP address` of a website by its `domain name`. Thus, we can surf internet world by `Domain Names` and forget any `IP addresses`.   

`CORBA Name Server` does the similar thing. Each `CORBA Object` has a unique ID, any `CORBA Object` can bind its `Object ID` with pre-defined `Context Name` to `CORBA Name Server`. Thus the client can resolve `Object ID` with known `Context Name` from `CORBA Name Server` to make a direct RPC-call.   

What's the catch? Rember the first parameter `hostId` of method `connect()`? This is the reason why this `hostId` must be unique, because it's part of the `CORBA Context Name`, although it is hidden.   
