#include "string.h"
#include "stdio.h"
#include "stdlib.h"
#include "trace_log.h"
#include "util.h"
#include "msg_protocol.h"
#include "client_message.h"

using namespace std;
extern map<string, int> g_socket;

EtClientMessage::EtClientMessage()
{
	buffer_len = 0;
}


/**
 * add a field.
 */
bool EtClientMessage::putField(int _field, string& _val)
{
    if (_val.empty()) 
    {
		DBG(TRACE_LOG_ERR, "The parameter is empty.");
		return false;
    }

    m_inMap.insert( make_pair(_field, _val) );
    return true;
}


bool EtClientMessage::putField(int _field, const char* _val)
{
	string str = _val;
	return putField(_field, str);
}


/**
 * add a field.
 */
bool EtClientMessage::putField(int _field, int _val)
{
	string str = EtUtil::intToStr(_val);
	return putField(_field, str);
}


/**
 * add a field.
 */
bool EtClientMessage::putField(int _field, double _val)
{
	string str = EtUtil::doubleToStr(_val);
	return putField(_field, str);
}


bool EtClientMessage::getFieldValue(int _field, string& _returnVal)
{
	map<int, string>::iterator iter = m_outMap.find(_field);
	
	if (iter == m_outMap.end())
	{
		DBG_ERR("do not find the field: %d", _field);
		return false;
	}
	
	_returnVal = iter->second;
    return true;
}


bool EtClientMessage::getFieldValue(int _field, int& _returnVal)
{
	map<int, string>::iterator iter = m_outMap.find(_field);
	
	if (iter == m_outMap.end())
	{
		DBG_ERR("do not find the field: %d", _field);
		return false;
	}
	
	_returnVal = EtUtil::strToInt(iter->second);
    return true;
}


bool EtClientMessage::getFieldValue(int _field, double& _returnVal)
{
	map<int, string>::iterator iter = m_outMap.find(_field);
	
	if (iter == m_outMap.end())
	{
		DBG_ERR("do not find the field: %d", _field);
		return false;
	}
	
	_returnVal = EtUtil::strToDouble(iter->second);
    return true;
}


void EtClientMessage::clearInMap()
{
	m_inMap.clear();
}


/**
 * Build the message.
 * return:
 *    true: ok;
 *   false: error;
 */
bool EtClientMessage::buildMessage()
{
    if (m_inMap.empty())
    {
		DBG(TRACE_LOG_ERR, "The field's map is empty.");
		return false;
    }
    
    char* ptr = buffer;
    char* end_ptr = ptr + CLIENT_BUFFER_LEN;
     
    int writeLen = 0;
    
    memset(buffer, 0, CLIENT_BUFFER_LEN);
    map<int, string>::iterator iter;
    
    for (iter = m_inMap.begin(); iter != m_inMap.end(); iter++)
    {
    	writeLen = sprintf(ptr, "%d=%s", iter->first, iter->second.c_str());
    	ptr += writeLen;
    	
    	*ptr = cMsgFieldDelimiter;
    	ptr++;
    	
    	if (ptr >= end_ptr)
    	{
			DBG_ERR("buffer is overflow.");
			return false;
    	}
    }
    
    // buffer end
    writeLen = sprintf(ptr, "%d=%c", fid_msg_end, cMsgEndChar);
    ptr += writeLen;
    
    if (ptr >= end_ptr)
   	{
		DBG_ERR("buffer is overflow.");
		return false;
   	}
   	
   	buffer_len = ptr - buffer;

    return true;
}


bool EtClientMessage::getMessage(char** _buff, int& _len)
{
	if ( !buildMessage() )
	{
		DBG_ERR("fail to build the message.");
		return false;
	}
	
	*_buff = buffer;
	_len = buffer_len;
	return true;
}


bool EtClientMessage::sendMessage(int _socket)
{
	if (_socket == -1)
	{
		DBG_ERR("socket is error.");
		return false;
	}
	
	if ( !buildMessage() )
	{
		DBG_ERR("fail to build the message.");
		return false;
	}
	
	if ( !EtUtil::sendData(_socket, buffer, buffer_len) )
	{
		DBG_ERR("fail to send the message.");
		return false;
	}	
	return true;
}

bool EtClientMessage::sendMessageWithId(string _id)
{
  map<string, int>::iterator it;
  it = g_socket.find(_id);

  if(it == g_socket.end()){
	  DBG_ERR("fail to find id: %s", _id.c_str());

	  map<string, int>::iterator cit; 
	  cit = g_socket.begin(); 
	  while(cit != g_socket.end()){ 
		  DBG_DEBUG("map first: %s, second: %d: ", it->first.c_str(), it->second);
		  cit++; 
	  }

	  return false;
  }

  sendMessage(it->second);
  return true;
}

bool EtClientMessage::parseMessage(const char* _buffer, int _iLen)
{
	if (_buffer == NULL || _iLen <= 0)
	{
		DBG_ERR("buffer is NULL.");
		return false;
	}
	
    const char* ptr = _buffer;
    const char* end_ptr = _buffer + _iLen;
    
    int field = 0;
    string strVal;
    
    char temp[256];
    memset(temp, 0, 256);
    char* tmpPtr = temp;
    
    m_outMap.clear();
    
    while (ptr < end_ptr)
    {
        if (*ptr == cMsgEqual)
        {
        	*tmpPtr = 0;
            field = atoi(temp);
            
            tmpPtr = temp;
            ptr++;
        }
        else if (*ptr == cMsgFieldDelimiter)
        {
            *tmpPtr = 0;
            strVal = temp;
            m_outMap.insert( make_pair(field, strVal) );
            
            tmpPtr = temp;
            ptr++;
        }
        else if (*ptr == cMsgEndChar)
        {
        	break;
        }
        else
        {
        	*tmpPtr = *ptr;
        	tmpPtr++;
        	ptr++;
        }
    }
    //printOutMap();
    return true;
}


void EtClientMessage::printOutMap()
{
	map<int, string>::iterator iter;
    
	DBG_DEBUG("----------------");

    for (iter = m_outMap.begin(); iter != m_outMap.end(); iter++)
    {
		DBG_DEBUG("field: %d, value: %s", iter->first, iter->second.c_str());
    }
	DBG_DEBUG("----------------");
}

