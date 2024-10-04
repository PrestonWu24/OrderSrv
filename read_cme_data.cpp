
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/tcp.h>

#include "trace_log.h"
#include "globex_common.h"
#include "read_cme_data.h"
#include "exec_globex.h"
#include "audit_trail.h"
#include "util.h"
#include "client_message.h"
#include "msg_protocol.h"
#include "posi_fill.h"
#include "trade_process.h"

extern string g_product_name; 
using namespace std;

// ---- globe variable ----
extern exec_globex* et_g_globex;
extern audit_trail* et_g_auditTrail;
extern order_book* et_g_orderBook;
extern PosiFill* et_g_posiFill;
extern EtTradeProcess *et_g_tradeProcess;
// ---- globe variable end ----

EtReadCmeData::EtReadCmeData()
{
	m_stopFlag = false;

	memset(m_leaveBuff, 0, ORDER_BUFFER_SIZE);
	memset(m_cmeRecvBuff, 0, ORDER_BUFFER_SIZE);
	memset(m_parseBuff, 0, ORDER_BUFFER_SIZE);
	m_leaveLen = 0;
	m_parseLen = 0;
	m_recvSize = 0;

	// set flag
	memset(m_endFlag, 0, 5);
	m_endFlag[0] = SOH;
	m_endFlag[1] = '1';
	m_endFlag[2] = '0';
	m_endFlag[3] = '=';
	m_endFlagLen = 8;
	
    memset(m_sohFlag, 0, 2);
    m_sohFlag[0] = SOH;  
    m_sohFlag[1] = '\0';

	m_recvSocket = -1;
	m_threadCondition = NULL;
    m_auditCondition = NULL;
	recv_order_message_num = -1;
    bool_resend_request = false;
	
	pthread_mutex_init(&m_mutex, NULL);
}


EtReadCmeData::~EtReadCmeData()
{
	pthread_mutex_destroy(&m_mutex);
}


void EtReadCmeData::setRecvSocket(int _socket)
{
	m_recvSocket = _socket;
}


void EtReadCmeData::setThreadCondition(pthread_cond_t* _cond)
{
	m_threadCondition = _cond;
}

void EtReadCmeData::setAuditCondition(pthread_cond_t* _cond) 
{ 
  m_auditCondition = _cond; 
}

bool EtReadCmeData::startReadCme()
{
	// connect to server
	if (-1 == m_recvSocket)
	{
		DBG_ERR("recv socket is errro.");
      return false;
	}
	
	if (NULL == m_threadCondition)
	{
		DBG_ERR("thread condition is errro.");
      return false;
	}
	
	// start thread
	int iRes = pthread_create(&m_recvThread, 0, readOrderThread, this);
	if (iRes != 0)
	{
		DBG_ERR("fail to create a thread. %s.", strerror(iRes));
		return false;
	}

	/*
    iRes = pthread_create(&m_heartbeatThread, 0, cmeHeartbeatThread, this); 
    if (iRes != 0) 
      { 
		  DBG_ERR("fail to create a thread. %s.", strerror(iRes));
		  return false; 
      } 
	*/
	
	return true;
}


void EtReadCmeData::stopReadCme()
{
	m_stopFlag = true;
}

bool EtReadCmeData::isExit()
{
	return m_stopFlag;
}


bool EtReadCmeData::split_buffer()
{
	int len = 0;
    int leaveFlag = 0;
       
    char* buffEnd = m_parseBuff + m_parseLen;
    char* beginPtr = m_parseBuff;
    char* endPtr = NULL;
    char *cTempBegin = m_parseBuff;
    
    while (beginPtr < buffEnd)
    {
        endPtr = strstr(beginPtr, m_endFlag);
		if (endPtr == NULL)
	    {
	        leaveFlag = 1;
	        break;
	    }

        endPtr += 4; 
        int tempLength = endPtr - beginPtr; 
        cTempBegin += tempLength; 
        endPtr = strstr(cTempBegin, m_sohFlag); 
        if(endPtr == NULL){ 
          leaveFlag = 1; 
          break; 
        } 
 
        endPtr += 1;
	    len = endPtr - beginPtr;
		DBG_DEBUG("Parse CME: %.*s", len, beginPtr);
	    parse_fix(beginPtr, &endPtr);
	    
		beginPtr += len;
        cTempBegin += 4;
	}
	
	if (1 == leaveFlag)
	{
	    int leaveSize = buffEnd - beginPtr;
	    memcpy(m_leaveBuff, beginPtr, leaveSize);
        m_leaveLen = leaveSize;
	}
	
	//pthread_cond_signal(m_threadCondition);
    return true;
}


void* EtReadCmeData::readOrderThread(void* arg)
{
#ifdef DEBUG 
  sprintf(static_dbg, "[%s][%s][line: %d]Enter readOrderThread.", __FILE__,  __FUNCTION__, __LINE__); 
  add_trace_log(0, static_dbg); 
#endif 
	
	EtReadCmeData* myReadData = (EtReadCmeData*)arg;
		
	char* ptr = NULL;
	
	fd_set fds;
	int	res = 0;
	
    struct timeval timeout;	
	timeout.tv_sec  = 1;
	timeout.tv_usec = 0;
	
	while (true)
	{	
		if (myReadData->m_stopFlag)
		{
		    break;
		}
		
		FD_ZERO(&fds);
		FD_SET(myReadData->m_recvSocket, &fds);

    	res = select(myReadData->m_recvSocket + 1, &fds, NULL, NULL, &timeout);
		if (res < 0)
		{
			DBG_ERR("fail to select when receive data: %s.", strerror(errno));
			break;
		}
		else if (res == 0)
		{
			continue;
		}
		
		if (FD_ISSET(myReadData->m_recvSocket, &fds))
		{
			memset(myReadData->m_cmeRecvBuff, 0, ORDER_BUFFER_SIZE);
			myReadData->m_recvSize = recv(myReadData->m_recvSocket, myReadData->m_cmeRecvBuff, ORDER_BUFFER_SIZE, 0);
			if (myReadData->m_recvSize <= 0)
			{
				DBG_ERR("fail to read data. %s", strerror(errno));
				break;
			}
			DBG_DEBUG("READ CME %d:", myReadData->m_recvSize);
            memset(myReadData->m_parseBuff, 0, ORDER_BUFFER_SIZE * 2);

			if (myReadData->m_leaveLen == 0)
			{
				memcpy(myReadData->m_parseBuff, myReadData->m_cmeRecvBuff, myReadData->m_recvSize);
				myReadData->m_parseLen = myReadData->m_recvSize;
			}
			else
			{
				ptr = myReadData->m_parseBuff;
				memcpy(ptr, myReadData->m_leaveBuff, myReadData->m_leaveLen);
				ptr += myReadData->m_leaveLen;
				memcpy(ptr, myReadData->m_cmeRecvBuff, myReadData->m_recvSize);
				myReadData->m_parseLen = myReadData->m_leaveLen + myReadData->m_recvSize;
				
				memset(myReadData->m_leaveBuff, 0, ORDER_BUFFER_SIZE);
				myReadData->m_leaveLen = 0;
			}

			if(!myReadData->split_buffer()){
              return NULL;
            }
		}
	}
	
	if (myReadData->m_recvSocket != -1)
	{
		close(myReadData->m_recvSocket);
		myReadData->m_recvSocket = -1;
	}
	
	myReadData->m_stopFlag = true;
	
	DBG(TRACE_LOG_DEBUG, "Leave the readDataThread.");
	return NULL;
}

