
#include <iostream>
#include <errno.h>

#include "trace_log.h"
#include "util.h"
#include "globex_common.h"
#include "exec_globex.h"
#include "order_book.h"
#include "package_store.h"
#include "audit_trail.h"
#include "read_config.h"

using namespace std;

extern string g_product_name;
extern audit_trail* et_g_auditTrail;

int exec_globex::build_field_int(char* _buffer, const int _tag, const int _iVal)
{
	int size = 0;
	char* ptr = _buffer;
	
	size = sprintf(ptr, "%d=%d", _tag, _iVal);
	ptr += size;

	*ptr = SOH;
	return size + 1;
}


int exec_globex::build_field_string(char* _buffer, const int _tag, const char* _strVal)
{
	int size = 0;
	char* ptr = _buffer;
	
	size = sprintf(ptr, "%d=%s", _tag, _strVal);
	ptr += size;

	*ptr = SOH;
	return size + 1;
}


int exec_globex::build_field_char(char* _buffer, const int _tag, const char _cVal)
{
	int size = 0;
	char* ptr = _buffer;
	
	size = sprintf(ptr, "%d=%c", _tag, _cVal);
	ptr += size;

	*ptr = SOH;
	return size + 1;
}


int exec_globex::build_field_double(char* _buffer, const int _tag, const double _dVal)
{
	int size = 0;
	char* ptr = _buffer;
	
	size = sprintf(ptr, "%d=%f", _tag, _dVal);
	ptr += size;

	*ptr = SOH;
	return size + 1;
}

// login to fix server
bool exec_globex::send_login()
{
	int size = 0;
	
	char body_buffer[ORDER_BUFFER_SIZE];
	memset(body_buffer, 0, ORDER_BUFFER_SIZE);
	char* body_ptr = body_buffer;
	
	// password len
	size = build_field_int(body_ptr, 95, (int)strlen(ORDER_PASSWORD));
	body_ptr += size;

    // account
    size = build_field_string(body_ptr, 1, GLOBEX_ACCT); 
    body_ptr += size;

    // SenderSubID
    size = build_field_string(body_ptr, 50, GLOBEX_ACCT);
    body_ptr += size;

	// password
	size = build_field_string(body_ptr, 96, ORDER_PASSWORD);
	body_ptr += size;

	// encrypt type
	size = build_field_int(body_ptr, 98, 0);
	body_ptr += size;

	// heart beach method
	size = build_field_string(body_ptr, 108, GLOBEX_HEART_BEAT_INTERVAL);
	body_ptr += size;

    char write_buffer[ORDER_BUFFER_SIZE];
    char* write_ptr = write_buffer;
    memset(write_buffer, 0, ORDER_BUFFER_SIZE);
    
    // header
    make_fix_header(write_buffer, &write_ptr, body_ptr - body_buffer, "A", 0, 0, sent_seq_num);
	
	// copy body
	memcpy(write_ptr, body_buffer, body_ptr - body_buffer);
	write_ptr += (body_ptr - body_buffer);
	
	// trailer
	make_fix_trailer(write_buffer, &write_ptr);

	size = write_ptr - write_buffer;
	
	DBG_DEBUG("login_str: %s", write_buffer);
	
	if (send_to_order_des(write_buffer, size))
	{
		login_state = SENT_LOGIN;
		sent_msg = LOGIN_SENT;

		// add audit trail
		audit_struct new_audit;
		memset(&new_audit, 0, sizeof(audit_struct));
        build_audit_time_local(new_audit);
		build_audit_data(new_audit);
		new_audit.direction = FROM_CLIENT;
		new_audit.status = OK;
		new_audit.msg_type = LOGIN;
		et_g_auditTrail->add_audit(new_audit);
        pthread_cond_signal(m_auditCondition);
		// end audit trail

		return true;
	}
    order_message_num--;
	DBG_ERR("fail to send login message.");
	return false;
}


bool exec_globex::send_reset_seq_login()
{
	int size = 0;	
	
	char body_buffer[ORDER_BUFFER_SIZE];
	memset(body_buffer, 0, ORDER_BUFFER_SIZE);
	char* body_ptr = body_buffer;

    // 95 = RawDataLength, password length
	size = build_field_int(body_ptr, 95, (int)strlen(ORDER_PASSWORD));
	body_ptr += size;

	// 96 = RawData, password
	size = build_field_string(body_ptr, 96, ORDER_PASSWORD);
	body_ptr += size;

    // account 
    size = build_field_string(body_ptr, 1, GLOBEX_ACCT);  
    body_ptr += size; 
 
    // SenderSubID   
    size = build_field_string(body_ptr, 50, GLOBEX_ACCT); 
    body_ptr += size;

	// encrypt type
	size = build_field_int(body_ptr, 98, 0);
	body_ptr += size;

	// heart beach method
	size = build_field_string(body_ptr, 108, GLOBEX_HEART_BEAT_INTERVAL);
	body_ptr += size;

	// reset seq num
	order_message_num = 1;
	size = build_field_string(body_ptr, 141, "Y");
	body_ptr += size;
	
	
	char write_buffer[ORDER_BUFFER_SIZE];
	char* write_ptr = write_buffer;

    // header
	make_fix_header(write_buffer, &write_ptr, body_ptr - body_buffer, "A", 0, 0, sent_seq_num);
	// copy body
	memcpy(write_ptr, body_buffer, body_ptr - body_buffer);
	write_ptr += (body_ptr - body_buffer);
	// trailer
	make_fix_trailer(write_buffer, &write_ptr);

	size = write_ptr - write_buffer;

	DBG_DEBUG("login_str: %s", write_buffer);

	if (send_to_order_des(write_buffer, size))
	{
	    sent_msg = LOGIN_SENT;

		// add audit trail
		audit_struct new_audit;
		memset(&new_audit, 0, sizeof(audit_struct));
	
		build_audit_data(new_audit);
		new_audit.direction = FROM_CLIENT;
		new_audit.status = OK;
		new_audit.msg_type = LOGIN;
		build_audit_time_local(new_audit);
		et_g_auditTrail->add_audit(new_audit);
        pthread_cond_signal(m_auditCondition);
		// end audit trail

		return true;
	}
    order_message_num--;
	DBG_ERR("fail to send login message.");

	return false;
}


