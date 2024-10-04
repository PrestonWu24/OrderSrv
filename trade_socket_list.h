
#ifndef _ET_TRADE_SOCKET_LIST_H_
#define _ET_TRADE_SOCKET_LIST_H_

#include <vector>
#include <pthread.h>
using namespace std;

class EtTradeSocketList
{
public:
	
	// ---- function ----
	EtTradeSocketList();
	~EtTradeSocketList();

	bool addSocketToList(int _socket);
	bool deleteSocketFromList(int _socket);
	bool cleanSocketList();
    bool sendDataToServers(const char* _data, int _len);
	bool sendStopTradeToServers();
	bool sendReStartTradeToServers();
    bool sendExitMsg(int _socket);
    bool sendStopTradeToOneServer(int _socket);
	
private:
	
	// ---- function ----
	static void* threadAcceptServerProcess(void *arg);
		
	// ---- data ----
	pthread_mutex_t m_mutex;
	vector<int> m_socketList;
};

#endif