/*
void* EtReadCmeData::cmeHeartbeatThread(void* arg) 
{
	DBG_DEBUG("enter into cmeHeartbeatThread.");

	EtReadCmeData* myReadData = (EtReadCmeData*)arg;

  while(1)
    {
      if (myReadData->m_stopFlag) 
        { 
          break; 
        } 

      sleep(atoi(GLOBEX_HEART_BEAT_INTERVAL));
      et_g_globex->send_heartbeat("heartbeat");
    }

  DBG(TRACE_LOG_DEBUG, "Leave the cmeHeartbeatThread.");

  return NULL;
}
*/

bool EtReadCmeData::addData(cme_response_struct& _response)
{
	pthread_mutex_lock(&m_mutex);
	m_queue.push(_response);
	pthread_mutex_unlock(&m_mutex);
	return true;
}


bool EtReadCmeData::popData(cme_response_struct& _response)
{	
	pthread_mutex_lock(&m_mutex);
	
    if (m_queue.empty()) 
      { 
        pthread_mutex_unlock(&m_mutex);
        return false; 
      } 

	cme_response_struct data = m_queue.front();
	m_queue.pop();
	memcpy(&_response, &data, sizeof(cme_response_struct));

	pthread_mutex_unlock(&m_mutex);
	
	return true;
}


bool EtReadCmeData::isEmpty()
{
  bool empty = false; 
 
  pthread_mutex_lock(&m_mutex);
  empty =  m_queue.empty();
  pthread_mutex_unlock(&m_mutex);

  return empty;
}


int EtReadCmeData::parse_fix(char *buffer, char **buffer_ptr)
{
	fix_message_struct* fix_msg = NULL;
	
	fix_field_struct seq_field;

	int ret_val = 0;
	int new_seq_num = 0;

	for (;(ret_val = parse_fix_get_message(buffer, buffer_ptr, &fix_msg)) == 0;)
    {
    	// tag: 34 - MsgSeqNum
		if (get_fix_field(fix_msg, "34", &seq_field) == -1)
	    {
			DBG_ERR("fail to get 34 field.");
			return 0;
	    }
		new_seq_num = atoi(seq_field.field_val);
		DBG_DEBUG("PARSE SEQUENCE: %d", new_seq_num);
        
		if (recv_order_message_num == -1)
		{
			recv_order_message_num = new_seq_num;
			recv_order_message_num++;
		}
		else
		{
			if (new_seq_num == recv_order_message_num)
			{
				recv_order_message_num++;
                if(bool_resend_request){
                  bool_resend_request = false;
                }
			}
			else
			{
				// Range of messages to resend is 2500
				if ((new_seq_num - recv_order_message_num + 1) > 2500)
				{
					recv_order_message_num = new_seq_num - 2500 + 1;
				}
				
                if(!bool_resend_request){
                  cme_response_struct response;
                  memset(&response, 0, sizeof(cme_response_struct));
                  response.type = cme_resend_request;
                  response.seq_num = recv_order_message_num;

                  addData(response);
                  pthread_cond_signal(m_threadCondition);
                  bool_resend_request = true;
                }				
			}
		}

		if (strcmp(fix_msg->msg_type, "5") == 0)
	    {
			handle_globex_logout(fix_msg);
	    }
		else if (strcmp(fix_msg->msg_type, "A") == 0)
	    {
	    	recv_order_message_num = new_seq_num + 1;
			handle_globex_login(fix_msg);
	    }
		else if (strcmp(fix_msg->msg_type, "1") == 0)
	    {
			handle_globex_heartbeat_request(fix_msg);
	    }
		else if (strcmp(fix_msg->msg_type, "2") == 0)
	    {
			handle_globex_resend_reqest(fix_msg);
	    }
		else if (strcmp(fix_msg->msg_type, "3") == 0)
	    {
			handle_globex_session_reject(fix_msg);
	    }
		else if (strcmp(fix_msg->msg_type, "4") == 0)
	    {
	    	// YT, had to do this for the cert test but i think the logic is wrong
			//if (get_fix_field(fix_msg, "369", &last_seq_proc_field) == 0)
			//{
			//	recv_order_message_num = atoi(last_seq_proc_field.field_val);
			//}
			handle_globex_sequence_reqest(fix_msg);
	    }
		else if (strcmp(fix_msg->msg_type, "8") == 0)
	    {
			handle_globex_order_message(fix_msg);
	    }
		else if (atoi(fix_msg->msg_type) == 9)
	    {
	    	/* seems like they send "09" sometimes */
          // or modify reject
			handle_globex_cancel_reject_message(fix_msg);
	    }
		else if (strcmp(fix_msg->msg_type, "j") == 0)
	    {
	    	/* business reject */
			handle_globex_business_reject_message(fix_msg);
	    }
	    else if (strcmp(fix_msg->msg_type, "0") == 0)
	    {
	    }
	    else if (strcmp(fix_msg->msg_type, "b") == 0)
	    {
	    	handle_globex_quote_acknowledge(fix_msg);
	    }
		else
	    {
			DBG_ERR("unhandled msg_type = %s", fix_msg->msg_type);
	    }
	    
		free_fix_struct(fix_msg);
    }
    return 0;
}


/****************************************/
/* there is a memory leak in here		*/
/* fixed it in in ICE so check there	*/
/* for fix						        */
/* its only orders here so no big deal	*/
/*								        */
/* -YT							        */
/****************************************/
int EtReadCmeData::parse_fix_get_message(char *buffer, char **buffer_ptr, 
	fix_message_struct **fix_msg)
{
	fix_field_struct* fix_field = NULL;
	fix_field_struct* curr_field = NULL;
	char* curr_ptr = NULL;
	
	int field_size = 0;
	int ret_get_field = 0;
	int to_erase = 0;
	int to_move = 0;


	for (*fix_msg = 0, curr_ptr = buffer; ; curr_ptr += field_size)
	{
		ret_get_field = parse_fix_get_field(curr_ptr, buffer_ptr, &field_size, &fix_field);
		if (-1 == ret_get_field)
		{
			if (*fix_msg != 0)
			{
				free_fix_struct(*fix_msg);
			}
			return (-1);
		}

		if (fix_field == NULL)
		{
			continue;
		}

		// 8 = BeginString
		if (strcmp(fix_field->field_id, "8") == 0)
		{
			if (*fix_msg == NULL)
			{
				*fix_msg = (fix_message_struct*)malloc(sizeof(fix_message_struct));
				memset(*fix_msg, 0, sizeof(fix_message_struct));
			}
		}
		// 35 = MsgType
		else if (strcmp(fix_field->field_id, "35") == 0)
		{
			if (*fix_msg != 0)
			{
				(*fix_msg)->msg_type = strdup(fix_field->field_val);
			}
		}
		// 10 = CheckSum
		else if (strcmp(fix_field->field_id, "10") == 0)
		{
			curr_ptr += field_size;
			to_erase =  curr_ptr - buffer;
			to_move =  *buffer_ptr - curr_ptr;

			memmove(buffer, buffer + to_erase, to_move);
			(*buffer_ptr) -= to_erase;
			return (0);
		}
		else if (*fix_msg != 0)
		{
			if ((*fix_msg)->fields == 0)
		    {
				(*fix_msg)->fields = fix_field;
		    }
			else
		    {
				for (curr_field = (*fix_msg)->fields; curr_field->next != 0; curr_field = curr_field->next);
				curr_field->next = fix_field;
		    }
		}
	}

	if (*fix_msg != 0)
	{
		free_fix_struct(*fix_msg);
	}
	return (-1);
}


