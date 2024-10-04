
#include <sys/time.h>
#include <stdlib.h>
#include <math.h>
#include "trace_log.h" 
#include "util.h"
#include "msg_protocol.h"
#include "read_config.h"
#include "read_cme_data.h"
#include "read_client_data.h"
#include "client_message.h"
#include "exec_globex.h"
#include "order_book.h"
#include "trade_process.h"
#include "trade_socket_list.h"
#include "globex_common.h"

// ---- globe variable ----
extern bool et_g_stopProcess;
extern bool g_need_clean_order;
extern map<string, int> g_socket;
extern int g_posiSocket;
extern int g_cli_num;

extern exec_globex* et_g_globex;
extern order_book* et_g_orderBook;
extern EtReadCmeData* et_g_readCme;
extern EtReadClientData* et_g_readClient;
extern EtTradeSocketList* g_socketListPtr;
// ---- globe variable, end ----


EtTradeProcess::EtTradeProcess()
{
	pthread_mutex_init(&m_mutex, NULL);
}


EtTradeProcess::~EtTradeProcess()
{
	pthread_mutex_destroy(&m_mutex);
}

bool EtTradeProcess::startTradeProcess()
{
  int iRes = 0;
  iRes = pthread_create(&m_startThread, NULL, startTradeThread, this); 
  if (iRes != 0) 
    { 
      //DBG_ERR("fail to create a thread. %s.", strerror(iRes)); 
      return false; 
    } 
 
  return true;
}

void EtTradeProcess::setThreadCondition(pthread_cond_t* _cond)
{
	m_threadCondition = _cond;
}


void* EtTradeProcess::startTradeThread(void *arg)
{
  EtTradeProcess* myReadData = (EtTradeProcess*)arg;
  myReadData->mainLoop();

  return NULL;
}

void EtTradeProcess::mainLoop()
{
	DBG(TRACE_LOG_DEBUG, "enter into mainLoop.");

	int iRes = 0;

	struct timespec waitTime;
	struct timeval nowTime;
	pthread_mutex_t conditionMutex;

	// thread mutex
	iRes = pthread_mutex_init(&conditionMutex, NULL);
	if (iRes != 0)
	{
		DBG(TRACE_LOG_ERR, "Failed to init the mutex of conditionMutex. %s.", 
			strerror(iRes));
		return;
	}

	while (true)
	{	
	    if (et_g_stopProcess)
		{
			break;
		}

		gettimeofday(&nowTime, NULL);
        waitTime.tv_sec = nowTime.tv_sec + 1;
		waitTime.tv_nsec = nowTime.tv_usec * 100;

		pthread_mutex_lock(&conditionMutex);

		if (isEmpty() && et_g_readCme->isEmpty() )
		{
			pthread_cond_timedwait(m_threadCondition, &conditionMutex, &waitTime);
		}
		pthread_mutex_unlock(&conditionMutex);

		// get new data
		while ( !isEmpty() )
		{
			doRecvData();
		}

		// get order response
		while ( !et_g_readCme->isEmpty() )
		{
			cme_response_struct response;
			if(et_g_readCme->popData(response)){
              readCmeResponse(&response);
            }
		}

	} // End for

	DBG(TRACE_LOG_DEBUG, "Leave et_mainLoop.");
}


bool EtTradeProcess::addData(receive_data& _data)
{
	pthread_mutex_lock(&m_mutex);
	m_queue.push(_data);
	pthread_mutex_unlock(&m_mutex);
	return true;
}


bool EtTradeProcess::popData(receive_data& _data)
{
	pthread_mutex_lock(&m_mutex);

    if (m_queue.empty()) 
      { 
        pthread_mutex_unlock(&m_mutex);
        return false; 
      } 

	_data = m_queue.front();
	m_queue.pop();

	pthread_mutex_unlock(&m_mutex);

	return true;
}


