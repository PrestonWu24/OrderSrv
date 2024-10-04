
#ifndef _TRADE_PROCESS_H_
#define _TRADE_PROCESS_H_


#include <pthread.h>
#include <queue>
using namespace std;

#include "client_message.h"
#include "globex_common.h"
#include "exec_globex.h"
#include "read_cme_data.h"

enum receive_data_type
{
  type_logout,
  type_new_order,
  type_alter_order,
  type_cancel_order
};

struct receive_data
{
  int data_type;
  
  string id;
  string account;
  string tag50;

  int local_num;
  int global_num;
  int side;
  int price;
  int size;
  int max_show;
  string symbol;
};

class EtTradeProcess
{
public:
	
	EtTradeProcess();
	~EtTradeProcess();
	
	// ---- function ----
    bool startTradeProcess();
	void mainLoop();
	void setThreadCondition(pthread_cond_t* _cond);
	bool addData(receive_data& _data);

	// ---- data ----
	
private:
	
	// ---- function ----
    static void* startTradeThread(void *arg);

    bool doAcceptedResponse(const char* _cli_order_id, const char* _ex_order_id, string _id);
	bool doFilledResponse(const char* _ex_order_id, int _fill_price, int _fill_size, string _id, const char* _symbol);
    bool doFilledLeg(string _id, int _size, const char* _symbol);
	bool doCancelledResponse(const char* _cli_order_id, string _id);
	bool doAlteredResponse(const char* _cli_order_id, int _size, int _price, const char* _ex_order_id, string _id);
	bool doOrderRejectedResponse(const char* _cli_order_id, string _id);
	bool doSessionReject(int _sent_seq_num, string _id);
    bool doBusinessReject(int _sent_seq_num, string _id);

    bool check_limit();

    bool stopOrderProcess(int _global_order_id, int _local_order_id, string _id);
	void doRecvData();
    bool doCmeLogout();

	void readCmeResponse(cme_response_struct* _response);
	bool popData(receive_data& _data);
	bool isEmpty();

	// --- do message send by client ----
    void doLogin(receive_data& _data);
    void doLogout(receive_data& _data);
    bool doNewOrder(receive_data& _data);
    bool doAlterOrder(receive_data& _data);
    bool doCancelOrder(receive_data& _data);
	// --- do message send by client, end ----
	
	// ---- data ----
    pthread_t m_startThread;
	pthread_cond_t* m_threadCondition;

	queue<receive_data> m_queue;
	pthread_mutex_t m_mutex;
    char dbg[4096];
};

#endif