void EtReadCmeData::print_fix_struct(fix_message_struct *fix_msg) 
{
	fix_field_struct* curr_field = NULL;

	for (curr_field = fix_msg->fields; curr_field != NULL; curr_field = curr_field->next)
    {
      //DBG_DEBUG("field: id = %s, val = %s", curr_field->field_id, curr_field->field_val);
    }
}


int EtReadCmeData::free_fix_struct(fix_message_struct *fix_msg) 
{
	fix_field_struct* curr_field = NULL;
	fix_field_struct* to_del = NULL;

	curr_field = fix_msg->fields;
	while (curr_field != NULL)
    {
		to_del = curr_field;
		curr_field = curr_field->next;

		if (to_del->field_id != 0)
	    {
			free(to_del->field_id);
	    }
		if (to_del->field_val != 0)
	    {
			free(to_del->field_val);
	    }
		free (to_del);
    }

	if (fix_msg->msg_type != 0)
    {
		free(fix_msg->msg_type);
    }
	free(fix_msg);
	return 0;
}


/**
 * return: int
 *    0: ok
 *   -1: error
 */
int EtReadCmeData::parse_fix_get_field(char *buffer, char **buffer_ptr, int *field_size, 
	fix_field_struct **fix_field)
{
	char* field_end = NULL;
	char* field_start = NULL;

	int to_erase = 0;
	int to_move = 0;

	/* might just be waiting for the SOH leave buffer alone */
	if ( (field_end = (char *)memchr(buffer, SOH, *buffer_ptr - buffer) ) == 0)
    {
      *fix_field = 0;
      return (-1);
    }

	/* no equall means bad field move along in buffer */
	field_start = (char *)memchr(buffer, '=', field_end - buffer);
	if (field_start == NULL)
    {
		DBG_ERR("do not find =");
		field_end++;
		to_erase = field_end - buffer;
		to_move = *buffer_ptr - field_end;

		memmove(buffer, buffer + to_erase, to_move);
		(*buffer_ptr) -= to_erase;
		*fix_field = 0;
		*field_size = 0;
		return (0);
    }

	*fix_field = (fix_field_struct*)malloc(sizeof (fix_field_struct));
	memset(*fix_field, 0, sizeof(fix_field_struct));

	(*fix_field)->field_id = (char*)malloc(field_start - buffer + 1);
	(*fix_field)->field_val = (char*)malloc(field_end - field_start);
	
	memset((*fix_field)->field_id, 0, (field_start - buffer + 1));
	memset((*fix_field)->field_val, 0, (field_end - field_start));

	memcpy((*fix_field)->field_id, buffer, (field_start - buffer));
	memcpy((*fix_field)->field_val, field_start + 1, (field_end - field_start - 1));

	*field_size = (field_end + 1) - buffer;
	return (0);
}


/**
 * return:
 *    0: ok
 *   -1: error
 */
int EtReadCmeData::get_fix_field(fix_message_struct *fix_msg, const char *look_for, 
	fix_field_struct *ret_field) 
{
	fix_field_struct* curr_field = NULL;

	for (curr_field = fix_msg->fields; curr_field != NULL; curr_field = curr_field->next)
    {
		if (curr_field->field_id != NULL)
	    {
			if (strcmp(curr_field->field_id, look_for) == 0)
		    {
		    	memset(ret_field, 0, sizeof(fix_field_struct));
				memcpy(ret_field, curr_field, sizeof(fix_field_struct));
				return (0);
		    }
	    }
    }

	return (-1);
}

/**
 * handle login response.
 */
bool EtReadCmeData::handle_globex_login(fix_message_struct *fix_msg)
{
	fix_field_struct found_field;

	cme_response_struct response;
	memset(&response, 0, sizeof(cme_response_struct));
	response.type = cme_login;

    addData(response); 
    pthread_cond_signal(m_threadCondition);

	if (get_fix_field(fix_msg, "108", &found_field) == 0)
	{
	    if (strcmp(found_field.field_val, GLOBEX_HEART_BEAT_INTERVAL) == 0)
	    {

	    }

	    // add to audit trail                                                                                                                                                                 
	    audit_struct new_audit;
	    memset(&new_audit, 0, sizeof(new_audit));

	    build_audit_data(new_audit);
	    new_audit.direction = TO_CLIENT;
	    new_audit.status = OK;
	    new_audit.msg_type = LOGIN;
        et_g_globex->build_audit_time_local(new_audit);
	    et_g_auditTrail->add_audit(new_audit);
        pthread_cond_signal(m_auditCondition);
	    //return true;
	}   
	else
	{

	}

	return false;
}

/**
 * handle logout response.
 */
bool EtReadCmeData::handle_globex_logout(fix_message_struct *fix_msg)
{
	fix_field_struct found_field;
	fix_field_struct text_field;
	
	cme_response_struct response;
	memset(&response, 0, sizeof(cme_response_struct));
	response.type = cme_logout;

	//tag: 58 - reason or confirmation text
	if (get_fix_field(fix_msg, "58", &text_field) == 0)
	    {

	    }

    // tag: 789 - NextExpectedMsgSeqNum
	if (get_fix_field(fix_msg, "789", &found_field) == 0)
	    {
			DBG_DEBUG("logout from order server, expected sequence number: %s", found_field.field_val);
		
          //order_message_num = atoi(found_field.field_val);
          int seq_num = atoi(found_field.field_val);
          response.seq_num = seq_num;
          addData(response);
          pthread_cond_signal(m_threadCondition);
          
          // add audit trail
          audit_struct new_audit;
          memset(&new_audit, 0, sizeof(new_audit));
          
          build_audit_data(new_audit);
          new_audit.direction = TO_CLIENT;
          
          if(et_g_globex->sent_msg == LOGOUT_SENT)
		    {
              new_audit.status = OK;
              new_audit.msg_type = LOGOUT;
		    }
          else if(et_g_globex->login_state == LOGIN_SUCCESS)
		    {
              new_audit.status = OK;
              new_audit.msg_type = LOGIN;
		    }
          else
		    {
              new_audit.status = REJECT;
              new_audit.msg_type = LOGIN;
		    }
          
          strcpy(new_audit.reason, text_field.field_val);
          
          et_g_globex->build_audit_time_local(new_audit); 
          et_g_auditTrail->add_audit(new_audit);
          pthread_cond_signal(m_auditCondition);
          //return true;
	    }
	else
      {

      }

	return false;
}


/**
 * handle hearbeat reponse.
 */
