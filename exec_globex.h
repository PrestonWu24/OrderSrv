
#ifndef _EXEC_GLOBEX_H_
#define _EXEC_GLOBEX_H_

#include <fstream>
#include "order_book.h"
#include "audit_trail.h"
#include "globex_common.h"

class exec_globex
{
public:
		
	exec_globex();
	~exec_globex();
	
	// ---- function ----
	// ---------- exec_globex.cpp ----------
    void setAuditCondition(pthread_cond_t* _cond);
    bool loginOrderServer();
	int connectOrderServer();
    
    bool send_login();
	bool send_logout();
	bool send_heartbeat(const char*);
	bool send_resend_request(int start, int end);
	bool send_resend_order(int _beginSeqNum, int _endSeqNum);
    bool place_order(string _account, string _tag50, int _local_num, char* display_name, int side, int dec_price,  
                                  int size, int max_show, tif_type tif, char execute_type);
    bool alter_order(string _account, string _tag50, int _global_num,  double _price, int _size, int max_show);
	bool cancel_order(string _account, string _tag50, int _global_num, char* _reason);
    bool build_audit_time_local(audit_struct& new_audit);

    
	// ---------- exec_globex.cpp end ----------
	
	// ---- data ----
	login_state_type login_state;
	int globex_audit_id;
	sent_msg_type sent_msg;
    string inst_code;
    //int m_send_msg_num;

private:

	// data
	int order_des;
	int order_message_num;
	int last_request_seq_num;
	int sent_seq_num;
	static int global_order_num;
	unsigned int globex_seq_order_id;        // tag 11, ClOrdID
	fstream seqfile;
    pthread_cond_t* m_auditCondition;

    // function
	// -------- exec_globex.cpp --------
	void body_len(char*& len_ptr, int& t_write_byte_count);
	// -------- exec_globex.cpp, end --------
	
	// ---------- exec_globex_cmd.cpp ----------
	int build_field_int(char* _buffer, const int _tag, const int _iVal);
	int build_field_string(char* _buffer, const int _tag, const char* _strVal);
	int build_field_char(char* _buffer, const int _tag, const char _cVal);
	int build_field_double(char* _buffer, const int _tag, const double _dVal);

    bool build_audit_data(audit_struct& new_audit);
    bool build_audit_from_definition(audit_struct& new_audit, string security_id);
	
	bool make_fix_header(char *write_buffer, char **write_ptr, int body_size, const char *msg_type, 
			     int is_resend, int send_orig_sending_time, int& sent_seq_num);

	bool make_fix_trailer(char *write_buffer, char **write_ptr);
	bool send_to_order_des(const char* _buffer, int _length);
	
	bool send_test_request();
	bool send_reset_seq_login();
	bool send_sequence_reset_due_to_gap(int);
	
    bool send_order(string _account, string _tag50, const char *display_name, int side, int price, int size, int max_show,  
                                 tif_type tif, char execute_type, char* cli_order_id, int& sent_seq_num);
	
	bool send_alter(string _account, string _tag50, char *order_id, char *orig_cli_order_id, char *correlation_cli_order_id, 
                   int side, double price, const char *display_name, int size, int max_show,
                   char *new_order_id, double stop_price, tif_type tif, char execute_type, bool giveup);
	
    int send_cancel(string _account, string _tag50, int side, char *ex_order_id,char *orig_cli_order_id,
			char *correlation_cli_order_id, const char *display_name,
			char *new_order_id, char* cli_order_id, int size, char* expire_date, tif_type tif, char execute_type);
                    
    /**
     * parameter:
	 *   quote_type:
	 * 	   0 = indicative (Eurodollar options only)
	 *     1 = tradable
	 */
	bool send_quote(int no_related_sym, const char* symbol, const char* display_name, int order_qty, int side, 
	               bool two_side, int quote_type, char* quote_req_id);
	// ---------- exec_globex_cmd.cpp end ----------
	
};

#endif