bool exec_globex::send_logout()
{
	int size = 0;
	
	char body_buffer[ORDER_BUFFER_SIZE];	
	char* body_ptr = body_buffer;
	memset(body_buffer, 0, ORDER_BUFFER_SIZE);

	// text
	size = build_field_string(body_ptr, 58, "logging out");
	body_ptr += size;

    // account 
    size = build_field_string(body_ptr, 1, GLOBEX_ACCT);  
    body_ptr += size; 
 
    // SenderSubID   
    size = build_field_string(body_ptr, 50, GLOBEX_ACCT); 
    body_ptr += size;

	// next_seq_num
	size = build_field_int(body_ptr, 789, 1);
	body_ptr += size;
    
	char write_buffer[ORDER_BUFFER_SIZE];
	char* write_ptr = write_buffer;
	memset(write_buffer, 0, ORDER_BUFFER_SIZE);
    
	// header
	make_fix_header(write_buffer, &write_ptr, body_ptr - body_buffer, "5", 0, 0, sent_seq_num);
	// copy body
	memcpy (write_ptr, body_buffer, body_ptr - body_buffer);
	write_ptr += (body_ptr - body_buffer);
	// trailer
	make_fix_trailer(write_buffer, &write_ptr);

	size = write_ptr - write_buffer;
	
	DBG_DEBUG("size: %d, logout_str: %s", size, write_buffer);

	if (send_to_order_des(write_buffer, size))
	{
		login_state = SENT_LOGOUT;
		sent_msg = LOGOUT_SENT;

		// add audit trail
		audit_struct new_audit;
		memset(&new_audit, 0, sizeof(audit_struct));
	
		build_audit_data(new_audit);
		new_audit.direction = FROM_CLIENT;
		new_audit.status = OK;
		new_audit.msg_type = LOGOUT;
		build_audit_time_local(new_audit);
		et_g_auditTrail->add_audit(new_audit);
        pthread_cond_signal(m_auditCondition);
		// end audit trail

		return true;
	}
    order_message_num--;
	DBG_ERR("fail to send logout message.");

	return false;
}


bool exec_globex::send_heartbeat(const char *id_string)
{
	int size = 0;
	char body_buffer[ORDER_BUFFER_SIZE];
	char* body_ptr = body_buffer;
	memset(body_buffer, 0, ORDER_BUFFER_SIZE);

    if (id_string != NULL)
    {
		size = build_field_string(body_ptr, 112, id_string);
		body_ptr += size;
	}

    // account 
    size = build_field_string(body_ptr, 1, GLOBEX_ACCT);  
    body_ptr += size; 
 
    // SenderSubID   
    size = build_field_string(body_ptr, 50, GLOBEX_ACCT); 
    body_ptr += size;

	char write_buffer[ORDER_BUFFER_SIZE];
	memset(write_buffer, 0, ORDER_BUFFER_SIZE);
	char* write_ptr = write_buffer;
	
	make_fix_header(write_buffer, &write_ptr, body_ptr - body_buffer, "0", 0, 0, sent_seq_num);
	memcpy(write_ptr, body_buffer, body_ptr - body_buffer);
	write_ptr += (body_ptr - body_buffer);
	make_fix_trailer(write_buffer, &write_ptr);

	size = write_ptr - write_buffer;
	
	DBG_DEBUG("heartbeat_str: %s", write_buffer);
    
	if (send_to_order_des(write_buffer, size))
	{
		return true;
	}
    order_message_num--;
	DBG_ERR("fail to send heartbeat message.");

	return false;
}


bool exec_globex::send_test_request()
{
	char body_buffer[ORDER_BUFFER_SIZE];
	memset(body_buffer, 0, ORDER_BUFFER_SIZE);
	char* body_ptr = body_buffer;

	char write_buffer[ORDER_BUFFER_SIZE];
	memset(write_buffer, 0, ORDER_BUFFER_SIZE);
	char* write_ptr = write_buffer;
	
	int size = 0;

	/* id */
	size = build_field_string(body_ptr, 112, "ARE YOU THERE");
	body_ptr += size;

    // account 
    size = build_field_string(body_ptr, 1, GLOBEX_ACCT);  
    body_ptr += size; 
 
    // SenderSubID   
    size = build_field_string(body_ptr, 50, GLOBEX_ACCT); 
    body_ptr += size;

	make_fix_header(write_buffer, &write_ptr, body_ptr - body_buffer, "1", 0, 0, sent_seq_num);
	memcpy(write_ptr, body_buffer, body_ptr - body_buffer);
	write_ptr += (body_ptr - body_buffer);
	make_fix_trailer(write_buffer, &write_ptr);

	size = write_ptr - write_buffer;
	
	DBG_DEBUG("test_request_str: %s", write_buffer);

	if (send_to_order_des(write_buffer, size))
	{
		return true;
	}
    order_message_num--;	
	DBG_ERR("fail to send test request message.");

	return false;
}