void EtReadCmeData::handle_globex_heartbeat_request(fix_message_struct *fix_msg)
{
	fix_field_struct found_field;

	cme_response_struct response;
	memset(&response, 0, sizeof(cme_response_struct));
	response.type = cme_heartbeat_request;

    // tag: 112 - TestReqID
	if (get_fix_field(fix_msg, "112", &found_field) == 0)
    {
    	strcpy(response.text, found_field.field_val);
    }
	else
	{
		DBG_ERR("fail to get TestReqID.");
	}

	addData(response);
	pthread_cond_signal(m_threadCondition);
}


void EtReadCmeData::handle_globex_session_reject(fix_message_struct *fix_msg)
{
	fix_field_struct text_field;
	fix_field_struct last_client_seq_field;
    fix_field_struct account_field; 
    fix_field_struct tag50_field;

	int last_seq_num;
	cme_response_struct response;
    memset(&response, 0, sizeof(cme_response_struct));

	DBG_DEBUG("handle session reject.");
    // tag: 1 - account 
    if (get_fix_field(fix_msg, "1", &account_field) == -1){  
		DBG_ERR("can't find account field.");
		return;
    } 
 
    // tag 50 
    if (get_fix_field(fix_msg, "57", &tag50_field) == -1){   
		DBG_ERR("can't find tag 50 field.");
		return;   
    } 
 
    strcat(account_field.field_val, tag50_field.field_val); 
    strcpy(response.id, account_field.field_val);

	// 45 - rejected sequence 
	if (get_fix_field(fix_msg, "45", &last_client_seq_field) == 0)
	    {
          last_seq_num = atoi(last_client_seq_field.field_val);
	    }
	response.sent_seq_num = last_seq_num;

	// tag: 58 - Text
	if (get_fix_field(fix_msg, "58", &text_field) == -1)
	    {
			DBG_ERR("Fail to get field of text.");
	    }

	response.type = cme_session_reject;
	addData(response);
    pthread_cond_signal(m_threadCondition);

    audit_struct new_audit;
    memset(&new_audit, 0, sizeof(new_audit));
    
    build_audit_data(new_audit);
    new_audit.direction = TO_CLIENT;
    new_audit.status = REJECT;
    new_audit.customer = INDIVIDUAL_TRADING;
    new_audit.origin = 1;
    
    if(et_g_globex->sent_msg == LOGIN_SENT)
      {
        new_audit.msg_type = LOGIN;
      }
    else if(et_g_globex->sent_msg == ORDER_SENT)
      {
        new_audit.msg_type = NEW_ORDER;
      }
    strcpy(new_audit.reason, text_field.field_val);
    
    //should use security id for the second parameter
    build_audit_from_definition(new_audit, new_audit.security_des);
    et_g_globex->build_audit_time_local(new_audit); 
    et_g_auditTrail->add_audit(new_audit);
    pthread_cond_signal(m_auditCondition);
	return;
}


bool EtReadCmeData::handle_globex_resend_reqest(fix_message_struct *fix_msg)
{
	fix_field_struct start_seq_field;
	fix_field_struct end_seq_field;
	
	int seq_num = 0;
	
    // tag: 7 - BeginSeqNo
	if (get_fix_field(fix_msg, "7", &start_seq_field) == -1)
    {
		DBG_ERR("Fail to get field of BeginSeqNo.");
		return false;
    }

    seq_num = atoi(start_seq_field.field_val);

	// tag: 16 - EndSeqNo
	if (get_fix_field(fix_msg, "16", &end_seq_field) == -1)
    {
		DBG_ERR("Fail to get field of EndSeqNo.");
		return false;
    }
    
	cme_response_struct response;
    memset(&response, 0, sizeof(cme_response_struct));
    response.type = cme_resend_order;
    response.seq_num = seq_num;
    response.recv_num = recv_order_message_num;
    addData(response);
	pthread_cond_signal(m_threadCondition);
	
	return true;
}


void EtReadCmeData::handle_globex_business_reject_message(fix_message_struct *fix_msg)
{
	fix_field_struct text_field;
	fix_field_struct reason_field;
	fix_field_struct order_id_field;
    fix_field_struct last_client_seq_field;
    fix_field_struct account_field; 
    fix_field_struct tag50_field;

    order_struct our_order;
    char comp_id[0xf];

    cme_response_struct response; 
    memset(&response, 0, sizeof(cme_response_struct)); 
    response.type = cme_business_reject;

    // tag: 1 - account 
    if (get_fix_field(fix_msg, "1", &account_field) == -1){  
		DBG_ERR("can't find account field.");
		return;
    } 
 
    // tag 50 
    if (get_fix_field(fix_msg, "57", &tag50_field) == -1){   
		DBG_ERR("can't find tag 50 field.");
		return;
    }
 
    strcat(account_field.field_val, tag50_field.field_val); 
    strcpy(response.id, account_field.field_val);

	// add to audit trail
	audit_struct new_audit;
	memset(&new_audit, 0, sizeof(new_audit));

	build_audit_data(new_audit);
	new_audit.direction = TO_CLIENT;
	new_audit.status = REJECT;
    new_audit.customer = INDIVIDUAL_TRADING;
    new_audit.origin = 1;
    if(et_g_globex->sent_msg == LOGIN_SENT)
      {
        new_audit.msg_type = LOGIN;
      }
    else if(et_g_globex->sent_msg == ORDER_SENT)
      {
        new_audit.msg_type = NEW_ORDER;
      }
    
    
	// tag: 380 - BusinessRejectReason
	if (get_fix_field(fix_msg, "380", &reason_field) == -1)
    {
		DBG_ERR("Fail to get field of 380.");
    }
	else
    {
		DBG_ERR("Business reject reason code: %s", reason_field.field_val);
    }

    // tag: 58 - Text
	if (get_fix_field(fix_msg, "58", &text_field) == -1)
    {
		DBG_ERR("Fail to get field of text.");
    }
	else
    {
      strcpy(new_audit.reason, text_field.field_val);
    }
    response.reject_reason = atoi(reason_field.field_val);

    // 45 - rejected sequence  
    int last_seq_num;
    if (get_fix_field(fix_msg, "45", &last_client_seq_field) == 0) 
      { 
        last_seq_num = atoi(last_client_seq_field.field_val); 
      } 
    response.sent_seq_num = last_seq_num; 
    addData(response);
    
	pthread_cond_signal(m_threadCondition);

	// tag: 11 - ClOrdID                                                                                                                                                                                      
        if (get_fix_field(fix_msg, "11", &order_id_field) == -1)
	    {
			DBG_ERR("can't find ClOrdID field.");
			return;
	    }
        strcpy(new_audit.clordid, order_id_field.field_val);

        int iLen = strlen(order_id_field.field_val);

        if (iLen == 8)
	    {
          sprintf(comp_id, "%s", order_id_field.field_val);
	    }
        else if (iLen == 5)
	    {
          switch (order_id_field.field_val[0])
		    {
		    case 'O' :
			sprintf(comp_id, "ORD%s", order_id_field.field_val);
			break;
		    case 'C' :
			sprintf(comp_id, "CAN%s", order_id_field.field_val);
			break;
		    case 'A' :
			sprintf(comp_id, "ALT%s", order_id_field.field_val);
			break;
		    default :
				DBG_ERR("ClOrdID is error. %s", order_id_field.field_val);
			return;
		    }
	    }
        else
	    {
			DBG_ERR("ClOrdID field is error. %s", order_id_field.field_val);
			return;
	    }

        if (et_g_orderBook->get_order_by_client_order_id(comp_id, &our_order) == -1)
	    {
			DBG_ERR("can't find order, comp_id: %s, cli_ord_id: %s.", comp_id,
					order_id_field.field_val);
			return;
	    }

        if(our_order.giveup == true)
	    {
                new_audit.give_up = GU;
	    }

        strcpy(new_audit.orderid, our_order.ex_order_id);
        new_audit.quantity = our_order.total_size;
        strcpy(new_audit.security_des, our_order.display_name);

        // should use security id for the second parameter                                                                                                                                                    
        build_audit_from_definition(new_audit, new_audit.security_des);
        new_audit.order_type = our_order.execute_type;
        new_audit.order_qualifier = our_order.tif;
        et_g_globex->build_audit_time_local(new_audit);
        et_g_auditTrail->add_audit(new_audit);
        pthread_cond_signal(m_auditCondition);
		return;
}


