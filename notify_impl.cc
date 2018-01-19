#include <iostream>
#include <fstream>
#include <thread>
#include "cos.h"
#include "notify_impl.h"

using namespace std;

void destroy_filters(FilterSeq& seq) {
  for (unsigned int i = 0; i < seq.length(); i++) {
    try {
      seq[i]->destroy();
      seq[i] = CosNF::Filter::_nil();
    } catch (...) { }
  } 
  seq.length(0);
}

void offer_any(CosNC::NotifyPublish_ptr proxy, const char* objnm, CORBA::Boolean verbose) {
  if (verbose) cout << objnm << ": offering type %ANY" << endl;
  CosN::EventTypeSeq added, deled;
  added.length(1);
  added[0].domain_name = CORBA::string_dup("");
  added[0].type_name = CORBA::string_dup("%ANY");
  deled.length(0);
  try {
    proxy->offer_change(added, deled);
  } catch (...) {
    if (verbose) cout << "** registration failed **" << endl;
  }
}

CORBA::Boolean
sample_add_filter(CosNCA::EventChannel_ptr channel, 
		  CosNF::FilterAdmin_ptr fadmin,
		  CosN::EventTypeSeq& evs,
		  const char* constraint_expr,
		  const char* obj_name,
		  CosNF::Filter_ptr& filter, // out param
		  CORBA::Boolean verbose) {
  // if evs and constraint expr are empty, we ignore them + do not add a filter
  if ((evs.length() == 0) && (!constraint_expr || 0 == strlen(constraint_expr))) {
    if (verbose) cout << obj_name << ": (no filter used)" << endl;
    return 0; // OK
  }
  // Obtain a reference to the default filter factory; create a filter object 
  CosNF::FilterFactory_ptr ffp;
  filter = CosNF::Filter::_nil();
  try {
    if (verbose) cout << obj_name << ": Obtaining default filter factory" << endl;
    ffp    = channel->default_filter_factory();  
    filter = ffp->create_filter("EXTENDED_TCL");
  } catch (CORBA::COMM_FAILURE& ex) {
    cerr << obj_name << ": Caught COMM_FAILURE obtaining filter object" << endl;
    return 1; // error
  } catch (...) {
    cerr << obj_name << ": Caught exception obtaining filter object" << endl;
    return 1; // error
  }
  if (verbose) cout << obj_name << ": Obtained filter from default filter factory" << endl;

  // Construct a simple constraint expression; add it to fadmin
  CosNF::ConstraintExpSeq   exp;
  exp.length(1);
  exp[0].event_types = evs;
  exp[0].constraint_expr = CORBA::string_dup(constraint_expr);
  CORBA::Boolean res = 0; // OK
  try {
    if (verbose) cout << obj_name << ": Adding constraints to filter" << endl;
    filter->add_constraints(exp);
    if (verbose) cout << obj_name << ": Adding filter to target" << endl;
    fadmin->add_filter(filter);
    if (verbose) {
      if (evs.length()) {
	cout << obj_name << ": Added filter for types ";
	for (unsigned int j = 0; j < evs.length(); j++) { 
	  cout << (const char*)evs[j].domain_name << "::" << (const char*)evs[j].type_name;
	  if ((j+1) < evs.length())
	    cout << ", ";
	}
      } else {
	cout << obj_name << ": Added filter for type *::* ";
      }
      cout << " and constraint expression \"" << constraint_expr << "\" " << endl;
    }
  }
  catch(CosNF::InvalidConstraint& _exobj1) {
    cerr << obj_name << ": Exception thrown : Invalid constraint given "
	 << (const char *)constraint_expr << endl;
    res = 1; // error
  }
  catch (...) {
    cerr << obj_name << ": Exception thrown while adding constraint " 
	 << (const char *)constraint_expr << endl; 
    res = 1; // error
  }
  if (res == 1) { // error so destroy filter
    try {
      filter->destroy();
    } catch (...) { }
    filter = CosNF::Filter::_nil();
    return 1; // error
  }
  // OK
  return 0; // OK
}