bool exec_globex::send_resend_request(int start, int end)
{
	char write_buffer[ORDER_BUFFER_SIZE];
	memset(write_buffer, 0, ORDER_BUFFER_SIZE);
	char *write_ptr = write_buffer;
	
	char body_buffer[ORDER_BUFFER_SIZE];
	memset(body_buffer, 0, ORDER_BUFFER_SIZE);
	char *body_ptr = body_buffer;
	
	int size = 0;

	/* start num */
	size = build_field_int(body_ptr, 7, start);
	body_ptr += size;

    // account 
    size = build_field_string(body_ptr, 1, GLOBEX_ACCT);  
    body_ptr += size; 
 
    // SenderSubID   
    size = build_field_string(body_ptr, 50, GLOBEX_ACCT); 
    body_ptr += size;

	/* end num */
	size = build_field_int(body_ptr, 16, end);
	body_ptr += size;

	make_fix_header(write_buffer, &write_ptr, body_ptr - body_buffer, "2", 0, 0, sent_seq_num);
	memcpy(write_ptr, body_buffer, body_ptr - body_buffer);
	write_ptr += (body_ptr - body_buffer);
	make_fix_trailer(write_buffer, &write_ptr);

	size = write_ptr - write_buffer;
	
	DBG_DEBUG("resend_request_str: %s", write_buffer);

	if (send_to_order_des (write_buffer, size))
	{
	    // add to audit trail
	    audit_struct new_audit;
	    memset(&new_audit, 0, sizeof(audit_struct));
	    
	    build_audit_data(new_audit);
	    new_audit.direction = FROM_CLIENT;
	    new_audit.status = OK;
	    new_audit.msg_type = RESEND_REQUEST;
	    
	    et_g_auditTrail->add_audit(new_audit);
        pthread_cond_signal(m_auditCondition);
	    // end audit trail
	    
	    return true;
	}
    order_message_num--;	
	DBG_ERR("fail to send resend_request message.");

	return false;
}


bool exec_globex::send_sequence_reset_due_to_gap(int new_seq)
{
	char write_buffer[ORDER_BUFFER_SIZE];
	memset(write_buffer, 0, ORDER_BUFFER_SIZE);
	char* write_ptr = write_buffer;
	
	char body_buffer[ORDER_BUFFER_SIZE];
	memset(write_buffer, 0, ORDER_BUFFER_SIZE);
	char* body_ptr = body_buffer;
	
	int size = 0;
	int save_msg_num = 0;

    // account 
    size = build_field_string(body_ptr, 1, GLOBEX_ACCT);  
    body_ptr += size; 
 
    // SenderSubID   
    size = build_field_string(body_ptr, 50, GLOBEX_ACCT); 
    body_ptr += size;

	// NewSeqNo
	size = build_field_int(body_ptr, 36, order_message_num);
	body_ptr += size;

	// GapFillFlag
	size = build_field_string(body_ptr, 123, "Y");
	body_ptr += size;
	
	save_msg_num = order_message_num;
	order_message_num = new_seq;
	make_fix_header(write_buffer, &write_ptr, body_ptr - body_buffer, "4", 1, 1, sent_seq_num);
	order_message_num = save_msg_num;

	memcpy(write_ptr, body_buffer, body_ptr - body_buffer);
	write_ptr += (body_ptr - body_buffer);
	make_fix_trailer(write_buffer, &write_ptr);

	size = write_ptr - write_buffer;

	DBG_DEBUG("buffer: %s.", write_buffer);
	if (send_to_order_des (write_buffer, size))
	{
		return true;
	}
    order_message_num--;	

	DBG_ERR("fail to send send_sequence_reset_due_to_gap message.");

	return false;
}

