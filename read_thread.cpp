
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "trace_log.h"
#include "read_thread.h"
#include "msg_protocol.h"

EtReadThread::EtReadThread()
{
	m_exitThread = false;
	m_recvSocket = -1;
    m_ptr = NULL;

    memset(m_recvBuff, 0, recv_buffer_size);
	memset(m_leaveBuff, 0, recv_buffer_size);
    memset(m_parseBuff, 0, recv_buffer_size * 2);
    m_recvSize = 0;
	m_leaveLen = 0;
	m_parseLen = 0;

	// set flag
	memset(m_endFlag, 0, 5);
	m_endFlagLen = 0;

    res = 0;
    timeout.tv_sec  = 1; 
    timeout.tv_usec = 0;
}


EtReadThread::~EtReadThread()
{
	if (-1 != m_recvSocket)
	{
		close(m_recvSocket);
		m_recvSocket = -1;
	}
}


void EtReadThread::setRecvSocket(int _socket)
{
	m_recvSocket = _socket;
}


bool EtReadThread::startReadData()
{
	DBG_DEBUG("enter into startReadData");

	if (-1 == m_recvSocket)
	{
		DBG_ERR("the socket is error");
		return false;
	}
	
	if (m_endFlagLen <= 0)
	{
		DBG_ERR("do not set the end flag");
		return false;
	}

	int iRes = pthread_create(&m_recvThread, NULL, readDataThread, this);
	if (iRes != 0)
	{
		DBG_ERR("Fail to run the readDataThread");
		return false;
	}
	
	return true;
}


void EtReadThread::stopReadData()
{
    m_exitThread = true;
    close(m_recvSocket);
    m_recvSocket = -1;
}

bool EtReadThread::isExit()
{
	return m_exitThread;
}


void* EtReadThread::readDataThread(void* arg)
{
	DBG_DEBUG("enter readDataThread");

	EtReadThread* myObj = (EtReadThread*)arg;

	while (true)
	{
		if (myObj->m_exitThread)
		{
		    break;
		}
		
		FD_ZERO(&myObj->fds);
		FD_SET(myObj->m_recvSocket, &myObj->fds);

		myObj->res = select(myObj->m_recvSocket + 1, &myObj->fds, NULL, NULL, &myObj->timeout);
		if (myObj->res < 0)
		{
			DBG_ERR("fail to select when receive data: %s.", strerror(errno));
			break;
		}
		else if (myObj->res == 0)
		{
			continue;
		}
		
		if (FD_ISSET(myObj->m_recvSocket, &myObj->fds))
		{
			memset(myObj->m_recvBuff, 0, recv_buffer_size);
			myObj->m_recvSize = recv(myObj->m_recvSocket, myObj->m_recvBuff, recv_buffer_size, 0);
			if (myObj->m_recvSize <= 0)
			{
				DBG_ERR("fail to read data. %s", strerror(errno));
				break;
			}
			
			memset(myObj->m_parseBuff, 0, recv_buffer_size * 2);
            myObj->m_ptr = NULL;

			DBG_DEBUG("SOCKET: %d, receive:%s", myObj->m_recvSocket, myObj->m_recvBuff);

			if (myObj->m_leaveLen == 0)
			{
				memcpy(myObj->m_parseBuff, myObj->m_recvBuff, myObj->m_recvSize);
				myObj->m_parseLen = myObj->m_recvSize;
			}
			else
			{
              myObj->m_ptr = myObj->m_parseBuff;
              memcpy(myObj->m_ptr, myObj->m_leaveBuff, myObj->m_leaveLen);
              myObj->m_ptr += myObj->m_leaveLen;
              memcpy(myObj->m_ptr, myObj->m_recvBuff, myObj->m_recvSize);

              myObj->m_parseLen = myObj->m_leaveLen + myObj->m_recvSize;
              
              memset(myObj->m_leaveBuff, 0, myObj->recv_buffer_size);
              myObj->m_leaveLen = 0;
			}

			DBG_DEBUG("m_parseBuff: %s", myObj->m_parseBuff);

			if ( !myObj->splitBuffer() )
			{
				DBG_ERR("fail to split buffer");
				break;
			}
		}
	}
	
	myObj->m_exitThread = true;
	if(myObj->m_recvSocket != -1)
	    {
			DBG_DEBUG("close socket: %d", myObj->m_recvSocket);
			close(myObj->m_recvSocket);
			myObj->m_recvSocket = -1;
	    }
	DBG_DEBUG("Leave the readDataThread.");
	return NULL;
}


bool EtReadThread::splitBuffer()
{
	int len = 0;
    int leaveFlag = 0;
       
    char* buffEnd = m_parseBuff + m_parseLen;
    char* beginPtr = m_parseBuff;
    char* endPtr = NULL;

	DBG_DEBUG("Split buffer: %s", beginPtr);

    while (beginPtr < buffEnd)
    {
        endPtr = strstr(beginPtr, m_endFlag);
		if (endPtr == NULL)
	    {
	        leaveFlag = 1;
	        break;
	    }
	    
	    endPtr += m_endFlagLen;
	    len = endPtr - beginPtr;
	    
	    if ( !parseBuffer(beginPtr, len) )
	    {
			DBG_DEBUG("fail to parse buffer.");
	    	return false;
	    }

		beginPtr += len;
	}
	
	if (1 == leaveFlag)
	{
	    int leaveSize = buffEnd - beginPtr;
	    memcpy(m_leaveBuff, beginPtr, leaveSize);
	    m_leaveLen = leaveSize;
		DBG_DEBUG("Leave %d: %s", leaveSize, m_leaveBuff);
	}
	return true;
}