CosNCA::ProxyConsumer_ptr get_proxy_consumer(CORBA::ORB_ptr orb,
					   CosNCA::EventChannel_ptr channel,
					   CosNCA::ClientType ctype,
					   CORBA::Boolean push_proxy,
					   CosNCA::SupplierAdmin_ptr& admin, // out param
					   CORBA::Boolean verbose) {
  CosNCA::ProxyConsumer_ptr generic_proxy = CosNCA::ProxyConsumer::_nil();
  CosNCA::InterFilterGroupOperator ifoper = CosNCA::AND_OP;
  //  CosNA::InterFilterGroupOperator ifoper = CosNCA::OR_OP;
  CosNCA::AdminID admID;
  admin = CosNCA::SupplierAdmin::_nil();
  try {
    admin = channel->new_for_suppliers(ifoper, admID);
    if ( CORBA::is_nil(admin) ) {
      cerr << "Failed to obtain admin" << endl;
      return generic_proxy; // failure
    }
  } catch (...) {
    cerr << "Failed to obtain admin" << endl;
    return generic_proxy;  // failure
  }
  if (verbose) cout << "Obtained admin from the channel" << endl;

  CosNCA::ProxyID prxID;
  try {
    if (push_proxy) {
      generic_proxy = admin->obtain_notification_push_consumer(ctype, prxID);
    } else {
      generic_proxy = admin->obtain_notification_pull_consumer(ctype, prxID);
    }
  } catch (...) {  }
  if (CORBA::is_nil(generic_proxy)) {
    cerr << "Failed to obtain proxy" << endl;
    try {
      admin->destroy();
    } catch (...) { }
    admin = CosNCA::SupplierAdmin::_nil();
    return generic_proxy;
  }
  if (verbose) cout << "Obtained proxy from admin" << endl;
  return generic_proxy; // success if generic_proxy is non-nil, otherwise failure
}

// ==================== PushSupplier_i ===================
//

PushSupplier_i::
PushSupplier_i(CosNCA::StructuredProxyPushConsumer_ptr proxy,
	       CosNCA::SupplierAdmin_ptr admin, CosNF::Filter_ptr filter,
	       const char* objnm, type_change_fn* change_fn) :
  _my_proxy(proxy), _my_admin(admin), _my_filters(0),
  _obj_name(objnm), _change_fn(change_fn), _verbose(0)
{
  // providing explict NULL for supply_fn is not OK -- must have a valid function
  if (! CORBA::is_nil(filter)) {
    _my_filters.length(1);
    _my_filters[0] = filter;
  }
}

PushSupplier_i*
PushSupplier_i::create(CORBA::ORB_ptr orb,
		       CosNCA::EventChannel_ptr channel,
		       const char* objnm,
		       type_change_fn* change_fn,
		       CosN::EventTypeSeq* evs_ptr,
		       const char* constraint_expr)
{
  // Obtain appropriate proxy object
  CosNCA::SupplierAdmin_ptr admin = CosNCA::SupplierAdmin::_nil();
  CosNCA::ProxyConsumer_var generic_proxy =
    get_proxy_consumer(orb, channel, CosNCA::STRUCTURED_EVENT, 1,  admin, 0);
  CosNCA::StructuredProxyPushConsumer_ptr proxy = CosNCA::StructuredProxyPushConsumer::_narrow(generic_proxy);
  if ( CORBA::is_nil(proxy) ) {
    return 0; // get_proxy_consumer failed
  }

  // If evs or constraint_expr are non-empty, add a filter to proxy
  CosNF::Filter_ptr filter = CosNF::Filter::_nil();

  if (evs_ptr) {
    CORBA::Boolean filt_err = sample_add_filter(channel, proxy, *evs_ptr, constraint_expr, objnm, filter, 0);
    if (filt_err) {
      try {
	admin->destroy();
      } catch (...) { }
      return 0; // adding filter failed
    }
  }

  // Construct a client
  PushSupplier_i* client =
    new PushSupplier_i(proxy, admin, filter, objnm, change_fn);
  return client;
}

void PushSupplier_i::push(const CosN::StructuredEvent& data)
{
    _my_proxy->push_structured_event(data);
}

void PushSupplier_i::disconnect_structured_push_supplier()
{
}

CORBA::Boolean PushSupplier_i::connect() {
  try {
    _my_proxy->connect_structured_push_supplier(_this());
    if (_change_fn) {
      _my_proxy->obtain_subscription_types(CosNCA::NONE_NOW_UPDATES_ON);
    } else {
      _my_proxy->obtain_subscription_types(CosNCA::NONE_NOW_UPDATES_OFF);
    }
  } catch (CORBA::BAD_PARAM& ex) {
    cerr << _obj_name << ": BAD_PARAM Exception while connecting" << endl;
    return 1; // error
  } catch (CosECA::AlreadyConnected& ex) {
    cerr << _obj_name << ": Already connected" << endl;
    return 1; // error
  } catch (...) {
    cerr << _obj_name << ": Failed to connect" << endl;
    return 1; // error
  }
  // register the types to be supplied
  offer_any(_my_proxy, _obj_name, _verbose);
  return 0; // OK
}

