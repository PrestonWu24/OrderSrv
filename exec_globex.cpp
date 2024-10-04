
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <unistd.h>
#include <assert.h>
#include <fstream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/tcp.h>

#include "trace_log.h"
#include "util.h"
#include "read_config.h"
#include "globex_common.h"
#include "order_book.h"
#include "exec_globex.h"
#include "audit_trail.h"
#include "read_cme_data.h"

// ---- globe variable ----
extern order_book* et_g_orderBook;
extern audit_trail* et_g_auditTrail;
extern EtReadCmeData* et_g_readCme;
// ---- globe variable end ----


int exec_globex::global_order_num = 0;


exec_globex::exec_globex()
{
	order_des = -1;
	sent_seq_num = -1;;
	login_state = NOT_LOGGED_IN;
    //m_send_msg_num = 0;

	globex_seq_order_id = 1;
	last_request_seq_num = -1;
	globex_audit_id = 1;
    inst_code = "CL";

	// if begin of week, need set sequence to 1
	seqfile.open("database/sequence.txt", fstream::in);
	assert(seqfile.good());
	char buf[64];
	seqfile.getline(buf, sizeof(buf));
	int diff = EtUtil::diff_time(buf);

	seqfile.getline(buf, sizeof(buf));
	int last_weekday = atoi(buf);

	seqfile.getline(buf, sizeof(buf));
	order_message_num = atoi(buf);

	char date_buffer[2];
	memset(date_buffer, 0, sizeof(date_buffer));
	EtUtil::build_weekday(date_buffer, sizeof(date_buffer));
	int curr_weekday = atoi(date_buffer);

	if(last_weekday > 1 && (curr_weekday < 2 || (curr_weekday >= 2 && diff > curr_weekday ) ))
	    {
          // begin of the week
          order_message_num = 1;
	    }

	seqfile.close();
}


exec_globex::~exec_globex()
{
	// write file for last saved sequence number
	seqfile.open("database/sequence.txt", fstream::out);

	char date_buffer[9];
	memset(date_buffer, 0, sizeof(date_buffer));
	EtUtil::build_date(date_buffer, sizeof(date_buffer));

	char weekday_buffer[2];
	memset(weekday_buffer, 0, sizeof(weekday_buffer));
	EtUtil::build_weekday(weekday_buffer, sizeof(weekday_buffer));

	seqfile << date_buffer << "\n";
	seqfile << weekday_buffer << "\n";
	seqfile << order_message_num << "\n";
    seqfile << et_g_readCme->recv_order_message_num << "\n";
	seqfile.close();

        if (order_des != -1)
	    {
                close(order_des);
	    }
}

void exec_globex::setAuditCondition(pthread_cond_t* _cond) 
{ 
  m_auditCondition = _cond; 
}

bool exec_globex::loginOrderServer()
{
	if ( !send_login() )
	{
		DBG_ERR("fail to send login command.");
		return false;
	}
	return true;
}


int exec_globex::connectOrderServer()
{
	int iRes = 0;
	
	// connect
	iRes = EtUtil::connect_client(EtReadConfig::m_orderSrvIp.c_str(), 
		EtReadConfig::m_orderSrvPort);
	if (iRes == -1)
	{
		DBG_ERR("Fail to connect to host: %s port: %d", 
				EtReadConfig::m_orderSrvIp.c_str(), EtReadConfig::m_orderSrvPort);
		return -1;
    }
	else
	{
		DBG_DEBUG("connect to ORDER SERVER: host: %s port: %d", 
				  EtReadConfig::m_orderSrvIp.c_str(), EtReadConfig::m_orderSrvPort);
	}

	order_des = iRes;
	return order_des;
}


/**
 * add a new order.
 * return: int
 *    -1: error
 *    >0: the local number.
 */
bool exec_globex::place_order(string _account, string _tag50, int _local_num, char* display_name, int side, int dec_price, 
                              int size, int max_show, tif_type tif, char execute_type)
{
	if (login_state != LOGIN_SUCCESS)
	{
		DBG_ERR("login_state is error, %d", login_state);
		return (-1);
	}

	char id_buffer[MAX_STRING_ORDER_ID];
	memset(id_buffer, 0, MAX_STRING_ORDER_ID);
	
	int sent_seq_num;
    if( !send_order(_account, _tag50, display_name, side, dec_price,  size, max_show,
                    tif, execute_type, id_buffer, sent_seq_num) )
	{
		DBG_ERR("fail placing order, name: %s, side: %d, price: %f, size: %d",
				display_name, side, dec_price, size);
		return false;
	}

	global_order_num++;
	
	DBG_DEBUG("success placing_order: %d, %s, %s, %d, %f, size: %d, max_show: %d", 
			  global_order_num, id_buffer, display_name, side, dec_price, size, max_show);
	
	int iPrice = EtUtil::doubleToInt(dec_price, 1);
	if ( !et_g_orderBook->add_order(global_order_num, _local_num, display_name, side, iPrice, size, sent_seq_num) )
	{
		DBG_ERR("can't add order to order list, name: %s, price: %f, size: %d", 
				display_name, dec_price, size);
		return false;
	}

	if ( !et_g_orderBook->set_correlation_cli_order_id(global_order_num, id_buffer) )
	{
		DBG_ERR("fail to set correlation_cli_order_id.");
		return false;
	}
	
	if ( !et_g_orderBook->set_orig_cli_order_id(global_order_num, id_buffer) )
	{
		DBG_ERR("fail to set orig_cli_order_id.");
		return false;
    }
	
	return true;
}