bool EtTradeProcess::isEmpty()
{
  bool empty = false;
  pthread_mutex_lock(&m_mutex);
  empty =  m_queue.empty();
  pthread_mutex_unlock(&m_mutex);

  return empty;
}

void EtTradeProcess::doRecvData()
{
	receive_data data;

	if ( !popData(data) )
	{
		DBG(TRACE_LOG_DEBUG, "Opps.");
		return;
	}

	switch (data.data_type)
      {
      case type_logout: 
        doLogout(data); 
        break;

      case type_new_order:
        doNewOrder(data);
        break;
        
      case type_alter_order:
        doAlterOrder(data);
        break;
        
      case type_cancel_order:
        doCancelOrder(data);
        break;
        
      default:
        break;
      }
}

void EtTradeProcess::readCmeResponse(cme_response_struct* _res)
{
	if (_res == NULL)
	{
		DBG_ERR("The parameter is error.");
		return;
	}

	switch (_res->type)
	{
		case cme_login:
			et_g_globex->login_state = LOGIN_SUCCESS;
			break;

		case cme_logout:
          doCmeLogout();
			break;

		case cme_session_reject:
          doSessionReject(_res->sent_seq_num, _res->id);
			break;

		case cme_business_reject:
          doBusinessReject(_res->sent_seq_num, _res->id);
			break;

		case cme_heartbeat_request:
			et_g_globex->send_heartbeat(_res->text);
			break;

		case cme_resend_request:
			et_g_globex->send_resend_request(_res->seq_num, 0);
			break;

		case cme_resend_order:
			et_g_globex->send_resend_order(_res->seq_num, _res->recv_num - 1);
			break;

		case cme_order_reject:
          doOrderRejectedResponse(_res->cli_order_id, _res->id);
			break;

		case cme_order_accepted:
          doAcceptedResponse(_res->cli_order_id, _res->ex_order_id, _res->id);
			break;

		case cme_order_filled_spread:
          doFilledResponse(_res->ex_order_id, _res->price, _res->size, _res->id, _res->symbol);
			break;

    case cme_order_filled_leg:
      doFilledLeg(_res->id, _res->size, _res->symbol);
      break;

		case cme_order_cancelled:
          doCancelledResponse(_res->cli_order_id, _res->id);
			break;

        case cme_cancelalter_reject:
          // do nothing
          break;

		case cme_order_altered:
          doAlteredResponse(_res->cli_order_id, _res->size, _res->price, _res->ex_order_id, _res->id);
			break;
			
		default:
			DBG(TRACE_LOG_ERR, "The type is error: %d", _res->type);
			break;
	}
}

void EtTradeProcess::doLogout(receive_data& _data) 
{
  g_cli_num--;

  map<string, int>::iterator it;  
  it = g_socket.begin();
  while(it != g_socket.end()){ 
    if(it->first.compare(_data.id) == 0){
      g_socketListPtr->deleteSocketFromList(it->second);

      // send client logout information to positionsrv
      EtClientMessage csMsg;
      csMsg.putField(fid_msg_type, msg_posi_calendar_exit);
      csMsg.putField(fid_posi_account, _data.account);
  
      if ( !csMsg.sendMessage(g_posiSocket) )  
        {  
			DBG(TRACE_LOG_ERR, "Failed to send position message.");
			return;
        }

      g_socket.erase(it);
      break;
    }
    it++; 
  }

  int iaccount = EtReadConfig::getAccountId(_data.account);
  et_g_readClient[iaccount].stopReadData();

  if(g_cli_num == 0){ 
    sleep(5);
    et_g_globex->send_logout(); 
  }
}