void PushSupplier_i::cleanup() {
  CosNCA::StructuredProxyPushConsumer_var proxy;
  proxy = _my_proxy;
  _my_proxy = CosNCA::StructuredProxyPushConsumer::_nil();
 
  try {
    if (!CORBA::is_nil(proxy))
      proxy->disconnect_structured_push_consumer();
  } 
  catch(...) {

  }
  try {
    _my_admin->destroy();
  } 
  catch (...) { 
  
  }
  _my_admin = CosNCA::SupplierAdmin::_nil();
  destroy_filters(_my_filters);
}

void PushSupplier_i::subscription_change(const CosN::EventTypeSeq& added,
					 const CosN::EventTypeSeq& deled)
{
  static CORBA::ULong events;
  if (_change_fn) 
      (*_change_fn)(added, deled, _obj_name, events++, _verbose);
  else if (_verbose) 
      cout << _obj_name << ": subscription_change received [# " << events << "]" << endl;
  else
      ;
}

CosNCA::ProxySupplier_ptr get_proxy_supplier(CORBA::ORB_ptr orb,
					   CosNCA::EventChannel_ptr channel,
					   CosNCA::ClientType ctype,
					   CORBA::Boolean push_proxy,
					   CosNCA::ConsumerAdmin_ptr& admin, // out param
					   CORBA::Boolean verbose) {
  CosNCA::ProxySupplier_ptr generic_proxy = CosNCA::ProxySupplier::_nil();
  CosNCA::InterFilterGroupOperator ifoper = CosNCA::AND_OP;
  //  CosNA::InterFilterGroupOperator ifoper = CosNCA::OR_OP;
  CosNCA::AdminID admID;
  admin = CosNCA::ConsumerAdmin::_nil();
  try {
    admin = channel->new_for_consumers(ifoper, admID);
    if ( CORBA::is_nil(admin) ) {
      cerr << "Failed to obtain admin" << endl;
      return generic_proxy; // failure
    }
  } 
  catch (...) {
    cerr << "Failed to obtain admin" << endl;
    return generic_proxy;  // failure
  }
  if (verbose) cout << "Obtained admin from the channel" << endl;

  CosNCA::ProxyID prxID;
  try {
    if (push_proxy) {
      generic_proxy = admin->obtain_notification_push_supplier(ctype, prxID);
    } 
    else {
      generic_proxy = admin->obtain_notification_pull_supplier(ctype, prxID);
    }
  } 
  catch (...) {  }
  if (CORBA::is_nil(generic_proxy)) {
    cerr << "Failed to obtain proxy" << endl;
    try {
      admin->destroy();
    } 
    catch (...) { 
    }
    admin = CosNCA::ConsumerAdmin::_nil();
    return generic_proxy;
  }
  if (verbose) cout << "Obtained proxy from admin" << endl;
  return generic_proxy; // success if generic_proxy is non-nil, otherwise failure
}

// ==================== PushConsumer_i ===================
//

PushConsumer_i::
PushConsumer_i(CosNCA::StructuredProxyPushSupplier_ptr proxy,
	       CosNCA::ConsumerAdmin_ptr admin,
	       CosNF::Filter_ptr filter,
	       const char* objnm,
	       consume_fn* consume_func, 
           type_change_fn* change_fn) :
  _my_proxy(proxy), _my_admin(admin), _my_filters(0),
  _obj_name(objnm), _consume_fn(consume_func), _change_fn(change_fn), _verbose(0),
  _recvEvents(0)
{
  if (! CORBA::is_nil(filter)) {
    _my_filters.length(1);
    _my_filters[0] = filter;
  }
}