int exec_globex::send_cancel(string _account, string _tag50, int side, char *ex_order_id, char *orig_cli_order_id,
			     char *correlation_cli_order_id, const char *display_name, char *new_order_id,
			     char* cli_order_id, int total_size, char* expire_date, tif_type tif, char execute_type)
{
	char time_buffer[32];
	char new_id[32];
	
	char body_buffer[ORDER_BUFFER_SIZE];
	memset(body_buffer, 0, ORDER_BUFFER_SIZE);
	char* body_ptr = body_buffer;
	
	char write_buffer[ORDER_BUFFER_SIZE];
	memset(write_buffer, 0, ORDER_BUFFER_SIZE);
	char* write_ptr = write_buffer;
	
	int size = 0;
		
	// account
	size = build_field_string(body_ptr, 1, _account.c_str());
	body_ptr += size;

	memset(new_id, 0, 32);
	sprintf(new_id, "CANC%04X", globex_seq_order_id);
	globex_seq_order_id++;
	
	// update new id
	strcpy(new_order_id, new_id);

	// 11 = ClOrdID
	size = build_field_string(body_ptr, 11, new_id);
	body_ptr += size;

	// 37 = OrderID
	size = build_field_string(body_ptr, 37, ex_order_id);
	body_ptr += size;

	// OrigClOrdID
	size = build_field_string(body_ptr, 41, orig_cli_order_id);
	body_ptr += size;

    // SenderSubID 
    size = build_field_string(body_ptr, 50, _tag50.c_str());  
    body_ptr += size;

	// side
	size = build_field_int(body_ptr, 54, side + 1);
	body_ptr += size;

	// symbol
	// may have to change this when we bring in ice
	size = build_field_string(body_ptr, 55, display_name);
	body_ptr += size;

	memset(time_buffer, 0, 32);
	EtUtil::build_time_buffer(time_buffer, sizeof(time_buffer));
	
	// sending time
	size = build_field_string(body_ptr, 60, time_buffer);
	body_ptr += size;

	// symbol des, this is what we will us to look up inst
	// may have to change this when we bring in ice
	size = build_field_string(body_ptr, 107, display_name);
	body_ptr += size;

	// inst type
	// YT, should get this from some func
	size = build_field_string(body_ptr, 167, "FUT");
	body_ptr += size;

	// CorrelationClOrdID
	size = build_field_string(body_ptr, 9717, correlation_cli_order_id);
	body_ptr += size;
	
	// header
	make_fix_header(write_buffer, &write_ptr, body_ptr - body_buffer, "F", 0, 0, sent_seq_num);
	
	// copy body
	memcpy(write_ptr, body_buffer, body_ptr - body_buffer);
	write_ptr += (body_ptr - body_buffer);
	
	// trailer
	make_fix_trailer (write_buffer, &write_ptr);

	size = write_ptr - write_buffer;
	DBG_DEBUG("cancel_str: %s", write_buffer);
    if (send_to_order_des(write_buffer, size))
    {
      sent_msg = CANCEL_SENT;
		// add to audit trail
		audit_struct new_audit;
		memset(&new_audit, 0, sizeof(audit_struct));

		build_audit_data(new_audit);
		new_audit.direction = FROM_CLIENT;
		new_audit.status = OK;
		new_audit.msg_type = CANCEL;

		strcpy(new_audit.security_des, display_name);

		// need to get information from security definition
		build_audit_from_definition(new_audit, display_name);

		new_audit.order_side = side;
		new_audit.order_type = execute_type;
		new_audit.order_qualifier = tif;
        strcpy(new_audit.maturity_date, expire_date);
        //strcpy(new_audit.inst_code, g_product_name.c_str());
        strcpy(new_audit.inst_code, inst_code.c_str());
        new_audit.quantity = total_size;
        strcpy(new_audit.clordid, new_id);
        strcpy(new_audit.correlation_clordid, cli_order_id);
        strcpy(new_audit.orderid, ex_order_id);
		new_audit.customer = INDIVIDUAL_TRADING;
		new_audit.origin = 1;
        strcpy(new_audit.senderid, _tag50.c_str()); 
        strcpy(new_audit.account, _account.c_str());

		//new_audit.give_up = giveup;

		// currently no give up order
		//new_audit.give_up_firm = give_up_firm;
		//new_audit.give_up_account = give_up_account;

		build_audit_time_local(new_audit);
		et_g_auditTrail->add_audit(new_audit);
        pthread_cond_signal(m_auditCondition);
		// end audit trail

        //m_send_msg_num++;

        int iaccount = EtReadConfig::getAccountId(_account);
        if(iaccount != -1){
          (EtReadConfig::m_account[iaccount].quote)++;
        }
        else{
			DBG_ERR("fail to get account id");
        }
    	return true;
    }
    order_message_num--;    
	DBG_ERR("fail to send cancel order.");

	return false;
}

/**
 * send a new order.
 * return:
 *    false: error
 *     true: ok
 */