bool EtTradeProcess::doCmeLogout()
{
  et_g_readCme->stopReadCme();
  et_g_globex->login_state = NOT_LOGGED_IN; 

  // send exit to position server   
  EtClientMessage csMsg;    
  csMsg.putField(fid_msg_type, msg_posi_exit);  
  csMsg.putField(fid_posi_seq_num, et_g_readCme->recv_order_message_num);  
     
  if ( !csMsg.sendMessage(g_posiSocket) )    
    {    
		DBG(TRACE_LOG_ERR, "Failed to send position message.");
    }    
     
  if (g_posiSocket != -1)    
    {    
      close(g_posiSocket);    
      g_posiSocket = -1;    
    }

  et_g_stopProcess = true;

  return true;
}

bool EtTradeProcess::doNewOrder(receive_data& _data)  
{
	DBG_DEBUG("doNewOrder");

  char* cstr;
  cstr = new char [_data.symbol.size()+1];
  strcpy (cstr, _data.symbol.c_str());

  if(!et_g_globex->place_order(_data.account, _data.tag50, _data.local_num, cstr, _data.side, _data.price, _data.size, _data.max_show, DAY, LIMIT_ORDER)) 
    { 
		DBG_ERR("fail to send an order, name: %s, side: %d, price: %d, size: %d", _data.symbol.c_str(), _data.side, _data.price, _data.size);
      EtClientMessage csMsg; 
      csMsg.putField(fid_msg_type, ord_msg_error); 
      csMsg.putField(fid_msg_code, er_fail_send_new); 
      csMsg.putField(ord_fid_local_order_num, _data.local_num); 
 
      if ( !csMsg.sendMessageWithId(_data.id) )  
        {  
			DBG_ERR("fail to send message to client.");
			return false;  
        } 
      return false; 
    } 
 
  return true; 
} 

bool EtTradeProcess::doAlterOrder(receive_data& _data) 
{ 
  order_struct alterOrder; 
  if (-1 == et_g_orderBook->get_order(_data.global_num, &alterOrder)) 
    { 
		DBG_ERR("fail to find the order: %d", _data.local_num);
		return false; 
    } 
 
  if ( !et_g_globex->alter_order(_data.account, _data.tag50, alterOrder.global_order_id, _data.price, _data.size, _data.max_show) ) 
    { 
		DBG_ERR("fail to alter an order, stage: %d, name: %s, side: %d, price: %d, size: %d.", alterOrder.stage, alterOrder.display_name, 
				alterOrder.side, alterOrder.price, alterOrder.total_size);
      EtClientMessage csMsg;  
      csMsg.putField(fid_msg_type, ord_msg_error);  
      csMsg.putField(fid_msg_code, er_fail_send_alter);
      csMsg.putField(ord_fid_local_order_num, _data.local_num);  
  
      if ( !csMsg.sendMessageWithId(_data.id) )
        {   
			DBG_ERR("fail to send message to client.");
			return false;   
        } 
 
      return false; 
    } 
 
  DBG_DEBUG("success send alter an order, name: %s, side: %d, price: %d, size: %d.", 
            alterOrder.display_name, alterOrder.side, alterOrder.price, alterOrder.total_size);

  return true;
}

bool EtTradeProcess::doCancelOrder(receive_data& _data)
{
  char reason[128]; 
  memset(reason, 0, sizeof(reason));

  if ( et_g_globex->cancel_order(_data.account, _data.tag50, _data.global_num, reason) )
    { 
      return true; 
    } 
  else 
    {
      EtClientMessage csMsg;   
      csMsg.putField(fid_msg_type, ord_msg_error);
      csMsg.putField(fid_msg_code, er_fail_send_cancel);
      csMsg.putField(ord_fid_local_order_num, _data.local_num);
   
      if ( !csMsg.sendMessageWithId(_data.id) )    
        {    
			DBG_DEBUG("Fail to send cancel command to server.");
			return false;    
        }

      return false; 
    } 

  return true;
}


