#include <stdlib.h> 
#include <stdio.h> 
#include <unistd.h> 
#include <string.h> 
#include <errno.h> 
#include <sys/types.h>  
#include <sys/socket.h>  
#include <algorithm> 
#include <vector>
#include "trace_log.h" 
#include "msg_protocol.h" 
#include "read_config.h" 
#include "monitor.h" 
#include "trade_process.h"
#include "exec_globex.h"
#include "trade_socket_list.h"

extern EtTradeProcess *et_g_tradeProcess; 
extern exec_globex* et_g_globex;
extern EtTradeSocketList* g_socketListPtr;
extern map<string, int> g_socket;

EtMonitor::EtMonitor()
{
	// set flag
	m_endFlag[0] = cMsgEqual;
	m_endFlag[1] = cMsgEndChar;
	m_endFlagLen = 2;
}


EtMonitor::~EtMonitor()
{
}

bool EtMonitor::startSendQuote()
{
  int iRes = pthread_create(&m_quoteThread, NULL, sendQuoteThread, this);
  if (iRes != 0)
    {
		DBG_ERR("Fail to run the sendQuoteThread");
		return false;
    }
  return true;
}

void* EtMonitor::sendQuoteThread(void* arg)
{
	DBG_DEBUG("start sendQuoteThread");

  EtMonitor* myObj = (EtMonitor*)arg;

  while(1)
    {
      if(myObj->m_exitThread)
        {
          break;
        }

      sleep(QUOTE_INTERVAL);

      vector<struct_account>::iterator it; 
      it = EtReadConfig::m_account.begin(); 
      while(it != EtReadConfig::m_account.end()){ 
        EtClientMessage qcsMsg; 
        qcsMsg.putField(fid_msg_type, ord_msg_quote); 
        qcsMsg.putField(ord_fid_account, (*it).account);
        qcsMsg.putField(ord_fid_quote_num, (*it).quote); 
        qcsMsg.putField(ord_fid_fill, (*it).fill);
        if ( !qcsMsg.sendMessage(myObj->m_recvSocket) )   
          { 
			  DBG_ERR("failed to send quotes sum message to monitor."); 
			  return false;   
          } 
        else{ 
        }
        it++; 
      }

    }

  return NULL;
}

bool EtMonitor::parseBuffer(const char* _buffer, int _length)
{
  EtClientMessage csMsg; 
  if ( !csMsg.parseMessage(_buffer, _length) ) 
    { 
		DBG_ERR("Fail to parse the client message.");
		return false; 
    } 
 
  int iMsgType = 0; 
  if ( !csMsg.getFieldValue(fid_msg_type, iMsgType) ) 
    { 
		DBG_ERR("Fail to get the fid_msg_type value.");
		return false; 
    } 

  switch (iMsgType)  
    { 
    case ord_msg_fill_limit:
      doFillLimit(csMsg);
      break;
 
    case ord_msg_stop_trade:
      doStopTrade(csMsg);
      break;

    case ord_msg_logout:  
      doLogout();
      break;

    default:  
		DBG_ERR("The MsgType %d is error.", iMsgType);
		return false;  
    }
 
  return true;
}

bool EtMonitor::doFillLimit(EtClientMessage& _csMsg)
{
  int limit;
  if ( !_csMsg.getFieldValue(ord_fid_fill_limit, limit) )
    {
      return false;  
    }

  EtReadConfig::m_fill_limit = limit;

  DBG_DEBUG("new fill limit=%d", EtReadConfig::m_fill_limit);
  return true;
}

bool EtMonitor::doStopTrade(EtClientMessage& _csMsg)
{
  string account;
  if ( !_csMsg.getFieldValue(ord_fid_account, account) )
    { 
      return false;   
    }

  DBG_DEBUG("stop Trade: %s", account.c_str());

  // get socket
  map<string, int>::iterator it;   
  it = g_socket.begin();
  while(it != g_socket.end()){  
    if(it->first.substr(0, 5).compare(account) == 0){
      g_socketListPtr->sendStopTradeToOneServer(it->second);
    } 
    it++;  
  } 

  return true;
}

bool EtMonitor::doLogout()
{
  m_exitThread = true;
  return true;  
}