bool EtReadCmeData::handle_globex_sequence_reqest(fix_message_struct *fix_msg)
{
	fix_field_struct new_seq_field;
	fix_field_struct gap_fill_field;
	
	int is_gap_fill = 0;

	if (get_fix_field(fix_msg, "36", &new_seq_field) == -1)
    {
		return false;
    }
	DBG_DEBUG("new_seq_field: %s", new_seq_field.field_val);
	
	if (get_fix_field(fix_msg, "123", &gap_fill_field) == -1)
    {
		DBG_ERR("fail to get 123 field.");
		is_gap_fill = 0;
    }
	else
    {
		if (strcmp(gap_fill_field.field_val, "Y") == 0)
	    {
			is_gap_fill = 1;
	    }
		else
	    {
			is_gap_fill = 0;
	    }
	}

	if (is_gap_fill == 1)
	{
		recv_order_message_num = atoi(new_seq_field.field_val);
	}
	
	return true;
}


bool EtReadCmeData::handle_globex_order_message(fix_message_struct *fix_msg)
{
	fix_field_struct order_id_field;
	fix_field_struct ex_order_id_field;
	fix_field_struct correlation_cli_id_field;
	fix_field_struct order_status_field;
	fix_field_struct inst_display_field;
	fix_field_struct text_field;
	fix_field_struct price_field;
	fix_field_struct size_field;
	fix_field_struct quant_left_field;
	fix_field_struct expire_field;
	fix_field_struct security_id_field;
	fix_field_struct order_side_field;
	fix_field_struct order_type_field;
	fix_field_struct qualifier_field;
	fix_field_struct symbol_field;
    fix_field_struct account_field;
    fix_field_struct tag50_field;

    order_struct our_order;

    int iLen = 0;
    int order_side = -1;
    char comp_id[0xf];

	cme_response_struct response;
	memset(&response, 0, sizeof(cme_response_struct));
	
	// add to audit trail
	audit_struct new_audit;
	memset(&new_audit, 0, sizeof(new_audit));

    build_audit_data(new_audit);
    new_audit.direction = FROM_CME;
    new_audit.customer = INDIVIDUAL_TRADING;
    new_audit.origin = 1;
	
	//DBG_DEBUG("handle globex order message.");

    // tag: 1 - account
    if (get_fix_field(fix_msg, "1", &account_field) == -1){ 
		DBG_ERR("can't find account field.");
		return false; 
    }
    strcpy(new_audit.account, account_field.field_val);

    // tag 50 -- cme response is 57
    if (get_fix_field(fix_msg, "57", &tag50_field) == -1){  
		DBG_ERR("can't find tag 57 field.");
		return false;  
    }
    strcpy(new_audit.senderid, tag50_field.field_val);

    strcat(response.id, account_field.field_val);
    strcat(response.id, tag50_field.field_val);
	DBG_DEBUG("id: %s", response.id);

	// tag: 48 - Security Id
	if (get_fix_field(fix_msg, "48", &security_id_field) == -1){
		DBG_ERR("can't find security_id field.");
		return false;
	}
    strcpy(new_audit.maturity_date, security_id_field.field_val);
    build_audit_from_definition(new_audit, security_id_field.field_val);

	// tag: 54 - side
	if (get_fix_field(fix_msg, "54", &order_side_field) == -1){
		DBG_ERR("can't find order_side.");
	}
	else{
	    order_side = atoi(order_side_field.field_val);
	    if(order_side == 1)
	    {
                new_audit.order_side = BID_SIDE;
	    }
	    else if(order_side == 2)
	    {
                new_audit.order_side = ASK_SIDE;
	    }
	}

	// tag 55 symbol
	if (get_fix_field(fix_msg, "55", &symbol_field) == -1){
		DBG_ERR("can't find order_side.");
	}
	else{

	}

	// tag: 39 - OrdStatus
	if (get_fix_field(fix_msg, "39", &order_status_field) == -1){
		DBG_ERR("can't find order status.");
      //return false;
	}
	else{

	}

	// tag: 40 - Order type
	if (get_fix_field(fix_msg, "40", &order_type_field) == -1){
      //DBG_ERR("can't find order_type.");
	    //return false;
	}
	else{
	    new_audit.order_type = order_type_field.field_val[0];
	    //DBG_DEBUG("order_type: %s", order_type_field.field_val);
	}

	// tag 11 ClOrdID
	if (get_fix_field(fix_msg, "11", &order_id_field) == -1){
      //DBG_ERR("can't find ClOrdID field.");
	    //return false;
	}
	else{
	    strcpy(new_audit.clordid, order_id_field.field_val);
	    //DBG_DEBUG("ClOrdID: %s", order_id_field.field_val);
	}

	iLen = strlen(order_id_field.field_val);
	if (iLen == 8){
	    sprintf(comp_id, "%s", order_id_field.field_val);
	}
	else if (iLen == 5){
	    switch (order_id_field.field_val[0]){
	    case 'O' :
		sprintf(comp_id, "ORD%s", order_id_field.field_val);
		break;
	    case 'C' :
		sprintf(comp_id, "CAN%s", order_id_field.field_val);
		break;
	    case 'A' :
		sprintf(comp_id, "ALT%s", order_id_field.field_val);
		break;
	    default :
          //		DBG_ERR("ClOrdID is error. %s", order_id_field.field_val);
		return false;
	    }
	}
	else{
      //DBG_ERR("ClOrdID field is error. %s", order_id_field.field_val);
	    //return false;
	}
	
	strcpy(response.cli_order_id, comp_id);
	//DBG_DEBUG("comp_id: %s", comp_id);
    if (et_g_orderBook->get_order_by_client_order_id(comp_id, &our_order) == -1){
      //DBG_ERR("can't find order, comp_id: %s, cli_ord_id: %s.", comp_id, order_id_field.field_val);
      //return false;
    }
    
    if(our_order.giveup == true){
      new_audit.give_up = GU;
	}

	// tag: 37 - OrderID
	if (get_fix_field(fix_msg, "37", &ex_order_id_field) == -1){
      //DBG_ERR("can't find exchange order id.");
	    //return false;
	}
    else{
      strcpy(response.ex_order_id, ex_order_id_field.field_val);
      strcpy(new_audit.orderid, ex_order_id_field.field_val);
      //DBG_DEBUG("OrderID: %s", ex_order_id_field.field_val);
	}

	// tag: 107 - SecurityDesc
	if (get_fix_field(fix_msg, "107", &inst_display_field) == -1){
      //DBG_ERR("can't find SecurityDesc field.");
      //return false;
	}
	else{
      strcpy(new_audit.security_des, inst_display_field.field_val);
      strcpy(response.symbol, inst_display_field.field_val);
      //DBG_DEBUG("DisplayName: %s", inst_display_field.field_val);

      if(strlen(inst_display_field.field_val) > 4){
        // tag: 59 - order qualifier                                                                                                                                                                  
        if (get_fix_field(fix_msg, "59", &qualifier_field) == -1){
          //DBG_ERR("can't find order qualifier.");
          // return false;                                                                                                                                                                          
        }
        else{
          new_audit.order_qualifier = (tif_type)(atoi(qualifier_field.field_val));
          //DBG_DEBUG("order qualifier: %s", qualifier_field.field_val);
        }

        // tag: 432 - Expire date                                                                                                                                                                     
        if (get_fix_field(fix_msg, "432", &expire_field) == -1){
          //DBG_ERR("can't find Expire date.");
          //return false;                                                                                                                                                                           
        }
        else{
          strcpy(new_audit.maturity_date, expire_field.field_val);
          //DBG_DEBUG("Expire date: %s", expire_field.field_val);

          if (et_g_orderBook->set_expire_date(our_order.global_order_id, expire_field.field_val) == -1){
            //DBG_DEBUG("fail to set expire date.");
          }
        }
      }

    }


	// tag: 9717 - CorrelationClOrdID
	if (get_fix_field(fix_msg, "9717", &correlation_cli_id_field) == -1){
      //DBG_ERR("can't find CorrelationClOrdID field.");
      //return false;
    }
    else{
      sprintf(new_audit.correlation_clordid, "ORD%s", correlation_cli_id_field.field_val );
      //DBG_DEBUG("CorrelationClOrdID: %s", correlation_cli_id_field.field_val);
    }

	// ------------------------
	// order rejected
	// OrdStatus = 8, rejected
	// ------------------------
	if (strcmp(order_status_field.field_val, "8") == 0)
	    {
		new_audit.status = REJECT;
		new_audit.msg_type = NEW_ORDER;

		// tag: 58 - Text
		if (get_fix_field(fix_msg, "58", &text_field) == -1)
		    {
              //DBG_ERR("Fail to get rejected text, order_id: %s, name: %s", ex_order_id_field.field_val, inst_display_field.field_val);
		    }
		else
		    {
              strcpy(new_audit.reason, text_field.field_val);
              //DBG_DEBUG("Success to get rejected text, order_id: %s, name: %s, text: %s",
              //ex_order_id_field.field_val, inst_display_field.field_val, text_field.field_val);
		    }
	    
		response.type = cme_order_reject;
		addData(response);
		pthread_cond_signal(m_threadCondition);

        et_g_globex->build_audit_time_local(new_audit);
        et_g_auditTrail->add_audit(new_audit);
        pthread_cond_signal(m_auditCondition);

		if (order_side == 1)
		    {
              //DBG_ERR("%s bid order rejected : %s", inst_display_field.field_val, text_field.field_val);
		    }
		else if(order_side == 2)
		    {
              //DBG_ERR("%s ask order rejected : %s", inst_display_field.field_val, text_field.field_val);
		    }
	    
		return true;
	}

	// ---------------------
	// order accepted
	// OrdStatus = 0, New
	// ---------------------
	else if (strcmp(order_status_field.field_val, "0") == 0){
      new_audit.status = OK;
      new_audit.msg_type = NEW_ORDER;
      
      strcpy(response.ex_order_id, ex_order_id_field.field_val);
      
      response.type = cme_order_accepted;
      addData(response);
      pthread_cond_signal(m_threadCondition);
      
      // tag 151 leave quantity
      if (get_fix_field(fix_msg, "151", &quant_left_field) == -1){
        //DBG_ERR("Fail to get LeavesQty field.");
      }
      // DBG_DEBUG("LeavesQty: %s", quant_left_field.field_val);
      
      // tag 44 price
      if (get_fix_field(fix_msg, "44", &price_field) == -1){
        //DBG_ERR("Fail to get LeavesQty field.");
      }

      //DBG_DEBUG("New order, ex_order_id: %s, name: %s, price: %s",  
      //        ex_order_id_field.field_val, inst_display_field.field_val, price_field.field_val);

      if( (atoi(order_type_field.field_val) != 1 && strcmp(order_type_field.field_val, "K") != 0 ) ){
        new_audit.limit_price = atoi(price_field.field_val);
      }
      
      new_audit.quantity = atoi(quant_left_field.field_val);
      et_g_globex->build_audit_time_local(new_audit);
      et_g_auditTrail->add_audit(new_audit);
      pthread_cond_signal(m_auditCondition);
      return true;
    }


	// -------------------------------------
	// order filled
	// OrderStatus = 1, Partially filled
	// OrderStatus = 2, Filled
	// -------------------------------------
	else if ( (strcmp(order_status_field.field_val, "1") == 0) || 
		      (strcmp(order_status_field.field_val, "2") == 0) ){
	    new_audit.status = OK;
	    new_audit.msg_type = EXECUTION;

	    // tag: 31 - LastPx
	    if (get_fix_field(fix_msg, "31", &price_field) == -1){
          //DBG_ERR("Fail to get LastPx field, ord_status = %s, cli_ord_id = %s", order_status_field.field_val, order_id_field.field_val);
          return false;
	    }
	    new_audit.fill_price = atof(price_field.field_val);
	    double price_filled = atof(price_field.field_val);
        int iPrice = atoi(price_field.field_val);

	    //if it is not a market order
	    if( (atoi(order_type_field.field_val) != 1 && strcmp(order_type_field.field_val, "K") != 0 )
            && (strcmp(order_status_field.field_val, "1") == 0))
          {
		    new_audit.limit_price = new_audit.fill_price;
          }
	    
		// tag: 32 - LastShares
	    if (get_fix_field(fix_msg, "32", &size_field) == -1)
          {
			//DBG_ERR("Fail to get LastShares field, ord_status = %s, cli_ord_id = %s", order_status_field.field_val, order_id_field.field_val);
			//return false;
          }

	    new_audit.quantity = atoi(size_field.field_val);
	    int size_filled = atoi(size_field.field_val);
	    //DBG_DEBUG("LastShares:%d", atoi(size_field.field_val));
	    //DBG_DEBUG("order filled, ex_order_id: %s, name: %s, price: %f, size: %d", 
        //    ex_order_id_field.field_val, inst_display_field.field_val, price_filled, size_filled);

        // ask  
        int cal_size = size_filled;
        if(strcmp(order_side_field.field_val,"2") == 0)  
          {
            cal_size = 0 - size_filled;  
          }

        // only deal with spread fill, not legs
        if(strlen(inst_display_field.field_val) > 4){
          response.type = cme_order_filled_spread;
          response.price = price_filled;
          response.size = size_filled;
          addData(response);
          pthread_cond_signal(m_threadCondition);

          et_g_posiFill->send_fill_spread(inst_display_field.field_val, account_field.field_val, iPrice, cal_size);
        }

        // if outright, then prepare to send to positionSrv, spread send only legs
        if( strlen(inst_display_field.field_val) < 5 ) 
          { 
            cme_response_struct response_leg; 
            memset(&response_leg, 0, sizeof(cme_response_struct));
            strcat(response_leg.id, account_field.field_val); 
            strcat(response_leg.id, tag50_field.field_val);
            response_leg.type = cme_order_filled_leg; 
            response_leg.size = cal_size;
            strcpy(response_leg.symbol, inst_display_field.field_val);
            addData(response_leg); 
            pthread_cond_signal(m_threadCondition);

            et_g_posiFill->send_fill_outright(inst_display_field.field_val, account_field.field_val, iPrice, cal_size);
          } 

        et_g_globex->build_audit_time_local(new_audit);
	    et_g_auditTrail->add_audit(new_audit);
        pthread_cond_signal(m_auditCondition);
	    return true;
	}


	// ---------------------------
	// order cancelled
	// OrderStatus = 4, Canceled
	// OrderStatus = C, Expired
	// ---------------------------
	else if	( (strcmp(order_status_field.field_val, "4") == 0) ||
		      (strcmp(order_status_field.field_val, "C") == 0) )
	{
		new_audit.status = OK;
		new_audit.msg_type = CANCEL;

		// tag: 151 - LeavesQty
		//if (get_fix_field(fix_msg, "151", &quant_left_field) == -1)
	    //{
	    //	DBG_ERR("Fail to get LeavesQty field.");
	    //}
		//DBG_DEBUG("LeavesQty: %s", quant_left_field.field_val);

		// tag: 38 - OrderQty
		//if (get_fix_field(fix_msg, "38", &order_quant_field) == -1)
	    //{
	    //	DBG_ERR("Fail to get OrderQty field.");
		//	return (-1);
	    //}
	    //DBG_DEBUG("OrderQty: %s", order_quant_field.field_val);
	    
	    //DBG_DEBUG("order cancelled, ex_order_id: %s, name: %s", ex_order_id_field.field_val, inst_display_field.field_val);
		
		response.type = cme_order_cancelled;
		addData(response);
		pthread_cond_signal(m_threadCondition);
		
		//if (-1 == et_g_orderBook->remove_order(our_order.order_id))
		//{
		//	DBG_ERR("fail to remove an order.");
		//	return -1;
		//}

        et_g_globex->build_audit_time_local(new_audit);
        et_g_auditTrail->add_audit(new_audit);
        pthread_cond_signal(m_auditCondition);
		return true;
	}
	

	// --------------------------
	// order altered
    // OrderStatus = 5, Replaced
    // --------------------------
	else if (strcmp(order_status_field.field_val, "5") == 0)
    {   
		new_audit.status = OK;
		new_audit.msg_type = MODIFY;
        strcpy(response.ex_order_id, ex_order_id_field.field_val);

	    // tag: 44 - Price
		if (get_fix_field(fix_msg, "44", &price_field) == -1)
	    {
          //DBG_ERR("Fail to get Price field, ord_status = %s, cli_ord_id = %s", order_status_field.field_val, order_id_field.field_val);
			return false;
	    }
		new_audit.limit_price = atoll(price_field.field_val);
	    //DBG_DEBUG("Price: %s", price_field.field_val);
		int alter_price = atoi(price_field.field_val);

		// tag: 151 - LeavesQty
		int alter_size = 0;
		if (get_fix_field(fix_msg, "151", &quant_left_field) == -1)
	    {
          //DBG_ERR("fail to get LeavesQty field.");
			alter_size = 0;
	    }
		else
	    {
			new_audit.quantity = atoi(quant_left_field.field_val);
	    	//DBG_DEBUG("LeavesQty: %s", quant_left_field.field_val);
			alter_size = atoi(quant_left_field.field_val);
	    }

		//DBG_DEBUG("order altered, ex_order_id: %s, name: %s", ex_order_id_field.field_val, inst_display_field.field_val);
		
		response.type = cme_order_altered;
		response.price = alter_price;
		response.size = alter_size;
		addData(response);
		pthread_cond_signal(m_threadCondition);

        et_g_globex->build_audit_time_local(new_audit);
        et_g_auditTrail->add_audit(new_audit);
        pthread_cond_signal(m_auditCondition);
		return true;
	}

	// -----------------------------------------
	// order unfilled (trade cancelled)
	// OrderStatus = H, Not find in fix 4.2
	// -----------------------------------------
	else if (strcmp(order_status_field.field_val, "H") == 0)
    {
		new_audit.status = OK;
		new_audit.msg_type = CANCEL;

    	// tag: 31 - LastPx
		//if (get_fix_field(fix_msg, "31", &price_field) == -1)
	    //{
		//	DBG_ERR("Fail to find LastPx field, cli_ord_id: %s.", order_id_field.field_val);
		//	return (-1);
	    //}

		// tag: 31 - LastShares
		//if (get_fix_field(fix_msg, "32", &size_field) == -1)
		//{
		//	DBG_ERR("Fail to find LastShares field, cli_ord_id: %s", order_id_field.field_val);
		//	return (-1);
		//}

		//if (-1 == et_g_orderBook->remove_order(our_order.order_id))
		//{
		//	DBG_ERR("fail to remove an order.");
		//	return -1;
		//}

		//DBG_DEBUG("trade cancelled, ex_order_id: %s, name: %s", ex_order_id_field.field_val, inst_display_field.field_val);
		
		response.type = cme_order_cancelled;
		addData(response);
		pthread_cond_signal(m_threadCondition);

        et_g_globex->build_audit_time_local(new_audit);
        et_g_auditTrail->add_audit(new_audit);
        pthread_cond_signal(m_auditCondition);
		return true;
    }
    
    //DBG_ERR("order status is not handle: %s", order_status_field.field_val);
    et_g_globex->build_audit_time_local(new_audit);
    et_g_auditTrail->add_audit(new_audit);
    pthread_cond_signal(m_auditCondition);
	return false;
}


