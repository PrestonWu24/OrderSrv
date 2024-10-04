
#ifndef _ET_READ_THREAD_H_
#define _ET_READ_THREAD_H_


#include <pthread.h>

class EtReadThread
{
public:
	
	EtReadThread();
	virtual ~EtReadThread();

	// ---- function ----
	virtual bool startReadData();
	virtual void stopReadData();
	
	void setRecvSocket(int _socket);
	bool isExit();

	// ---- data ----
    int m_recvSocket;
	bool m_exitThread;
	
protected:
	// ---- function ----
	static void* readDataThread(void *arg);
	bool splitBuffer();
	
	virtual bool parseBuffer(const char* _buffer, int _length) = 0;
	
	// ---- data ----
    // do not excess dbg size
	static const int recv_buffer_size = 1024;
	
	pthread_t m_recvThread;
    char* m_ptr;
	
    char m_recvBuff[recv_buffer_size];
    int m_recvSize;

	char m_leaveBuff[recv_buffer_size];
	int m_leaveLen;
	char m_parseBuff[recv_buffer_size * 2];
	int m_parseLen;

	char m_endFlag[5];
	int m_endFlagLen;

    fd_set fds; 
    int res; 
    struct timeval timeout; 
};


#endif
