
#include "trace_log.h"
#include "util.h"
#include "msg_protocol.h"
#include "client_message.h"
#include "trade_socket_list.h"

#include <string>
#include <map>
using namespace std;

EtTradeSocketList::EtTradeSocketList()
{
	pthread_mutex_init(&m_mutex, NULL);
}


EtTradeSocketList::~EtTradeSocketList()
{
	pthread_mutex_lock(&m_mutex);

    m_socketList.clear();
	
	pthread_mutex_unlock(&m_mutex);	
	pthread_mutex_destroy(&m_mutex);
}


bool EtTradeSocketList::addSocketToList(int _socket)
{
	// add the new socket to the socket list
	pthread_mutex_lock(&m_mutex);

	m_socketList.push_back(_socket);
	pthread_mutex_unlock(&m_mutex);
	
	return true;
}

bool EtTradeSocketList::deleteSocketFromList(int _socket)
{
    pthread_mutex_lock(&m_mutex);

    int i = 0; 
    vector<int>::iterator vi = m_socketList.begin();  
    while (vi != m_socketList.end()) 
      { 
        if(*vi == _socket) 
          { 
            m_socketList.erase(m_socketList.begin() + i); 
            break; 
          } 
        else 
          { 
            vi++; 
            i++; 
          } 
      } 

    pthread_mutex_unlock(&m_mutex);

    return true;
}

bool EtTradeSocketList::cleanSocketList()
{
    pthread_mutex_lock(&m_mutex);
    vector<int>::iterator vi = m_socketList.begin();
    while (vi != m_socketList.end())
        {
	    sendExitMsg(*vi);
	    close(*vi);
	    *vi = -1;
	    vi++;
	}
    m_socketList.clear();
    pthread_mutex_unlock(&m_mutex);

    return true;
}

bool EtTradeSocketList::sendDataToServers(const char* _data, int _len)
{
	if (_data == NULL || _len <= 0)
	{
		DBG_ERR("parameter is error.");
		return false;
	}
	
	pthread_mutex_lock(&m_mutex);
	vector<int>::iterator vi = m_socketList.begin();
	while (vi != m_socketList.end())
	{
	    if ( !EtUtil::sendData(*vi, _data, _len) )
		{
		    close(*vi);
		    vi = m_socketList.erase(vi);
			DBG(TRACE_LOG_WARN, "There is a connection is closed.");
		}
	    else
		{
		    vi++;
		}
	}
	
	pthread_mutex_unlock(&m_mutex);
	return true;
}

bool EtTradeSocketList::sendStopTradeToOneServer(int _socket) 
{ 
  EtClientMessage ecm; 
  ecm.putField(fid_msg_type, ord_msg_stop_trade); 
     
  char *msgPtr = NULL; 
  int msgLen = 0; 
     
  if ( !ecm.getMessage(&msgPtr, msgLen) ) 
    { 
		DBG_ERR("fail to get message.");
		return false; 
    } 
     
  if ( !EtUtil::sendData(_socket, msgPtr, msgLen) ) 
    { 
      close(_socket); 
	  DBG_ERR("There is a connection is closed");
    }

  return true; 
}


bool EtTradeSocketList::sendStopTradeToServers()
{
	EtClientMessage ecm;
	ecm.putField(fid_msg_type, ord_msg_stop_trade);
	
	char *msgPtr = NULL;
	int msgLen = 0;
	
	if ( !ecm.getMessage(&msgPtr, msgLen) )
	{
		DBG_ERR("fail to get message.");
		return false;
	}
	
	if ( !sendDataToServers(msgPtr, msgLen) )
	{
		DBG_ERR("fail to send the command stoped trade to all server.");
		return false;
	}
	return true;
}

bool EtTradeSocketList::sendExitMsg(int _socket)
{
    if(_socket != -1)
	{
	    EtClientMessage csMsg;
	    csMsg.putField(fid_msg_type, msg_posi_exit);
        
	    if ( !csMsg.sendMessage(_socket) )
		{
			DBG(TRACE_LOG_ERR, "Failed to send exit message.");
		}
	}
    return true;
}