bool exec_globex::send_order(string _account, string _tag50, const char *display_name, int side, int price, int size, int max_show, 
                             tif_type tif, char execute_type, char* cli_order_id, int& sent_seq_num)
{
	char time_buffer[32];
	char new_id[32];
	
	char write_buffer[ORDER_BUFFER_SIZE];
	memset(write_buffer, 0, ORDER_BUFFER_SIZE);
	char* write_ptr = write_buffer;
	
	char body_buffer[ORDER_BUFFER_SIZE];
	memset(body_buffer, 0, ORDER_BUFFER_SIZE);
	char* body_ptr = body_buffer;
		
	int len = 0;

	// account
	len = build_field_string(body_ptr, 1, _account.c_str());
	body_ptr += len;

	memset(new_id, 0, 32);
	sprintf(new_id, "ORDO%04X", globex_seq_order_id);
	strcpy(cli_order_id, new_id);
	globex_seq_order_id++;
	
	// ClOrdID
	len = build_field_string(body_ptr, 11, new_id);
	body_ptr += len;

	// handle inst
	len = build_field_int(body_ptr, 21, 1);
	body_ptr += len;

	// size
	len = build_field_int(body_ptr, 38, size);
	body_ptr += len;

	// order type
	len = build_field_char(body_ptr, 40, execute_type);
	body_ptr += len;

	// price
	if (execute_type != MARKET_LIMIT_ORDER && 
		execute_type != MARKET_ORDER && 
		execute_type != STOP_ORDER)
	{
      len = build_field_int(body_ptr, 44, price);
      //len = build_field_double(body_ptr, 44, price);
		body_ptr += len;
	}

    // SenderSubID
    len = build_field_string(body_ptr, 50, _tag50.c_str()); 
    body_ptr += len;

	// side
	len = build_field_int(body_ptr, 54, side + 1);
	body_ptr += len;

	// symbol
	// may have to change this when we bring in ice
	len = build_field_string(body_ptr, 55, display_name);
	body_ptr += len;

	// tif
	switch(tif)
	{
		case DAY:
			len = build_field_string(body_ptr, 59, "0");
			break;

		case GOOD_TILL_CANCEL:
			len = build_field_string(body_ptr, 59, "1");
			break;
	
		case FILL_OR_KILL:
			len = build_field_string(body_ptr, 59, "3");
			break;

		case GOOD_TILL_DATE:
			len = build_field_string(body_ptr, 59, "6");
			break;
			
		default:
          //DBG_ERR("this tif: %d is error", tif);
			return false;
	}
	body_ptr += len;

	memset(time_buffer, 0, sizeof(time_buffer));
	EtUtil::build_time_buffer(time_buffer, sizeof(time_buffer));
	
	// sending time
	len = build_field_string(body_ptr, 60, time_buffer);
	body_ptr += len;

    /*
	// stop price
	if (execute_type == STOP_ORDER || 
		execute_type == STOP_LIMIT_ORDER)
	{
		len = build_field_double(body_ptr, 99, stop_price);
		body_ptr += len;
	}
    */

	// symbol des, this is what we will us to look up inst
	// may have to change this when we bring in ice

	len = build_field_string(body_ptr, 107, display_name);
	body_ptr += len;

    /*
	// MinQty
	if (tif == FILL_OR_KILL)
	{
		len = build_field_int(body_ptr, 110, size);
		body_ptr += len;
	}
	else if (min_size > 0)
	{
		len = build_field_int(body_ptr, 110, min_size);
		body_ptr += len;
	}
    */

	// inst type
	// YT, should get this from some func
	len = build_field_string(body_ptr, 167, "FUT");
	body_ptr += len;

	// firm or customer
	len = build_field_int(body_ptr, 204, 0);
	body_ptr += len;

	// MaxShow
	if (max_show > 0)
	{
		len = build_field_int(body_ptr, 210, max_show);
		body_ptr += len;
	}

	// expire date
	if (tif == GOOD_TILL_DATE)
	{
		len = build_field_string(body_ptr, 432, EXPIRE_DATE);
		body_ptr += len;
	}

	// firm or customer
	len = build_field_int(body_ptr, 9702, 1);
	body_ptr += len;

    /*	
	if (giveup)
	{
		// firm id
		len = build_field_string(body_ptr, 9707, FIRM_ID);
		body_ptr += len;

		len = build_field_string(body_ptr, 9708, "GU");
		body_ptr += len;
	}
    */

	// order_id
	len = build_field_string(body_ptr, 9717, new_id);
	body_ptr += len;

	// header
	make_fix_header(write_buffer, &write_ptr, body_ptr - body_buffer, "D", 0, 0, sent_seq_num);
	
	// copy body
	memcpy(write_ptr, body_buffer, body_ptr - body_buffer);
	write_ptr += (body_ptr - body_buffer);

	// store package
	if ( !PackageStore::add_package(write_buffer, write_ptr - write_buffer, order_message_num - 1, "D") )
	{
      //DBG_ERR("fail to add a package to list.");
		return false;
	}
	
	// trailer
	make_fix_trailer(write_buffer, &write_ptr);
	
	len = write_ptr - write_buffer;
	
	DBG_DEBUG("send_order: %s", write_buffer);

	if (send_to_order_des(write_buffer, len))
	{
	    sent_msg = ORDER_SENT;
		// add to audit trail
		audit_struct new_audit;
		memset(&new_audit, 0, sizeof(audit_struct));
	
		build_audit_data(new_audit);
		new_audit.direction = FROM_CLIENT;
		new_audit.status = OK;
		new_audit.msg_type = NEW_ORDER;

		strcpy(new_audit.security_des, display_name);

		// need to get information from security definition
		build_audit_from_definition(new_audit, display_name);

        strcpy(new_audit.clordid, cli_order_id);
        strcpy(new_audit.correlation_clordid, cli_order_id);
        //strcpy(new_audit.inst_code, g_product_name.c_str());
        strcpy(new_audit.inst_code, inst_code.c_str());
		new_audit.limit_price = price;
		//new_audit.stop_price = stop_price;
		new_audit.order_side = side;
		new_audit.quantity = size;
		new_audit.order_type = execute_type;
		new_audit.order_qualifier = tif;
		new_audit.customer = INDIVIDUAL_TRADING;
		new_audit.origin = 1;
        strcpy(new_audit.senderid, _tag50.c_str());
        strcpy(new_audit.account, _account.c_str());

        if(max_show > 0){
          new_audit.maxshow = max_show;
        }

        /*
        if(giveup)
          {
            new_audit.give_up = GU;
          }
        */

		// currently no give up order
		//new_audit.give_up_firm = give_up_firm;
		//new_audit.give_up_account = give_up_account;

		build_audit_time_local(new_audit);
		et_g_auditTrail->add_audit(new_audit);
        pthread_cond_signal(m_auditCondition);
		// end audit trail

        //m_send_msg_num++;
        int iaccount = EtReadConfig::getAccountId(_account); 
        if(iaccount != -1){ 
          (EtReadConfig::m_account[iaccount].quote)++; 
        } 
        else{ 
			DBG_DEBUG("fail to get account id");
        } 
		return true;
	}
    order_message_num--;
	DBG_ERR("fail to send order to server.");

	return false;
}