bool EtReadCmeData::handle_globex_cancel_reject_message(fix_message_struct *fix_msg)
{
	fix_field_struct order_id_field;
	fix_field_struct ex_order_id_field;
	fix_field_struct order_status_field;
	fix_field_struct text_field;
	fix_field_struct correlation_cli_id_field;
    fix_field_struct account_field; 
    fix_field_struct tag50_field;

	order_struct our_order;	
	char comp_id[0xf];
	cme_response_struct response;
	memset(&response, 0, sizeof(cme_response_struct));
	
	//DBG_DEBUG("Cancel/Modify order was rejected.");
    audit_struct new_audit;
    memset(&new_audit, 0, sizeof(new_audit));
    
    build_audit_data(new_audit);
    new_audit.direction = TO_CLIENT;
    new_audit.status = REJECT;
    new_audit.customer = INDIVIDUAL_TRADING;
    new_audit.origin = 1;
    
    if(et_g_globex->sent_msg == CANCEL_SENT)
      {
        new_audit.msg_type = CANCEL;
      }
    else if(et_g_globex->sent_msg == ALTER_SENT)
      {
        new_audit.msg_type = MODIFY;
      }
	
    // tag: 1 - account 
    if (get_fix_field(fix_msg, "1", &account_field) == -1){  
      //DBG_ERR("can't find account field.");  
      return false;  
    } 
 
    // tag 50 
    if (get_fix_field(fix_msg, "57", &tag50_field) == -1){   
      //DBG_ERR("can't find tag 50 field.");   
      return false;   
    } 
 
    strcat(account_field.field_val, tag50_field.field_val); 
    strcpy(response.id, account_field.field_val);

	// tag: 39, OrdStatus
	if (get_fix_field(fix_msg, "39", &order_status_field) == -1)
    {
      //		DBG_ERR("Fail to get order status.");
		return false;
    }
    //DBG_DEBUG("OrdStatus: %s", order_status_field.field_val);

	// tag: 11, ClOrdID
	if (get_fix_field(fix_msg, "11", &order_id_field) == -1)
    {
      //DBG_ERR("Fail to get ClOrdID field.");
		return false;
    }
	strcpy(new_audit.clordid, order_id_field.field_val);
    //DBG_DEBUG("ClOrdID: %s", order_id_field.field_val);
    
    int iLen = strlen(order_id_field.field_val);
	if (iLen == 8)
    {
		sprintf(comp_id, "%s", order_id_field.field_val);
    }
	else if (iLen == 5)
    {
		switch (order_id_field.field_val[0])
	    {
			case 'O' :
				sprintf(comp_id, "ORD%s", order_id_field.field_val);
				break;
			case 'C' :
				sprintf(comp_id, "CAN%s", order_id_field.field_val);
				break;
			case 'A' :
				sprintf(comp_id, "ALT%s", order_id_field.field_val);
				break;
			default :
              //				DBG_ERR("ClOrdID is error. ClOrdID: %s", order_id_field.field_val);
				return false;
		}
	}
	else
    {
      //DBG_ERR("ClOrdID is error. ClOrdID: %s", order_id_field.field_val);
		return false;
    }
    
    strcpy(response.cli_order_id, comp_id);

    if (et_g_orderBook->get_order_by_client_order_id(comp_id, &our_order) == -1)
	{
      //DBG_ERR("can't find order, comp_id: %s, cli_ord_id: %s.", comp_id, order_id_field.field_val);
	    return (-1);
	}

    if(our_order.giveup == true)
        {
	    new_audit.give_up = GU;
        }

	
	// tag: 37, OrderID
	if (get_fix_field(fix_msg, "37", &ex_order_id_field) == -1)
    {
      //DBG_ERR("Fail to get exchange order id, ord_status=%s ord_id=%s.", order_status_field.field_val, order_id_field.field_val);
		return false;
    }
	strcpy(new_audit.orderid, ex_order_id_field.field_val);
    //DBG_DEBUG("OrderID: %s", ex_order_id_field.field_val);

	// tag: 58, Text
	if (get_fix_field(fix_msg, "58", &text_field) == -1)
    {
      //DBG_ERR("cancel reject, comp_id: %s, denied: (%s)", comp_id, "reason not provided");
    }
	else
    {
      strcpy(new_audit.reason, text_field.field_val);
      //DBG_DEBUG("cancel reject, comp_id: %s, denied: (%s)", comp_id, text_field.field_val);
    }

    if (get_fix_field(fix_msg, "9717", &correlation_cli_id_field) == -1)
      {
        //DBG_ERR("can't find CorrelationClOrdID field.");
        return (-1);
      }
    sprintf(new_audit.correlation_clordid, "ORD%s", correlation_cli_id_field.field_val);
    //DBG_DEBUG("CorrelationClOrdID: %s", correlation_cli_id_field.field_val);

    response.type = cme_cancelalter_reject;
	addData(response);
	pthread_cond_signal(m_threadCondition);
	
    new_audit.quantity = our_order.total_size;
    strcpy(new_audit.security_des, our_order.display_name);

	//should use security id for the second parameter
    build_audit_from_definition(new_audit, new_audit.security_des);
    new_audit.order_type = our_order.execute_type;
    new_audit.order_qualifier = our_order.tif;
    
    et_g_globex->build_audit_time_local(new_audit);
    et_g_auditTrail->add_audit(new_audit);
    pthread_cond_signal(m_auditCondition);
	return true;
}


