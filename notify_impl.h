#ifndef _NOTIFY_IMPL_H
#define _NOTIFY_IMPL_H

#include "cos.h"

typedef void consume_fn(const CosN::StructuredEvent&);
typedef void type_change_fn(const CosN::EventTypeSeq& added,
                            const CosN::EventTypeSeq& deled,
                            const char* objName,
                            CORBA::ULong numChanged,
                            CORBA::Boolean verbose);
#define WRAPPED_DECL_UNBOUNDED_SEQUENCE_TYPE(t,tseq) \
      typedef _CORBA_Unbounded_Sequence<t> tseq

WRAPPED_DECL_UNBOUNDED_SEQUENCE_TYPE(CosNF::Filter_var, FilterSeq);

//////////////////////////////////////////////////////////////////////
//
// Push Supplier/Consumer
//
//////////////////////////////////////////////////////////////////////

class PushSupplier_i : public POA_CosNotifyComm::StructuredPushSupplier
                     , public PortableServer::RefCountServantBase
{
private:
    PushSupplier_i( CosNCA::StructuredProxyPushConsumer_ptr proxy, 
                    CosNCA::SupplierAdmin_ptr admin, 
                    CosNF::Filter_ptr filter,
		            const char* objnm, 
                    type_change_fn* change_fn);
public:
    static 
    PushSupplier_i* create(CORBA::ORB_ptr orb,
	                       CosNCA::EventChannel_ptr channel,
	                       const char* objnm,
	                       type_change_fn* change_fn,
	                       CosN::EventTypeSeq* evs_ptr = 0,
	                       const char* constraint_expr = "");

    // IDL methods
    void disconnect_structured_push_supplier();
    void subscription_change(const CosN::EventTypeSeq& added,
	                         const CosN::EventTypeSeq& deled);
  
    // Local methods
    CORBA::Boolean connect();
    void  cleanup();
    void push(const CosN::StructuredEvent&);

protected:
    CosNCA::StructuredProxyPushConsumer_var _my_proxy;
    CosNCA::SupplierAdmin_var     _my_admin;
    FilterSeq                   _my_filters;
    const char*                 _obj_name;
    type_change_fn*             _change_fn;
    CORBA::Boolean              _verbose;
};


class PushConsumer_i : public POA_CosNotifyComm::StructuredPushConsumer,
  public PortableServer::RefCountServantBase
{
public:
  PushConsumer_i(CosNCA::StructuredProxyPushSupplier_ptr proxy, 
                 CosNCA::ConsumerAdmin_ptr admin, 
                 CosNF::Filter_ptr filter,
		         const char* objnm, 
                 consume_fn* consume_func,
                 type_change_fn* change_fn);

  static PushConsumer_i* 
  create(CORBA::ORB_ptr orb,
	     CosNCA::EventChannel_ptr channel,
	     const char* objnm,
	     consume_fn* consume_func,
	     type_change_fn* change_fn,
	     CosN::EventTypeSeq* evs_ptr = 0,
	     const char* constraint_expr = "");

  // IDL methods
  void push_structured_event(const CosN::StructuredEvent& data);
  void disconnect_structured_push_consumer();
  void offer_change(const CosN::EventTypeSeq& added,
		    const CosN::EventTypeSeq& deled);

  // Local methods
  CORBA::Boolean connect();
  void  cleanup();

protected:
  CosNCA::StructuredProxyPushSupplier_var _my_proxy;
  CosNCA::ConsumerAdmin_var     _my_admin;
  FilterSeq                     _my_filters;
  const char*                   _obj_name;
  consume_fn*                   _consume_fn;
  type_change_fn*               _change_fn;
  CORBA::Boolean                _verbose;
  CORBA::ULong                  _recvEvents;
};

#endif
