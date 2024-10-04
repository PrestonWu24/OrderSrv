#ifndef AUDIT_TRAIL_H
#define AUDIT_TRAIL_H

#include <queue>
#include <iostream>
#include <string>
#include <string.h>
#include <cstring>
#include <fstream>

#include "order_book.h"
#include "globex_common.h"
using namespace std;

// -------------
// struct
// -------------

enum exchange_code_type
{
	XOCH = 1,
	XCBT,
	XCME,
	XNYM,
	XCEC,
	XKBT,
	XMGE,
	DUMX,
	XBMF
};

enum message_direction_type
{
	TO_CME = 1,
	FROM_CME,
	TO_CLIENT,
	FROM_CLIENT
};

enum status_type
{
	OK = 1,
	REJECT
};

enum message_type
{
	LOGIN = 1,
	RESET_SEQ_LOGIN,
	LOGOUT,
	RESEND_REQUEST,
	NEW_ORDER,
	MODIFY,
	CANCEL,
	RESEND_ORDER,
	EXECUTION,
	REJECTED,
	ELIMINATED,
	TRADE_CANCEL,
	MASS_QUOTE,
	QUOTE_CANCEL_INST_LIST,
	QUOTE_CANCEL_INST_GROUP,
	QUOTE_CANCEL_ALL,
	INST_CREATION,
	QUOTE_ACK,
	QUOTE_PRATIAL_ACK,
	QUOTE_REJECT,
	QUOTE_CANCEL_ACK,
	QUOTE_CANCEL_PRACTIAL_ACK,
	QUOTE_CANCEL_REJECT
};

enum customer_type
{
	INDIVIDUAL_TRADING = 1,
	MEMBER_FIRM,
	INDIVIDUAL_MEMBER,
	CUSTOMER_ACCOUNT_OR_OTHER
};

enum give_up_indicator
{
	GU = 1,
	SX,
	TA
};

struct audit_struct
{
	int audit_num;
	char date[16];
	char time_stamp[16];
    exchange_code_type exchange_code;
    message_direction_type direction;
	status_type status;
	char reason[128];
	char senderid[16];
	char account[16];
	char firm_num[16];
	char session_id[16];
	char clordid[16];
	char correlation_clordid[16];
	char orderid[16];
	message_type msg_type;
	int order_side;
	int quantity;
	int maxshow;
	char security_des[32];
	char inst_code[16];
	char maturity_date[16];
	char cfi_code[16];
	double strike_price;
	double limit_price;
	double stop_price;
	double fill_price;
	char order_type;
	tif_type order_qualifier;
	customer_type customer;
	int origin;
	give_up_indicator give_up;
	char give_up_firm[16];
	char give_up_account[16];
};

class audit_trail
{
public:
	audit_trail();
	~audit_trail();

    // ---- data ----
	int globex_audit_id;

    // ---- function ----
    bool startWriteAudit();
    void setAuditCondition(pthread_cond_t* _cond);
	bool add_audit(audit_struct& new_audit);
	bool pop_audit();
	bool print_to_excel(audit_struct& ad);
	void print_title_to_excel();
	bool isEmpty();

private:
	//function
    static void* startAuditThread(void *arg);
    void mainLoop();

	//data
    pthread_t m_startThread;
	int transaction_number;
	audit_struct* audits;
	audit_struct* curr_audit;

	FILE *fp;
	queue<audit_struct> audit_queue;
	pthread_mutex_t audit_mutex;
    pthread_cond_t* m_audit_condition;
};

#endif
