
#ifndef __READ_CME_DATA_H__
#define __READ_CME_DATA_H__

#include <pthread.h>
#include <queue>

#include "audit_trail.h"
#include "globex_common.h"

using namespace std;


enum cme_response_type
{
	cme_login,
	cme_logout,
	cme_session_reject,
	cme_business_reject,
	cme_heartbeat_request,
	cme_resend_request,
	cme_resend_order,
	cme_order_reject,
	cme_order_accepted,
	cme_order_filled_spread,
    cme_order_filled_leg,
	cme_order_cancelled,
	cme_order_altered,
	cme_cancelalter_reject
};


struct cme_response_struct
{
	int type;  
  char symbol[16];
  char id[16];
  char account[8];
	int seq_num;
    int sent_seq_num;
	int recv_num;
	int reject_reason;
	char cli_order_id[MAX_STRING_ORDER_ID];
	char ex_order_id[MAX_STRING_ORDER_ID];
	int price;
	int size;
	char text[100];
};


class EtReadCmeData
{
public:
		
	EtReadCmeData();
	~EtReadCmeData();
	
	// ---- function ----
	void setRecvSocket(int _socket);
	void setThreadCondition(pthread_cond_t* _cond);
    void setAuditCondition(pthread_cond_t* _cond);
	
	bool startReadCme();
	void stopReadCme();
	bool isExit();
	
	bool popData(cme_response_struct& _client_data);
	bool isEmpty();

    int get_fix_field(fix_message_struct *fix_msg, const char *look_for,
			  fix_field_struct *ret_field);
    
    // ---- data ----
    int recv_order_message_num;

private:
	
    // ---- function ----
	static void* readOrderThread(void*);
    //static void* cmeHeartbeatThread(void*);
	
	bool addData(cme_response_struct& _response);
	bool split_buffer();
	
	int parse_fix(char *buffer, char **buffer_ptr);
	
	int parse_fix_get_field(char *buffer, char **buffer_ptr, int *field_size, 
		                    fix_field_struct **fix_field);

	int parse_fix_get_message(char *buffer, char **buffer_ptr, 
                              fix_message_struct **fix_msg);
	
	void print_fix_struct(fix_message_struct*);
	int	free_fix_struct(fix_message_struct*);
    bool build_audit_data(audit_struct& new_audit);
    bool build_audit_from_definition(audit_struct& new_audit, string security_id);
	
	void handle_globex_session_reject(fix_message_struct*);
	void handle_globex_business_reject_message(fix_message_struct*);
	bool handle_globex_login(fix_message_struct*);
	bool handle_globex_logout(fix_message_struct*);
	void handle_globex_heartbeat_request(fix_message_struct*);
	bool handle_globex_resend_reqest(fix_message_struct*);	
	bool handle_globex_sequence_reqest(fix_message_struct*);
	bool handle_globex_order_message(fix_message_struct*);
	bool handle_globex_cancel_reject_message(fix_message_struct*);
	
	int handle_globex_quote_acknowledge(fix_message_struct *fix_msg);
	
	
	// ---- data ----
	//static const int recv_buffer_size = 1024;
	
	int m_revcSocket;

	pthread_t m_recvThread;
    pthread_t m_heartbeatThread;
	pthread_cond_t* m_threadCondition;
    pthread_cond_t* m_auditCondition;

	//int recv_order_message_num;
	bool m_stopFlag;
	int m_recvSocket;

	char m_endFlag[5];
    char m_sohFlag[2];
	int m_endFlagLen;
    bool bool_resend_request;
	
	queue<cme_response_struct> m_queue;
	pthread_mutex_t m_mutex;

	char m_cmeRecvBuff[ORDER_BUFFER_SIZE];
	int m_recvSize;
	char m_leaveBuff[ORDER_BUFFER_SIZE]; 
	int m_leaveLen; 
	char m_parseBuff[ORDER_BUFFER_SIZE * 2]; 
	int m_parseLen;
};

#endif