bool EtTradeProcess::doAcceptedResponse(const char* _cli_order_id, const char* _ex_order_id, string _id)
{
	DBG_DEBUG("doAcceptedResponse");

	order_struct order;
	if ( !et_g_orderBook->accepted_modify_order(_cli_order_id, _ex_order_id, &order) )
	{
		DBG_ERR("fail to modify order list after it was accepted successfully.");
		return false;
	}

	DBG_DEBUG("success an order: local_num: %d, side: %d, price: %d, size: %d", order.global_order_id,
              order.side, order.price, order.open_size);

	// send order information to client.
	EtClientMessage csMsg;
	csMsg.putField(fid_msg_type, ord_msg_confirm_new);
	csMsg.putField(ord_fid_local_order_num, order.local_order_id);
    csMsg.putField(ord_fid_global_order_num, order.global_order_id);

	if ( !csMsg.sendMessageWithId(_id) )
	{
		DBG_ERR("fail to send new order information to client.");
		return false;
	}

    if(g_need_clean_order){ 
      stopOrderProcess(order.global_order_id, order.local_order_id, _id);
      et_g_orderBook->copy_remove_order(order.global_order_id);
      et_g_orderBook->remove_order(order.global_order_id);  
      et_g_orderBook->print_orders();  

      if(et_g_orderBook->get_order_num() == 0){ 
        g_need_clean_order = false; 
      } 
    } 

	return true;
}

// spread fill msg
bool EtTradeProcess::doFilledResponse(const char* _ex_order_id, int _fill_price, int _fill_size,  string _id, const char* _symbol)
{
	DBG_DEBUG("doFilledResponse: _id=%s.", _id.c_str());
  
	order_struct order;
	if ( !et_g_orderBook->filled_modify_order(_ex_order_id, _fill_size, &order, _symbol) )
	{
      order_struct remove_order;
      if( et_g_orderBook->find_in_remove_list(_ex_order_id, _fill_size, &remove_order, _symbol)){

        EtClientMessage csMsg;
        csMsg.putField(fid_msg_type, ord_msg_fill_spread);
        csMsg.putField(ord_fid_local_order_num, remove_order.local_order_id);  
        csMsg.putField(ord_fid_side, remove_order.side);
        csMsg.putField(ord_fid_size, _fill_size);
  
        if ( !csMsg.sendMessageWithId(_id) )  
          {  
			  DBG_ERR("fail to send new order information to client.");
			  return false;  
          }

        if(remove_order.open_size == 0){
          et_g_orderBook->remove_order_in_remove_list(remove_order.global_order_id);
        }
        return true;
      }
      else{
		  DBG_ERR("fail to modify order after fill.");
		  return false;
      }
	}
	DBG_DEBUG("local_num: %d, side: %d", order.global_order_id, order.side);

    EtClientMessage csMsg; 
    csMsg.putField(fid_msg_type, ord_msg_fill_spread);
    csMsg.putField(ord_fid_local_order_num, order.local_order_id); 
    csMsg.putField(ord_fid_price, _fill_price);
    csMsg.putField(ord_fid_size, _fill_size);
 
    if ( !csMsg.sendMessageWithId(_id) ) 
      { 
		  DBG_ERR("fail to send new order information to client.");
		  return false; 
      }

	// delete order from list                                                                                                                                                                     
    if(order.open_size == 0){
      et_g_orderBook->remove_order(order.global_order_id);
    }
    et_g_orderBook->print_orders();

    EtReadConfig::m_fill_sum += _fill_size;
    check_limit();

    string account = _id.substr(0, 5);

    int iaccount = EtReadConfig::getAccountId(account);
    if(iaccount != -1){
      (EtReadConfig::m_account[iaccount].fill)++;
    }
	return true;
}

bool EtTradeProcess::check_limit()
{
	DBG_DEBUG("m_fill_sum=%d", EtReadConfig::m_fill_sum);

  if(EtReadConfig::m_fill_sum >= EtReadConfig::m_fill_limit)
    {
      g_socketListPtr->sendStopTradeToServers();
      EtReadConfig::m_fill_sum = 0;
	  DBG_ERR("max fill size limit reached, stop trading");
    }

  return true;
}