PushConsumer_i*
PushConsumer_i::create(CORBA::ORB_ptr orb,
		       CosNCA::EventChannel_ptr channel,
		       const char* objnm,
		       consume_fn* consume_func,
		       type_change_fn* change_fn,
		       CosN::EventTypeSeq* evs_ptr,
		       const char* constraint_expr)
{
  // Obtain appropriate proxy object
  CosNCA::ConsumerAdmin_ptr admin = CosNCA::ConsumerAdmin::_nil();
  CosNCA::ProxySupplier_var generic_proxy =
    get_proxy_supplier(orb, channel, CosNCA::STRUCTURED_EVENT, 1, admin, 0); // 1 means push 0 means pull
  CosNCA::StructuredProxyPushSupplier_ptr proxy = CosNCA::StructuredProxyPushSupplier::_narrow(generic_proxy);
  if ( CORBA::is_nil(proxy) ) {
    return 0; // get_proxy_supplier failed
  }

  // If evs or constraint_expr are non-empty, add a filter to proxy
  CosNF::Filter_ptr filter = CosNF::Filter::_nil();

  if (evs_ptr) {
    CORBA::Boolean filt_err = sample_add_filter(channel, proxy, *evs_ptr, constraint_expr, objnm, filter, 0);
    if (filt_err) {
      try {
	admin->destroy();
      } catch (...) { }
      return 0; // adding filter failed
    }
  }

  // Construct a client
  PushConsumer_i* client =
    new PushConsumer_i(proxy, admin, filter, objnm, consume_func, change_fn);
  return client;
}

CORBA::Boolean PushConsumer_i::connect() {
  try {
    _my_proxy->connect_structured_push_consumer(_this());
    if (_change_fn) {
      _my_proxy->obtain_offered_types(CosNCA::NONE_NOW_UPDATES_ON);
    } else {
      _my_proxy->obtain_offered_types(CosNCA::NONE_NOW_UPDATES_OFF);
    }
  } 
  catch (CORBA::BAD_PARAM& ex) {
    cerr << _obj_name << ": BAD_PARAM Exception while connecting" << endl;
    return 1; // error
  } 
  catch (CosECA::AlreadyConnected& ex) {
    cerr << _obj_name << ": Already connected" << endl;
    return 1; // error
  } 
  catch (...) {
    cerr << _obj_name << ": Failed to connect" << endl;
    return 1; // error
  }
  if (_verbose) cout << _obj_name << ": Connected to proxy, ready to consume events" << endl;
  return 0; // OK
}

void PushConsumer_i::cleanup() {
  CosNCA::StructuredProxyPushSupplier_var proxy;
  
  // this method takes sole ownership of _
  //private:my_proxy ref
  proxy = _my_proxy;
  _my_proxy = CosNCA::StructuredProxyPushSupplier::_nil();
  
  // do not hold oplock while invoking disconnect
  try {
      proxy->disconnect_structured_push_supplier();
  } 
  catch(...) {
  }
  try {
    _my_admin->destroy();
  } 
  catch (...) { 
  }
  _my_admin = CosNCA::ConsumerAdmin::_nil();
  destroy_filters(_my_filters);
}

void PushConsumer_i::push_structured_event(const CosN::StructuredEvent& data)
{
  if (_consume_fn)
    (*_consume_fn)(data);
  else 
    if (_verbose) cout << _obj_name << ": event count = " << ++_recvEvents << endl;
}

void PushConsumer_i::disconnect_structured_push_consumer()
{
}

void PushConsumer_i::offer_change(const CosN::EventTypeSeq& added,
					 const CosN::EventTypeSeq& deled)
{
  if (_change_fn) 
      (*_change_fn)(added, deled, _obj_name, 0, _verbose);
  else if (_verbose) 
      cout << _obj_name << ": subscription_change received [# " << _recvEvents << "]" << endl;
}

#if 0
// ==================== PushSupplier_i ===================
//

PullSupplier_i::
PullSupplier_i(CosNCA::StructuredProxyPullConsumer_ptr proxy,
	       CosNCA::SupplierAdmin_ptr admin, CosNF::Filter_ptr filter,
	       const char* objnm, supply_any_fn* supply_fn, type_change_fn* change_fn) :
  _my_proxy(proxy), _my_admin(admin), _my_filters(0),
  _obj_name(objnm), _supply_fn(supply_fn), _change_fn(change_fn), 
  _verbose(0), _reqEvents(0)
{
  // providing explict NULL for supply_fn is not OK -- must have a valid function
  if (! CORBA::is_nil(filter)) {
    _my_filters.length(1);
    _my_filters[0] = filter;
  }
}

