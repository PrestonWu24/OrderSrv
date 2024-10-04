
#ifndef _ET_READCLIENT_H_
#define _ET_READCLIENT_H_

#include <fstream>
#include "read_thread.h"
#include "client_message.h"

class EtReadClientData : public EtReadThread
{
public:
	// ---- function ----
	EtReadClientData();
	~EtReadClientData();
	
    // ---- function ----
    void setThreadCondition(pthread_cond_t* _cond);
    void setId(std::string _id);
        
    // ---- data -----
    string m_id;

private:
	// ---- function ----
	bool parseBuffer(const char* _buffer, int _length);
    bool loadInstrument(EtClientMessage& _csMsg);
    bool doClientLogout(EtClientMessage& _csMsg);
    bool doNewOrder(EtClientMessage& _csMsg);
    bool doAlterOrder(EtClientMessage& _csMsg);
    bool doCancelOrder(EtClientMessage& _csMsg);

    // ---- data ----
    pthread_cond_t* m_threadCondition;
        
    std::string m_product_name;
    bool m_stop;
};


#endif