bool exec_globex::send_alter(string _account, string _tag50, char* ex_order_id, char* orig_cli_order_id, 
	char* correlation_cli_order_id, int side, double price, 
	const char* display_name, int size, int max_show, char *new_order_id,
	double stop_price, tif_type tif, char execute_type, bool giveup)
{
	char write_buffer[ORDER_BUFFER_SIZE];
	memset(write_buffer, 0, ORDER_BUFFER_SIZE);
	char *write_ptr = write_buffer;
	
	char body_buffer[ORDER_BUFFER_SIZE];
	memset(body_buffer, 0, ORDER_BUFFER_SIZE);
	char *body_ptr = body_buffer;

	char time_buffer[32];
	char new_id[32];
	int	length = 0;

	if (size <= 0)
	{
      //DBG_ERR("size is error.");
		return false;
	}

	// account
	length = build_field_string(body_ptr, 1, _account.c_str());
	body_ptr += length;

	sprintf(new_id, "ALTA%04X", globex_seq_order_id);
	globex_seq_order_id++;
	
	/* update new id */
	strcpy(new_order_id, new_id);

	// order_id
	length = build_field_string(body_ptr, 11, new_id);
	body_ptr += length;

	// handle inst
	length = build_field_int(body_ptr, 21, 1);
	body_ptr += length;

	// order_id
	length = build_field_string(body_ptr, 37, ex_order_id);
	body_ptr += length;

	// size
	length = build_field_int(body_ptr, 38, size);
	body_ptr += length;
	
	// order type
	length = build_field_int(body_ptr, 40, 2);
	body_ptr += length;

	// OrigClOrdID
	length = build_field_string(body_ptr, 41, orig_cli_order_id);
	body_ptr += length;

	// price
	length = build_field_double(body_ptr, 44, price);
	body_ptr += length;

    // SenderSubID 
    size = build_field_string(body_ptr, 50, _tag50.c_str());  
    body_ptr += size;

	// side
	length = build_field_int(body_ptr, 54, side + 1);
	body_ptr += length;

	/* symbol */
	/* may have to change this when we bring in ice */
	length = build_field_string(body_ptr, 55, display_name);
	body_ptr += length;

    /* sending time */
    memset(time_buffer, 0, sizeof(time_buffer));
	EtUtil::build_time_buffer(time_buffer, sizeof(time_buffer));
	length = build_field_string(body_ptr, 60, time_buffer);
	body_ptr += length;

	/* symbol des, this is what we will us to look up inst */
	/* may have to change this when we bring in ice */
	length = build_field_string(body_ptr, 107, display_name);
	body_ptr += length;

	// inst type
	// YT, should get this from some func
	length = build_field_string(body_ptr, 167, "FUT");
	body_ptr += length;

	// 204: CustomerOrFirm
	length = build_field_int(body_ptr, 204, 0);
	body_ptr += length;

	// maximum quantity
	if (max_show > 0)
	{
		// 210: MaxShow
		length = build_field_int(body_ptr, 210, max_show);
		body_ptr += length;
	}

	// 9702: CtiCode, firm or customer
	length = build_field_int(body_ptr, 9702, 1);
	body_ptr += length;

	// CorrelationClOrdID
	length = build_field_string(body_ptr, 9717, correlation_cli_order_id);
	body_ptr += length;

	// IFM
	length = build_field_string(body_ptr, 9768, "Y");
	body_ptr += length;

	// header
	make_fix_header(write_buffer, &write_ptr, body_ptr - body_buffer, "G", 0, 0, sent_seq_num);
	
	// copy body
	memcpy (write_ptr, body_buffer, body_ptr - body_buffer);
	write_ptr += (body_ptr - body_buffer);
	
	// trailer
	make_fix_trailer(write_buffer, &write_ptr);

	DBG_DEBUG("order_str: %s.", write_buffer);
	length = write_ptr - write_buffer;
	if (send_to_order_des(write_buffer, length))
	{
	    sent_msg = ALTER_SENT;
		// add to audit trail
		audit_struct new_audit;
		memset(&new_audit, 0, sizeof(audit_struct));
	
		build_audit_data(new_audit);
		new_audit.direction = FROM_CLIENT;
		new_audit.status = OK;
		new_audit.msg_type = MODIFY;

		strcpy(new_audit.security_des, display_name);

		// need to get information from security definition
		build_audit_from_definition(new_audit, display_name);

		new_audit.limit_price = price;
		//new_audit.stop_price = stop_price;
		new_audit.order_side = side;
		new_audit.order_type = execute_type;
		new_audit.order_qualifier = tif;
        new_audit.quantity = size;
        strcpy(new_audit.clordid, new_id);
        strcpy(new_audit.correlation_clordid, correlation_cli_order_id);
        strcpy(new_audit.orderid, ex_order_id);
        //strcpy(new_audit.inst_code, g_product_name.c_str());
        strcpy(new_audit.inst_code, inst_code.c_str());
		new_audit.customer = INDIVIDUAL_TRADING;
		new_audit.origin = 1;
        strcpy(new_audit.senderid, _tag50.c_str()); 
        strcpy(new_audit.account, _account.c_str());

        if(max_show > 0){ 
          new_audit.maxshow = max_show; 
        } 

        if(giveup)
          {
            new_audit.give_up = GU;
          }

		// currently no give up order
		//new_audit.give_up_firm = give_up_firm;
		//new_audit.give_up_account = give_up_account;
		build_audit_time_local(new_audit);
		et_g_auditTrail->add_audit(new_audit);
        pthread_cond_signal(m_auditCondition);
		// end audit trail

        //m_send_msg_num++;
        int iaccount = EtReadConfig::getAccountId(_account); 
        if(iaccount != -1){ 
          (EtReadConfig::m_account[iaccount].quote)++; 
        } 
        else{ 
			DBG_ERR("fail to get account id");
        } 
		return true;
	}
    order_message_num--;	
	DBG_ERR("fail to send alter.");

	return false;
}