PullSupplier_i*
PullSupplier_i::create(CORBA::ORB_ptr orb,
		       CosNCA::EventChannel_ptr channel,
		       const char* objnm,
               supply_any_fn* supply_fn,
		       type_change_fn* change_fn,
		       CosN::EventTypeSeq* evs_ptr,
		       const char* constraint_expr)
{
  // Obtain appropriate proxy object
  CosNCA::SupplierAdmin_ptr admin = CosNCA::SupplierAdmin::_nil();
  CosNCA::ProxyConsumer_var generic_proxy =
    get_proxy_consumer(orb, channel, CosNCA::STRUCTURED_EVENT, 0,  admin, 0);
  CosNCA::StructuredProxyPullConsumer_ptr proxy = CosNCA::StructuredProxyPullConsumer::_narrow(generic_proxy);
  if ( CORBA::is_nil(proxy) ) {
    return 0; // get_proxy_consumer failed
  }

  // If evs or constraint_expr are non-empty, add a filter to proxy
  CosNF::Filter_ptr filter = CosNF::Filter::_nil();

  if (evs_ptr) {
    CORBA::Boolean filt_err = sample_add_filter(channel, proxy, *evs_ptr, constraint_expr, objnm, filter, 0);
    if (filt_err) {
      try {
	admin->destroy();
      } catch (...) { }
      return 0; // adding filter failed
    }
  }

  // Construct a client
  PullSupplier_i* client =
    new PullSupplier_i(proxy, admin, filter, objnm, supply_fn, change_fn);
  return client;
}

CosN::StructuredEvent* PullSupplier_i::pull_structured_event()
{
    CosN::StructuredEvent* data = new CosN::StructuredEvent;
    if (_supply_fn)
        (*_supply_fn)(*data, _obj_name, ++_reqEvents, 0);
    return data;
}

CosN::StructuredEvent* PullSupplier_i::try_pull_structured_event(CORBA::Boolean& hasEvent)
{
    CosN::StructuredEvent* data = new CosN::StructuredEvent;
    if (_supply_fn)
        (*_supply_fn)(*data, _obj_name, ++_reqEvents, 0);
    return data;
}

void PullSupplier_i::disconnect_structured_pull_supplier()
{
}

CORBA::Boolean PullSupplier_i::connect() {
  try {
    _my_proxy->connect_structured_pull_supplier(_this());
    if (_change_fn) {
      _my_proxy->obtain_subscription_types(CosNCA::NONE_NOW_UPDATES_ON);
    } else {
      _my_proxy->obtain_subscription_types(CosNCA::NONE_NOW_UPDATES_OFF);
    }
  } catch (CORBA::BAD_PARAM& ex) {
    cerr << _obj_name << ": BAD_PARAM Exception while connecting" << endl;
    return 1; // error
  } catch (CosECA::AlreadyConnected& ex) {
    cerr << _obj_name << ": Already connected" << endl;
    return 1; // error
  } catch (...) {
    cerr << _obj_name << ": Failed to connect" << endl;
    return 1; // error
  }
  // register the types to be supplied
  offer_any(_my_proxy, _obj_name, _verbose);
  return 0; // OK
}

void PullSupplier_i::cleanup() {
  CosNCA::StructuredProxyPullConsumer_var proxy;
  proxy = _my_proxy;
  _my_proxy = CosNCA::StructuredProxyPullConsumer::_nil();
 
  try {
    if (!CORBA::is_nil(proxy))
      proxy->disconnect_structured_pull_consumer();
  } 
  catch(...) {

  }
  try {
    _my_admin->destroy();
  } 
  catch (...) { 
  
  }
  _my_admin = CosNCA::SupplierAdmin::_nil();
  destroy_filters(_my_filters);
}

void PullSupplier_i::subscription_change(const CosN::EventTypeSeq& added,
					 const CosN::EventTypeSeq& deled)
{
  static CORBA::ULong events;
  if (_change_fn) 
      (*_change_fn)(added, deled, _obj_name, events++, _verbose);
  else if (_verbose) 
      cout << _obj_name << ": subscription_change received [# " << events << "]" << endl;
  else
      ;
}

// These 2 helper routines are used to obtain one of the
// 12 kinds of notification channel proxies from a channel;
// they return a nil reference if a failure occurs


// ==================== PullConsumer_i ===================
//