// spread leg fill msg
bool EtTradeProcess::doFilledLeg(string _id, int _size, const char* _symbol)
{ 
	DBG_DEBUG("doFilledLeg, id=%s", _id.c_str());

  EtClientMessage csMsg;
  csMsg.putField(fid_msg_type, ord_msg_fill_leg); 
  csMsg.putField(ord_fid_symbol, _symbol); 
  csMsg.putField(ord_fid_size, _size); 
  
  if ( !csMsg.sendMessageWithId(_id) )  
    {  
		DBG_ERR("fail to send new order information to client.");
		return false;  
    }
 
  return true;
} 


bool EtTradeProcess::doSessionReject(int _sent_seq_num, string _id)
{
	DBG_DEBUG("sent_seq_num=%d", _sent_seq_num);

    order_struct order;
    if (et_g_orderBook->get_order_by_seq_num(_sent_seq_num, &order) == -1)
	{
		DBG_ERR("can't find order, _sent_seq_num: %d", _sent_seq_num);
		return false;
	}

    EtClientMessage csMsg;
    csMsg.putField(fid_msg_type, ord_msg_session_reject);
    csMsg.putField(ord_fid_local_order_num, order.local_order_id); 
 
    if ( !csMsg.sendMessageWithId(_id) ) 
      { 
		  DBG_ERR("fail to send new order information to client.");
		  return false; 
      }

    if (et_g_orderBook->remove_order(order.global_order_id) == -1)
        {
			DBG_ERR("Fail to remove order, local_num: %d, name: %s", order.global_order_id,
					order.display_name);
			
			return false;
        }

    return true;
}

bool EtTradeProcess::doBusinessReject(int _sent_seq_num, string _id)
{
  order_struct order; 
  if (et_g_orderBook->get_order_by_seq_num(_sent_seq_num, &order) == -1) 
    { 
		DBG_ERR("can't find order, _sent_seq_num: %d", _sent_seq_num);
		return false; 
    } 
 
  EtClientMessage csMsg;
  csMsg.putField(fid_msg_type, ord_msg_business_reject); 
  csMsg.putField(ord_fid_local_order_num, order.local_order_id);
  
  if ( !csMsg.sendMessageWithId(_id) )  
    {  
		DBG_ERR("fail to send new order information to client.");
		return false;  
    } 
 
  if (et_g_orderBook->remove_order(order.global_order_id) == -1) 
    { 
		DBG_ERR("Fail to remove order, local_num: %d, name: %s", order.global_order_id, 
				order.display_name);
		return false; 
    }

  return true;
}

bool EtTradeProcess::doOrderRejectedResponse(const char* _cli_order_id, string _id)
{
	order_struct order;
	if ( !et_g_orderBook->get_order_by_client_order_id(_cli_order_id, &order) )
	{
		DBG_ERR("fail to get an order.");
		return false;
	}

    EtClientMessage csMsg;
    csMsg.putField(fid_msg_type, ord_msg_order_reject);
    csMsg.putField(ord_fid_local_order_num, order.local_order_id);
  
    if ( !csMsg.sendMessageWithId(_id) )  
      {  
		  DBG_ERR("fail to send order reject to client.");
		  return false;  
      }

	if (et_g_orderBook->remove_order(order.global_order_id) == -1)
	{
		DBG_ERR("Fail to remove order, local_num: %d, name: %s", order.global_order_id, 
				order.display_name);
		return false;
	}

	return true;
}