bool exec_globex::send_resend_order(int _beginSeqNum, int _endSeqNum)
{
	int seq_num = _beginSeqNum;
	package_struct* pakPtr = NULL;
    
	if (last_request_seq_num == seq_num)
	{
    	return 0;
	}
	
	last_request_seq_num = seq_num;
    
	while (true)
	{
		pakPtr = PackageStore::get_package(seq_num);
		if (NULL == pakPtr)
		{
			DBG_ERR("Fail to get a package.");
			DBG_DEBUG("seq_num: %d, order_message_num: %d", seq_num, order_message_num);
			if ( seq_num == order_message_num )
			{
				break;
			}
			else if (seq_num < order_message_num)
			{
				return send_sequence_reset_due_to_gap(seq_num);
			}
			else
			{
				DBG_ERR("seq_num (%d) is larger than order_message_num (%d)", 
						seq_num, order_message_num);
				break;
			}
		}
        seq_num++;

		// add more info before send on resend messages
		char write_buffer[1024];
		char* write_ptr = NULL;
		char* curr_str = NULL;
		char* dup_str = NULL;
		char body_buffer[1024];
		char time_buffer[32];
		char* body_ptr = body_buffer;
		int size = 0;

		write_ptr = write_buffer;
		
		memset(body_buffer, 0, 1024);
		memcpy(body_ptr, pakPtr->buffer, pakPtr->buffer_len);
		body_ptr += pakPtr->buffer_len;

		// add tag 369 for last processed sequence number
		size = build_field_int(body_ptr, 369, _endSeqNum);
		body_ptr += size;

		char *len_ptr = NULL;
		char *start_count_ptr = NULL;
		char *end_count_ptr = NULL;
		int t_write_byte_count = 0;

		// add tag 122, can be the same value as tag 52
		char timeFlag[5];
		timeFlag[0] = SOH;
		timeFlag[1] = '5';
		timeFlag[2] = '2';
		timeFlag[3] = '=';
		timeFlag[4] = '\0';
		curr_str = 	strstr(body_buffer, timeFlag);
		curr_str += (1 + strlen("52="));

		memset(time_buffer, 0, 32);
		memcpy(time_buffer, curr_str, 21);
		time_buffer[21] = '\0';
		size = build_field_string(body_ptr, 122, time_buffer);
		body_ptr += size;

		// modify tag 9 body length
		char startFlag[4];
		startFlag[0] = SOH;
		startFlag[1] = '9';
		startFlag[2] = '=';
		startFlag[3] = '\0';
        len_ptr = start_count_ptr = (strstr(body_buffer, startFlag)  + 1 + strlen("9="));

		memset(len_ptr, 'x', 6);
		start_count_ptr += (6 + 1);

		end_count_ptr = body_ptr;
		t_write_byte_count = (end_count_ptr - start_count_ptr);
		body_len(len_ptr, t_write_byte_count);

		// modify tag 43, make it to "Y"
		char dupFlag[4];
		dupFlag[0] = SOH;
		dupFlag[1] = '4';
		dupFlag[2] = '3';
		dupFlag[3] = '=';	
		dupFlag[4] = '\0';
		dup_str = 	strstr(body_buffer, dupFlag);
		dup_str += (1 + strlen("43="));
		*dup_str = 'Y';

		// copy body
		memset(write_buffer, 0, 1024);
		memcpy(write_ptr, body_buffer, body_ptr - body_buffer);
		write_ptr += (body_ptr - body_buffer);

		// trailer
		make_fix_trailer(write_buffer, &write_ptr);
		
		size = write_ptr - write_buffer;
		
		DBG_DEBUG("resend: %s", write_buffer);

		if( send_to_order_des(write_buffer, size))
		{
			return true;
		}
	}
    order_message_num--;
	return false;
}


/**
 * 2009/12/09: currently this method is useless.
 */
bool exec_globex::send_quote(int no_related_sym, const char* symbol, const char* display_name, 
	int order_qty, int side, bool two_side, int quote_type, char* quote_req_id)
{
	char time_buffer[32];
	char new_id[32];
	
	char write_buffer[ORDER_BUFFER_SIZE];
	memset(write_buffer, 0, ORDER_BUFFER_SIZE);
	char *write_ptr = write_buffer;
	
	char body_buffer[ORDER_BUFFER_SIZE];
	memset(body_buffer, 0, ORDER_BUFFER_SIZE);
	char* body_ptr = body_buffer;
	
	int	size = 0;

	// QuoteReqID
	sprintf(new_id, "QUOO%04X", globex_seq_order_id);
	strcpy(quote_req_id, new_id);
	globex_seq_order_id++;

    // account 
    size = build_field_string(body_ptr, 1, GLOBEX_ACCT);  
    body_ptr += size; 
 
    // SenderSubID   
    size = build_field_string(body_ptr, 50, GLOBEX_ACCT); 
    body_ptr += size;

	// quote id
	size = build_field_string(body_ptr, 131, new_id);
	body_ptr += size;

	// NoRelatedSym
	size = build_field_int(body_ptr, 146, no_related_sym);
	body_ptr += size;

	// Symbol
	size = build_field_string(body_ptr, 55, symbol);
	body_ptr += size;

    // OrderQty
    if (side == CROSS_SIDE)
    {
    	size = build_field_int(body_ptr, 38, order_qty);
		body_ptr += size;
	}

	// side
	if (!two_side)
	{
		size = build_field_int(body_ptr, 54, side + 1);
		body_ptr += size;
	}
	
	// TransactTime
    memset(time_buffer, 0, 32);
	EtUtil::build_time_buffer(time_buffer, 32);

	size = build_field_string(body_ptr, 60, time_buffer);
	body_ptr += size;
	
	// SecurityDesc
	// symbol des, this is what we will us to look up inst
	// may have to change this when we bring in ice
	size = build_field_string(body_ptr, 107, display_name);
	body_ptr += size;
	
	// SecurityType
	// YT, should get this from some func
	size = build_field_string(body_ptr, 167, "FUT");
	body_ptr += size;
	
	// QuoteType
	if (side != CROSS_SIDE)
	{
		size = build_field_int(body_ptr, 9943, quote_type);
		body_ptr += size;
	}
	
	// header
	make_fix_header(write_buffer, &write_ptr, body_ptr - body_buffer, "R", 0, 0, sent_seq_num);
	// copy body
	memcpy(write_ptr, body_buffer, body_ptr - body_buffer);
	write_ptr += (body_ptr - body_buffer);
	// trailer
	make_fix_trailer(write_buffer, &write_ptr);
	
	DBG_DEBUG("quote_str: %s.", write_buffer);
	size = write_ptr - write_buffer;
	if (send_to_order_des(write_buffer, size))
	{
		return true;
	}
    order_message_num--;	
	DBG_ERR("fail to send quote.");

	return false;
}


