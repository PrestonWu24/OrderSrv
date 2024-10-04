
#ifndef _ET_MONITOR_H_
#define _ET_MONITOR_H_

#include <fstream>
#include <pthread.h>
#include "read_thread.h"
#include "client_message.h"

class EtMonitor : public EtReadThread
{
public:
	// ---- function ----
	EtMonitor();
	~EtMonitor();
    bool startSendQuote();
	
    // ---- function ----
        
    // ---- data -----
    
private:
	// ---- function ----
    static void* sendQuoteThread(void *arg); 
	bool parseBuffer(const char* _buffer, int _length);
    bool doFillLimit(EtClientMessage& _csMsg);
    bool doLogout();
    bool doStopTrade(EtClientMessage& _csMsg);

    // ---- data ----
    pthread_t m_quoteThread;
};


#endif