bool EtTradeProcess::doCancelledResponse(const char*_cli_order_id, string _id)
{
	order_struct order;
	
	if ( !et_g_orderBook->get_order_by_client_order_id(_cli_order_id, &order) )
	{
      order_struct remove_order; 
      if( et_g_orderBook->get_order_by_client_order_id_in_remove_list(_cli_order_id, &remove_order)){ 

        EtClientMessage csMsg;
        csMsg.putField(fid_msg_type, ord_msg_confirm_cancel);
        csMsg.putField(ord_fid_local_order_num, remove_order.local_order_id);

        if ( !csMsg.sendMessageWithId(_id) )
          {
			  DBG_ERR("failed to send cancel confirm message.");
			  return false;   
          }

        if (et_g_orderBook->remove_order_in_remove_list(remove_order.global_order_id) == -1) 
          {
			  DBG_ERR("Fail to remove order in remove list, local_num: %d, name: %s", remove_order.global_order_id, remove_order.display_name);
			  return false; 
          } 

        if(et_g_orderBook->get_order_num_in_remove_list() == 0)
          {
			  DBG_DEBUG("All orders cancelled.");
          }

        return true; 
      }
      else{
		  DBG_ERR("Fail to remove order, local_num: %d, name: %s", order.global_order_id,
				  order.display_name);
		  return false; 
      }
	}

    EtClientMessage csMsg; 
    csMsg.putField(fid_msg_type, ord_msg_confirm_cancel); 
    csMsg.putField(ord_fid_local_order_num, order.local_order_id); 
 
    if ( !csMsg.sendMessageWithId(_id) ) 
      { 
		  DBG_ERR("failed to send cancel confirm message.");
		  return false;    
      }

    if (et_g_orderBook->remove_order(order.global_order_id) == -1)
      {
		  DBG_ERR("Fail to remove order, local_num: %d, name: %s", order.global_order_id,
				  order.display_name);
		  return false;
      }

	return true;
}

bool EtTradeProcess::stopOrderProcess(int _global_order_id, int _local_order_id, string _id)
{
  char reason[128];
  memset(reason, 0, sizeof(reason));

  string account = _id.substr(0, 5);
  string tag50 = _id.substr(6);

  if ( et_g_globex->cancel_order(account, tag50, _global_order_id, reason) ) 
    {  
      return true;  
    }  
  else  
    { 
      EtClientMessage csMsg;    
      csMsg.putField(fid_msg_type, ord_msg_error); 
      csMsg.putField(fid_msg_code, er_fail_send_cancel); 
      csMsg.putField(ord_fid_local_order_num, _local_order_id); 
      
      if ( !csMsg.sendMessageWithId(_id) )
        {     
			DBG_ERR("fail to send message to client.");
			return false;     
        } 
      
      return false;  
    }  
}


bool EtTradeProcess::doAlteredResponse(const char* _cli_order_id, int _size, int _price, const char* _ex_order_id, string _id)
{
	order_struct order;
	if ( !et_g_orderBook->altered_modify_order(_cli_order_id, _size, _price, _ex_order_id, &order) )
	{
		DBG_ERR("fail to alter an order.");
		return false;
	}

	DBG_DEBUG("Success to alter order, side: %d, size: %d, price: %d, customer order id: %s, cme order id: %s", 
			  order.side, _size, _price, _cli_order_id, _ex_order_id);

    if(g_need_clean_order){
      stopOrderProcess(order.global_order_id, order.local_order_id, _id);
      et_g_orderBook->copy_remove_order(order.global_order_id);
      et_g_orderBook->remove_order(order.global_order_id); 

      et_g_orderBook->print_orders(); 
      if(et_g_orderBook->get_order_num() == 0){
        g_need_clean_order = false;
      }
    }

	// send order information to client.
	EtClientMessage csMsg;
	csMsg.putField(fid_msg_type, ord_msg_confirm_alter);
	csMsg.putField(ord_fid_local_order_num, order.local_order_id);
    csMsg.putField(ord_fid_size, _size);
    csMsg.putField(ord_fid_price, _price);
    
	if ( !csMsg.sendMessageWithId(_id) )
	{
		DBG_ERR("fail to send altered information to client.");
		return false;
	}

	return true;
}