bool exec_globex::alter_order(string _account, string _tag50, int _global_num, double _price, int _size, int max_show)
{
	order_struct to_alter;
	memset(&to_alter, 0, sizeof(order_struct));
	
	if ( !et_g_orderBook->get_order(_global_num, &to_alter) )
	{
		DBG_ERR("fail to find the order: %d", _global_num);
		return false;
	}

	if (_size <= 0)
    {
		DBG_ERR("size = %d is error.", _size);
		return false;
    }

	if (login_state != LOGIN_SUCCESS) 
	{
		DBG_ERR("not login to server.");
		return false;
	}

	if (to_alter.stage != ORDER_IN_MARKET)
        {
			DBG_ERR("order current stage is not able to send alter order: %d", to_alter.stage);
			return false;
		}
	
	char new_id[MAX_STRING_ORDER_ID];
	memset(new_id, 0, MAX_STRING_ORDER_ID);
    
    bool bRes = send_alter(_account, _tag50, to_alter.ex_order_id, to_alter.orig_cli_order_id, 
    	to_alter.correlation_cli_order_id, to_alter.side, _price, to_alter.display_name, 
			   _size, max_show, new_id, to_alter.stop_price, to_alter.tif, to_alter.execute_type, to_alter.giveup);
	if ( !bRes )
    {
		DBG_ERR("Fail to alter order, name = %s, order_id = %d", to_alter.display_name, _global_num);
		return false;
    }
    
   	/* update new id */
    if ( !et_g_orderBook->set_orig_cli_order_id(to_alter.global_order_id, new_id) )
	{
		DBG_ERR("Fail to set orig_cli_order_id.");
		return false;
	}
	    
	if ( !et_g_orderBook->set_order_state(to_alter.global_order_id, ORDER_SENT_ALTER) )
    {
		DBG_ERR("fail to set order state. order_id = %d", to_alter.global_order_id);
		return false;
    }
    
	return true;
}


bool exec_globex::cancel_order(string _account, string _tag50, int _global_num, char* _reason)
{
	char new_id[MAX_STRING_ORDER_ID];
	order_struct to_cancel;
	
	if ( !et_g_orderBook->get_order(_global_num, &to_cancel) )
	{
		DBG_ERR("fail to find the order: %d", _global_num);
		strcpy(_reason, "currently is NOT LOGIN!");
		return false;
	}

	DBG_DEBUG("cancel_order, stage=%d", to_cancel.stage);

	if (login_state != LOGIN_SUCCESS)
    {
		DBG_ERR("login_state is error.");
		return false;
    }

	if (to_cancel.stage != ORDER_IN_MARKET)
    {
		DBG_ERR("current stage cannot cancel. stage: %d", to_cancel.stage);
		sprintf(_reason, "current stage cannot cancel: %s", (order_stage[int(to_cancel.stage)]).c_str());
		return false;
    }

	memset(new_id, 0, MAX_STRING_ORDER_ID);
	
	if( !send_cancel(_account, _tag50, to_cancel.side, to_cancel.ex_order_id, to_cancel.orig_cli_order_id,
			   to_cancel.correlation_cli_order_id, to_cancel.display_name, new_id, to_cancel.correlation_cli_order_id,
			 to_cancel.total_size, to_cancel.expire_date, to_cancel.tif, to_cancel.execute_type) )
    {
		DBG_ERR("fail to cancel ex_order_id: %s", to_cancel.ex_order_id);
		strcpy(_reason, "send calcel order fail.");
		return false;
    }
    
    // update new id
	if ( !et_g_orderBook->set_orig_cli_order_id(_global_num, new_id) )
	{
		DBG_ERR("fail to set orig_cli_order_id.");
		return false;
	}
    
	if (-1 == et_g_orderBook->set_order_state(_global_num, ORDER_SENT_CANCEL))
	{
		DBG_ERR("fail to set order state.");
		return false;
    }

	return true;
}

bool exec_globex::build_audit_data(audit_struct& new_audit)
{
    new_audit.audit_num = globex_audit_id++;
    new_audit.exchange_code = XNYM;
    strcpy(new_audit.firm_num, FIRM_ID);
    strcpy(new_audit.session_id, SESSION_ID);

    return true;
}

bool exec_globex::build_audit_time_local(audit_struct& new_audit)
{
    char curr_date[12];
    char curr_time[16];
    memset(curr_date, 0, sizeof(curr_date));
    memset(curr_time, 0, sizeof(curr_time));
    char* curr_date_ptr = curr_date;
    char* curr_time_ptr = curr_time;

    EtUtil::getDate(curr_date_ptr, sizeof(curr_date));
    EtUtil::getTime(curr_time_ptr, sizeof(curr_time));

    strcpy(new_audit.date, curr_date);
    strcpy(new_audit.time_stamp, curr_time);
    
    return true;
}

bool exec_globex::build_audit_from_definition(audit_struct& new_audit, string security_id)
{
    // need to read security definition to fill in
    strcpy(new_audit.cfi_code, "");
    strcpy(new_audit.inst_code, "");  
    strcpy(new_audit.maturity_date, "");

    return true;
}