/**
 * 2009/12/09: currently this is useless.
 */
int EtReadCmeData::handle_globex_quote_acknowledge(fix_message_struct *fix_msg)
{
	fix_field_struct quote_req_id;
	fix_field_struct quote_ack_status;
	fix_field_struct ex_quote_req_id;
	fix_field_struct quote_id;
	
	//order_struct our_order;

    // tag: 131 - QuoteReqID
	if (get_fix_field(fix_msg, "131", &quote_req_id) == -1)
    {
      //DBG_ERR("Fail to get field of QuoteReqID.");
		return -1;
    }
	//DBG_DEBUG("QuoteReqID: %s", quote_req_id.field_val);
		
	//if (et_g_orderBook->get_order_by_mul_id(quote_req_id.field_val, &our_order) == -1)
    //{
	//	DBG_ERR("can't find quote, comp_id: %s", quote_req_id.field_val);
	//	return (-1);
    //}
    
    // tag: 297 - QuoteAckStatus
	if (get_fix_field(fix_msg, "297", &quote_ack_status) == -1)
    {
      //DBG_ERR("Fail to get field of QuoteAckStatus.");
		return -1;
    }
	//DBG_DEBUG("QuoteAckStatus: %s", quote_req_id.field_val);
    
    // tag: 9770 - ExchangeQuoteReqID
	if (get_fix_field(fix_msg, "9770", &ex_quote_req_id) == -1)
    {
      //DBG_ERR("Fail to get field of ExchangeQuoteReqID.");
		return -1;
    }
	//DBG_DEBUG("ExchangeQuoteReqID: %s", ex_quote_req_id.field_val);
	
	// tag: 117 - QuoteID
	if (get_fix_field(fix_msg, "117", &quote_id) == -1)
    {
      //DBG_ERR("Fail to get field of QuoteID.");
		return -1;
    }
	//DBG_DEBUG("QuoteID: %s", quote_id.field_val);
	
	return 0;
}

bool EtReadCmeData::build_audit_data(audit_struct& new_audit)
{
    new_audit.audit_num = et_g_auditTrail->globex_audit_id++;
    new_audit.exchange_code = XNYM;
    //strcpy(new_audit.account, GLOBEX_ACCT);
    //strcpy(new_audit.senderid, ORDER_USER_NAME);
    strcpy(new_audit.firm_num, FIRM_ID);
    strcpy(new_audit.session_id, SESSION_ID);

    return true;
}

bool EtReadCmeData::build_audit_from_definition(audit_struct& new_audit, string security_id)
{
    // need to read security definition to fill in
  strcpy(new_audit.inst_code, et_g_globex->inst_code.c_str());
  strcpy(new_audit.cfi_code, "");
  strcpy(new_audit.maturity_date, "");

  return true;
}
