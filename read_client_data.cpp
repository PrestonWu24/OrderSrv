#include <stdlib.h> 
#include <stdio.h> 
#include <unistd.h> 
#include <string.h> 
#include <errno.h> 
#include <sys/types.h>  
#include <sys/socket.h>  
#include <algorithm> 
#include "trace_log.h" 
#include "msg_protocol.h" 
#include "read_config.h" 
#include "read_client_data.h" 
#include "trade_process.h"

extern EtTradeProcess *et_g_tradeProcess; 
extern map<string, int> g_socket;

EtReadClientData::EtReadClientData()
{
	// set flag
	m_endFlag[0] = cMsgEqual;
	m_endFlag[1] = cMsgEndChar;
	m_endFlagLen = 2;
}


EtReadClientData::~EtReadClientData()
{
}

void EtReadClientData::setThreadCondition(pthread_cond_t* _cond)  
{  
  m_threadCondition = _cond;  
}  
 
 
void EtReadClientData::setId(string _id) 
{ 
  m_id = _id; 
}

bool EtReadClientData::parseBuffer(const char* _buffer, int _length)
{
	DBG_DEBUG("parse msg: %s", _buffer);

  EtClientMessage csMsg; 
  if ( !csMsg.parseMessage(_buffer, _length) ) 
    { 
		DBG(TRACE_LOG_ERR, "Fail to parse the client message.");
		return false; 
    } 
 
  int iMsgType = 0; 
  if ( !csMsg.getFieldValue(fid_msg_type, iMsgType) ) 
    { 
		DBG(TRACE_LOG_ERR, "Fail to get the fid_msg_type value.");
		return false; 
    } 

  switch (iMsgType) 
    { 
    case ord_msg_login: 
      // do nothing 
      break; 
         
    case ord_msg_logout: 
      doClientLogout(csMsg); 
      break; 
 
    case ord_msg_new_order: 
      doNewOrder(csMsg); 
      break; 
         
    case ord_msg_alter_order: 
      doAlterOrder(csMsg); 
      break; 
         
    case ord_msg_cancel_order: 
      doCancelOrder(csMsg); 
      break; 
         
    case ord_msg_product_name: 
      loadInstrument(csMsg); 
      break; 
           
    default: 
		DBG(TRACE_LOG_ERR, "The MsgType %d is error.", iMsgType);
		return false; 
    }
  return true;
}

bool EtReadClientData::doClientLogout(EtClientMessage& _csMsg)  
{  
  receive_data data;  
  data.data_type = type_logout;   
   
  string account;    
  if ( !_csMsg.getFieldValue(ord_fid_account, account))   
    {     
		DBG_ERR("fali to get client account value.");
		return false;     
    }  
  data.account = account;  
  
  string clientTag50;   
  if ( !_csMsg.getFieldValue(ord_fid_tag50, clientTag50) )    
    {    
		DBG_ERR("fali to get client tag 50 value.");
		return false;    
    }  
  data.tag50 = clientTag50;  

  string id = account + clientTag50;  
  data.id = id;
  
  et_g_tradeProcess->addData(data);  
  pthread_cond_signal(m_threadCondition);
  return true; 
}

bool EtReadClientData::doNewOrder(EtClientMessage& _csMsg)  
{  
  receive_data data;  
  data.data_type = type_new_order; 
 
  string account;  
  if ( !_csMsg.getFieldValue(ord_fid_account, account)) 
    {   
		DBG_ERR("fali to get client account value.");
		return false;   
    } 
  data.account = account; 
 
  string clientTag50; 
  if ( !_csMsg.getFieldValue(ord_fid_tag50, clientTag50) )  
    {  
		DBG_ERR("fali to get client tag 50 value.");
		return false;  
    } 
  data.tag50 = clientTag50; 
 
  string id = account + clientTag50; 
  data.id = id; 
 
  int iVal = 0;  
  if ( !_csMsg.getFieldValue(ord_fid_local_order_num, iVal) )  
    {   
		DBG_ERR("fail to get ord_fid_local_order_num value.");
		return false;   
    }  
  data.local_num = iVal;  
  
  string symbol;  
  if ( !_csMsg.getFieldValue(ord_fid_symbol, symbol) )    
    {     
		DBG_ERR("fail to get ord_fid_symbol value.");
		return false;     
    }    
  data.symbol = symbol;  
  
  if ( !_csMsg.getFieldValue(ord_fid_side, iVal) )   
    {    
		DBG_ERR("fail to get ord_fid_side value.");
		return false;    
    }   
  data.side = iVal;

  if ( !_csMsg.getFieldValue(ord_fid_size, iVal) )    
    {     
		DBG_ERR("fail to get ord_fid_size value.");
		return false;     
    }    
  data.size = iVal;  
  
  if ( !_csMsg.getFieldValue(ord_fid_price, iVal) )    
    {     
		DBG_ERR("fail to get ord_fid_price value.");
		return false;     
    }    
  data.price = iVal;  
 
  if ( !_csMsg.getFieldValue(ord_fid_max_show, iVal) )    
    {  
		DBG_ERR("fail to get ord_fid_matrix_row value.");
		return false;     
    }    
  data.max_show = iVal;  
  
  et_g_tradeProcess->addData(data); 
  pthread_cond_signal(m_threadCondition);
  return true;  
}