/**
 * send buffer to order server.
 *    true: ok
 *   false: error
 */
bool exec_globex::send_to_order_des(const char* _buffer, int _length)
{
	if (_buffer == NULL || _length <= 0)
	{
		DBG_ERR("parameter is error.");
		return false;
	}
	
	if (EtUtil::sendData(order_des, _buffer, _length))
	{
		return true;
	}

	DBG_ERR("fail to send to order server.");
	close(order_des);
	order_des = -1;
	return false;
}


bool exec_globex::make_fix_header(char *write_buffer, char **write_ptr, 
				  int body_size, const char *msg_type, int is_resend, int send_orig_sending_time, int& sent_seq_num)
{
	char time_buffer[32];
	char comp_id[32];

	int byte_count = 0;
	int size = 0;
	
	char* len_ptr = NULL;
	
	char* start_count_ptr = NULL;
	char* end_count_ptr = NULL;

	*write_ptr = write_buffer;

	// begin string
	size = build_field_string(*write_ptr, 8, "FIX.4.2");
	*write_ptr += size;

	// body len
	// 9=xxxxxx
	len_ptr = *write_ptr + 2;
	size = build_field_string(*write_ptr, 9, "xxxxxx");
	*write_ptr += size;

	start_count_ptr = *write_ptr;

	// msg type
	size = build_field_string(*write_ptr, 35, msg_type);
	*write_ptr += size;

	// sequence number 
	size = build_field_int(*write_ptr, 34, order_message_num);
	sent_seq_num = order_message_num;
	*write_ptr += size;
	order_message_num++;

	// is dup
	if (is_resend)
	{
		size = build_field_string(*write_ptr, 43, "Y");
	}
	else
	{
		size = build_field_string(*write_ptr, 43, "N");
	}
	*write_ptr += size;


	// SenderCompID
	memset(comp_id, 0, 32);
	strcpy(comp_id, SESSION_ID);
	/*
	// *start* change FIRM ID, route through test only	
	if(msg_type == "D")
	{
		char firmid[5];
		memset(firmid, 0, 5);
		cout << "Enter the firm id (826 or 033): ";
		cin >> firmid;	
		strcat(comp_id, firmid);
	}
	else
	{
		strcat(comp_id, FIRM_ID);
	}
	// *end* chage FIRM ID, route through test only	
	*/
	strcat(comp_id, FIRM_ID);
	strcat(comp_id, "N");
	
	size = build_field_string(*write_ptr, 49, comp_id);
	*write_ptr += size;

    /*
	// SenderSubID
	size = build_field_string(*write_ptr, 50, _tag50.c_str());
	*write_ptr += size;
    */

    // sending time
    memset(time_buffer, 0, 32);
    EtUtil::build_time_buffer(time_buffer, 32);
	size = build_field_string(*write_ptr, 52, time_buffer);
	*write_ptr += size;

	// target id
	size = build_field_string(*write_ptr, 56, "CME");	
	*write_ptr += size;;

	// target id
	size = build_field_string(*write_ptr, 57, "G");
	*write_ptr += size;

	if (send_orig_sending_time)
	{
		size = build_field_string(*write_ptr, 122, time_buffer);
		*write_ptr += size;
	}

	// location id
	size = build_field_string(*write_ptr, 142, "24th street");
	*write_ptr += size;

	end_count_ptr = *write_ptr;

	byte_count = (end_count_ptr - start_count_ptr) + body_size;
	body_len(len_ptr, byte_count);
	
	return true;
}


void exec_globex::body_len(char*& len_ptr, int& t_write_byte_count)
{
    len_ptr[0] = (char)((t_write_byte_count / 100000) + 0x30);
    t_write_byte_count = t_write_byte_count % 100000;
    len_ptr[1] = (char)((t_write_byte_count / 10000) + 0x30);
    t_write_byte_count = t_write_byte_count % 10000;
    len_ptr[2] = (char)((t_write_byte_count / 1000) + 0x30);
    t_write_byte_count = t_write_byte_count % 1000;
    len_ptr[3] = (char)((t_write_byte_count / 100) + 0x30);
    t_write_byte_count = t_write_byte_count % 100;
    len_ptr[4] = (char)((t_write_byte_count / 10) + 0x30);
    t_write_byte_count = t_write_byte_count % 10;
    len_ptr[5] = (char)((t_write_byte_count / 1) + 0x30);
}


bool exec_globex::make_fix_trailer(char *write_buffer, char **write_ptr)
{
    int t_write_byte_count = 0;
	int int_check_sum = 0;
	unsigned char char_check_sum;
	char check_sum[3] = { 0, 0, 0};
	
	const char *curr_str = NULL;
	
	char* ptr = NULL;
	for (ptr = write_buffer; ptr < *write_ptr; ptr++)
	{
		int_check_sum += *ptr;
	}

	// location id
	curr_str = "10=";
	memcpy(*write_ptr, curr_str, strlen(curr_str));
	*write_ptr += strlen(curr_str);

	char_check_sum = (unsigned char)int_check_sum;
	t_write_byte_count = char_check_sum;

	check_sum[0] = (char)((t_write_byte_count / 100) + 0x30);
	t_write_byte_count = t_write_byte_count % 100;
	check_sum[1] = (char)((t_write_byte_count / 10) + 0x30);
	t_write_byte_count = t_write_byte_count % 10;
	check_sum[2] = (char)((t_write_byte_count / 1) + 0x30);

	curr_str = check_sum;
	memcpy(*write_ptr, curr_str, 3);
	*write_ptr += 3;

	**write_ptr = SOH;
	*write_ptr += 1;

	return true;
}