PullConsumer_i::
PullConsumer_i(CosNCA::StructuredProxyPullSupplier_ptr proxy,
	       CosNCA::ConsumerAdmin_ptr admin,
	       CosNF::Filter_ptr filter,
	       const char* objnm,
           type_change_fn* change_fn) :
  _my_proxy(proxy), _my_admin(admin), _my_filters(0),
  _obj_name(objnm), _change_fn(change_fn), _verbose(0),
  _recvEvents(0)
{
  if (! CORBA::is_nil(filter)) {
    _my_filters.length(1);
    _my_filters[0] = filter;
  }
}

PullConsumer_i*
PullConsumer_i::create(CORBA::ORB_ptr orb,
		       CosNCA::EventChannel_ptr channel,
		       const char* objnm,
		       type_change_fn* change_fn,
		       CosN::EventTypeSeq* evs_ptr,
		       const char* constraint_expr)
{
  // Obtain appropriate proxy object
  CosNCA::ConsumerAdmin_ptr admin = CosNCA::ConsumerAdmin::_nil();
  CosNCA::ProxySupplier_var generic_proxy =
    get_proxy_supplier(orb, channel, CosNCA::STRUCTURED_EVENT, 0, admin, 0); // 1 means push 0 means pull
  CosNCA::StructuredProxyPullSupplier_ptr proxy = CosNCA::StructuredProxyPullSupplier::_narrow(generic_proxy);
  if ( CORBA::is_nil(proxy) ) {
    return 0; // get_proxy_consumer failed
  }

  // If evs or constraint_expr are non-empty, add a filter to proxy
  CosNF::Filter_ptr filter = CosNF::Filter::_nil();

  if (evs_ptr) {
    CORBA::Boolean filt_err = sample_add_filter(channel, proxy, *evs_ptr, constraint_expr, objnm, filter, 0);
    if (filt_err) {
      try {
	admin->destroy();
      } catch (...) { }
      return 0; // adding filter failed
    }
  }

  // Construct a client
  PullConsumer_i* client =
    new PullConsumer_i(proxy, admin, filter, objnm, change_fn);
  return client;
}

CORBA::Boolean PullConsumer_i::connect() {
  try {
    _my_proxy->connect_structured_pull_consumer(_this());
    if (_change_fn) {
      _my_proxy->obtain_offered_types(CosNCA::NONE_NOW_UPDATES_ON);
    } else {
      _my_proxy->obtain_offered_types(CosNCA::NONE_NOW_UPDATES_OFF);
    }
  } 
  catch (CORBA::BAD_PARAM& ex) {
    cerr << _obj_name << ": BAD_PARAM Exception while connecting" << endl;
    return 1; // error
  } 
  catch (CosECA::AlreadyConnected& ex) {
    cerr << _obj_name << ": Already connected" << endl;
    return 1; // error
  } 
  catch (...) {
    cerr << _obj_name << ": Failed to connect" << endl;
    return 1; // error
  }
  if (_verbose) cout << _obj_name << ": Connected to proxy, ready to consume events" << endl;
  return 0; // OK
}

void PullConsumer_i::cleanup() {
  CosNCA::StructuredProxyPullSupplier_var proxy;
  
  // this method takes sole ownership of _
  //private:my_proxy ref
  proxy = _my_proxy;
  _my_proxy = CosNCA::StructuredProxyPullSupplier::_nil();
  
  // do not hold oplock while invoking disconnect
  try {
      proxy->disconnect_structured_pull_supplier();
  } 
  catch(...) {
  }
  try {
    _my_admin->destroy();
  } 
  catch (...) { 
  }
  _my_admin = CosNCA::ConsumerAdmin::_nil();
  destroy_filters(_my_filters);
}

CosN::StructuredEvent* PullConsumer_i::pull()
{
    return _my_proxy->pull_structured_event();
}

CosN::StructuredEvent* PullConsumer_i::pull(CORBA::Boolean& hasEvent)
{
    return _my_proxy->try_pull_structured_event(hasEvent);
}

void PullConsumer_i::disconnect_structured_pull_consumer()
{
}

void PullConsumer_i::offer_change(const CosN::EventTypeSeq& added,
					 const CosN::EventTypeSeq& deled)
{
  if (_change_fn) 
      (*_change_fn)(added, deled, _obj_name, 0, _verbose);
  else if (_verbose) 
      cout << _obj_name << ": subscription_change received [# " << _recvEvents << "]" << endl;
}

#endif