bool EtReadClientData::doAlterOrder(EtClientMessage& _csMsg)  
{  
  receive_data data;   
  data.data_type = type_alter_order; 
 
  string account;   
  if ( !_csMsg.getFieldValue(ord_fid_account, account))  
    {    
		DBG_ERR("fali to get client account value.");
      return false;    
    }  
  data.account = account; 
 
  string clientTag50;  
  if ( !_csMsg.getFieldValue(ord_fid_tag50, clientTag50) )   
    {   
		DBG_ERR("fali to get client tag 50 value.");
      return false;   
    }  
  data.tag50 = clientTag50; 
  
  string id = account + clientTag50;  
  data.id = id; 
  int iVal; 
  if ( !_csMsg.getFieldValue(ord_fid_global_order_num, iVal) )  
    { 
		DBG_ERR("fail to get ord_fid_global_order_num value.");
		return false;   
    }  
  data.global_num = iVal;  
 
  if ( !_csMsg.getFieldValue(ord_fid_size, iVal) ) 
    {    
		DBG_ERR("fail to get ord_fid_size value.");
		return false;    
    }   
  data.size = iVal; 
 
  if ( !_csMsg.getFieldValue(ord_fid_price, iVal) )   
    {    
		DBG_ERR("fail to get ord_fid_price value.");
		return false;    
    }   
  data.price = iVal;

  if ( !_csMsg.getFieldValue(ord_fid_max_show, iVal) )   
    { 
		DBG_ERR("fail to get fid_msg_matrix_row value.");
		return false;
    }   
  data.max_show = iVal; 
 
  et_g_tradeProcess->addData(data); 
  pthread_cond_signal(m_threadCondition);
  return true;  
}

bool EtReadClientData::doCancelOrder(EtClientMessage& _csMsg)  
{  
  receive_data data;   
  data.data_type = type_cancel_order; 
 
  string account;   
  if ( !_csMsg.getFieldValue(ord_fid_account, account))  
    {    
		DBG_ERR("fali to get client account value.");
		return false;    
    }  
  data.account = account; 
  
  string clientTag50;  
  if ( !_csMsg.getFieldValue(ord_fid_tag50, clientTag50) )   
    {   
		DBG_ERR("fali to get client tag 50 value.");
		return false;   
    }  
  data.tag50 = clientTag50; 
  
  string id = account + clientTag50;  
  data.id = id; 
 
  int iVal; 
  if ( !_csMsg.getFieldValue(ord_fid_global_order_num, iVal) )   
    {  
		DBG_ERR("fail to get ord_fid_global_order_num value.");
		return false;    
    }   
  data.global_num = iVal; 
  
  et_g_tradeProcess->addData(data); 
  pthread_cond_signal(m_threadCondition);
  return true;  
}

bool EtReadClientData::loadInstrument(EtClientMessage& _csMsg) 
{ 
  if ( !_csMsg.getFieldValue(ord_fid_product_name, m_product_name) ) 
    { 
		DBG_ERR("fail to get fid_msg_product_name value.");
		return false; 
    } 

  DBG(TRACE_LOG_DEBUG, "The global product name is %s.", m_product_name.c_str() );
     
  if ( !EtReadConfig::loadInstrument(m_product_name.c_str()) ) 
    { 
		DBG_ERR("fail to read instrument file.");
      exit(-1); 
    } 
 
  return true; 
}
