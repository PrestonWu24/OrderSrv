
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <time.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <map>
#include <vector>

#include "trace_log.h"
#include "util.h"
#include "exec_globex.h"
#include "read_client_data.h"
#include "read_cme_data.h"
#include "read_config.h"
#include "audit_trail.h"
#include "posi_fill.h"
#include "trade_process.h"
#include "msg_protocol.h"
#include "client_message.h"
#include "monitor.h"
#include "trade_socket_list.h"
#include "globex_common.h"

using namespace std;

typedef void Sigfunc(int);

// ---- local ----
pthread_cond_t m_condition;
pthread_cond_t m_auditCondition;
pthread_cond_t m_traceCondition;
// ---- local end ----

// ---- globe variable ----
bool et_g_stopProcess = false;
bool g_need_clean_order = false;
bool g_login = false;
map<string, int> g_socket;
int order_client_num;
int g_posiSocket;
int g_monitor_socket;
int g_cli_num;

exec_globex* et_g_globex = NULL;
order_book* et_g_orderBook = NULL;
EtReadCmeData* et_g_readCme = NULL;
audit_trail* et_g_auditTrail = NULL;
PosiFill* et_g_posiFill = NULL;
EtTradeProcess *et_g_tradeProcess = NULL;
EtMonitor* et_g_monitorPtr = NULL;
EtTradeSocketList* g_socketListPtr = NULL;
EtReadClientData* et_g_readClient = NULL; 
//vector<EtReadClientData*> g_clientptr;
// ---- globe variable, end ----


// local function
void sigHandle(int signo);
Sigfunc *setupSignal(int signo, Sigfunc *func);
void* run_listen_connection(void* arg);
bool checkClient(int _socket); 
bool identifyClient(int _socket, const char* _buffer, int _length);
bool initConnect();
bool doMonitor(int _socket);
// local function, end


void sigHandle(int signo)
{
	switch (signo)
	{
		case SIGTERM:
          {
			  DBG(TRACE_LOG_DEBUG, "Catch SIGTERM.");
			  et_g_stopProcess = true;
			  break;
          }
		case SIGINT:
          {
			  DBG(TRACE_LOG_DEBUG, "Catch SIGINT.");

			  et_g_stopProcess = true;
			  break;
          }
		default:
          {
			  DBG(TRACE_LOG_ERR, "The signal is %d.", signo);
			  break;
          }
	}
}


Sigfunc *setupSignal(int signo, Sigfunc *func)
{
	struct sigaction act, oact;
	act.sa_handler = func;
	sigemptyset(&act.sa_mask);

	if (-1 == sigaddset(&act.sa_mask, SIGTERM))
	{
		DBG_ERR("Failed to add a signal: %s", strerror(errno));
		return SIG_ERR;
	}

	act.sa_flags = 0;

	if (sigaction(signo, &act, &oact) < 0)
	{
		DBG_ERR("Failed to act a signal: %s", strerror(errno));
		return SIG_ERR;
	}
	return (oact.sa_handler);
}


/**
 * listen the client connection.
 */
bool listenConnection(int _port)
{
	int iListenfd = 0;
	int clientSocket = 0;

	struct sockaddr_in clientAddr;
	struct sockaddr_in serverAddr;

	int iResult = 0;

	iListenfd = socket(AF_INET, SOCK_STREAM, 0);
	if (iListenfd < 0)
	{
		DBG(TRACE_LOG_ERR, "Fail to open listen socket.");
		return false;
	}

	memset(&serverAddr, 0, sizeof(serverAddr));
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serverAddr.sin_port = htons(_port);

	iResult = bind(iListenfd, (struct sockaddr *)&serverAddr, sizeof(serverAddr));
	if (iResult < 0)
	{
		DBG_ERR("Failed to bind the server port, %d", _port);
		close(iListenfd);
		return false;
	}

	iResult = listen(iListenfd, 10);
	if (iResult < 0)
	{
		close(iListenfd);
		DBG(TRACE_LOG_ERR, "Fail to listen the server.");
		return false;
	}

	DBG_DEBUG("begin listen on port %d", _port);

	socklen_t cliLen = sizeof(clientAddr);

	fd_set fds;
	struct timeval timeout;	
	timeout.tv_sec  = 2;
	timeout.tv_usec = 0;

	while (true)
	{
		if (et_g_stopProcess)
		{
          break;
		}

		FD_ZERO(&fds);
		FD_SET(iListenfd, &fds);

		iResult = select(iListenfd + 1, &fds, NULL, NULL, &timeout);
		if (iResult < 0)
		{
			DBG_ERR("fail to select when receive price data: %s.", strerror(errno));
			break;
		}
		else if (iResult == 0)
		{
			continue;
		}

		if (FD_ISSET(iListenfd, &fds))
		{
			clientSocket = accept(iListenfd, (struct sockaddr *)&clientAddr, &cliLen);
			if (clientSocket < 0)
			{
			  DBG_ERR("Fail to accept a socket.");
              break;
			}

			DBG_DEBUG("accept a socket: %d", clientSocket);

            if ( !checkClient(clientSocket) ) 
              {
                close(clientSocket); 
				DBG_ERR("fail to start client.");
                break; 
              }
		}
	}

    sleep(5);
	close(iListenfd);
	DBG_ERR("leave listenConnection.");
	return true;
}

bool checkClient(int _socket) 
{ 
  int recvSize = 0; 
  char recvBuff[128]; 
     
  fd_set fds; 
  int res = 0; 
     
  struct timeval timeout;  
  timeout.tv_sec  = 1; 
  timeout.tv_usec = 0; 
  int retry = 0; 
     
  while (true) 
    { 
      if (retry >= 3) 
        { 
			DBG_ERR("do not receive the identified info.");
			return false;
        } 
      FD_ZERO(&fds); 
      FD_SET(_socket, &fds); 
 
      res = select(_socket + 1, &fds, NULL, NULL, &timeout); 
      if (res < 0) 
        {
			DBG_ERR("fail to select when receive data: %s.", strerror(errno));
			return false; 
        } 
      else if (res == 0) 
        { 
          retry++; 
          continue; 
        } 
         
      if (FD_ISSET(_socket, &fds)) 
        { 
          memset(recvBuff, 0, sizeof(recvBuff)); 
          recvSize = recv(_socket, recvBuff, 128, 0); 
          if (recvSize <= 0) 
            { 
				DBG_ERR("fail to read data. %s", strerror(errno));
				return false; 
            } 
          // parse identified info. 
          if ( !identifyClient(_socket, recvBuff, recvSize) ) 
            { 
				DBG_ERR("fail to identify client.");
				return false; 
            } 
          break; 
        } 
    } 
     
  return true; 
}

bool identifyClient(int _socket, const char* _buffer, int _length) 
{ 
	DBG_DEBUG("identifyClient. buffer: %s", _buffer);
  if (_buffer == NULL || _length == 0) 
    { 
		DBG_ERR("parameter is error.");
		return false; 
    } 
     
  EtClientMessage ecm; 
  if ( !ecm.parseMessage(_buffer, _length) ) 
    { 
		DBG_ERR("fail to parse message.");
		return false; 
    } 
     
  int msgType = 0; 
  if ( !ecm.getFieldValue(fid_msg_type, msgType) ) 
    { 
		DBG_ERR("fail to get fid_msg_type value.");
		return false; 
    } 
     
  if (msgType != ord_msg_identify) 
    { 
		DBG_ERR("msg type is error: %d", msgType);
		return false; 
    }

  string account; 
  if ( !ecm.getFieldValue(ord_fid_account, account))
    {  
		DBG_ERR("fali to get client account value.");
		return false;  
    }

  if(account.compare(0, 7, "Monitor") == 0){
    g_monitor_socket = _socket;
    doMonitor(_socket);
  }
  else{
    string clientTag50;
    if ( !ecm.getFieldValue(ord_fid_tag50, clientTag50) ) 
      { 
		  DBG_ERR("fali to get client tag 50 value.");
		  return false; 
      }
    
    string id = account + clientTag50;

    g_socket.insert(pair<string, int>(id, _socket));
    
    g_cli_num++;

    int iaccount = EtReadConfig::getAccountId(account);
	DBG_DEBUG("iaccount= %d", iaccount);

	int isubid = EtUtil::strToInt(clientTag50.substr(1, 1)) - 1;
	DBG_DEBUG("isubid=%d", isubid);

    et_g_readClient[iaccount * SUBID_NUM + isubid].setThreadCondition(&m_condition);
    et_g_readClient[iaccount * SUBID_NUM + isubid].setRecvSocket(_socket);
    et_g_readClient[iaccount * SUBID_NUM + isubid].setId(id);
	et_g_readClient[iaccount * SUBID_NUM + isubid].m_exitThread = false;
    if(!et_g_readClient[iaccount * SUBID_NUM + isubid].startReadData()){  
		DBG(TRACE_LOG_ERR, "Fail to run the thread read client data.");
      return false;  
    }
    
    g_socketListPtr->addSocketToList(_socket);

	if(!g_login){
		// start the thread receiving response information.   
		if ( !et_g_readCme->startReadCme() )   
			{   
				DBG_ERR("fail to start thread receiving data from order server");
				et_g_stopProcess = true;   
				return false; 
			}

		sleep(1);
		// login order server  
		if ( !et_g_globex->loginOrderServer() )  
			{  
				DBG_ERR("fail to login order server");
				et_g_stopProcess = true;  
				return false; 
			}  
		else  
			{  
				DBG_DEBUG("success to login order server.");
			}
		g_login = true;
	} 
  }

  return true; 
} 

bool doMonitor(int _socket)
{
	DBG_DEBUG("enter doMonitor");

  // start read data from calendarSrv  
  et_g_monitorPtr = new EtMonitor;
  et_g_monitorPtr->setRecvSocket(_socket);  
  if ( !et_g_monitorPtr->startReadData() )  
    {  
		DBG_ERR("fail to start Monitor");
		return false;  
    }  

  if ( !et_g_monitorPtr->startSendQuote() )
    {
		DBG_ERR("fail to start send quote");
		return false;   
    }   

  return true;
}

bool initConnect()
{
  // connect to CME order server 
  int orderSocket = et_g_globex->connectOrderServer();  
  if (-1 == orderSocket)  
    {  
		DBG_ERR("fail to connect order server.");
		et_g_stopProcess = true;  
		return false;
    }

  // start the thread receiving response information.   
  et_g_readCme->setRecvSocket(orderSocket);   

  // connect to position server
  int iRes = 0; 
  iRes = EtUtil::connect_client(EtReadConfig::m_positionSrvIp.c_str(),  
                                EtReadConfig::m_positionSrvPort); 
  if (iRes == -1) 
    { 
		DBG_ERR("Fail to connect to host: %s, port: %d", EtReadConfig::m_positionSrvIp.c_str(),  
				EtReadConfig::m_positionSrvPort);
		return false; 
    } 
  else 
    { 
		DBG_DEBUG("connect to position server, host: %s, port: %d", EtReadConfig::m_positionSrvIp.c_str(),  
				  EtReadConfig::m_positionSrvPort);
    } 
 
  g_posiSocket = iRes; 

  DBG_DEBUG("SOCKET, position: %d", g_posiSocket);
   
  // send identify message 
  EtClientMessage csMsg; 
  csMsg.putField(fid_msg_type, msg_posi_identify); 
  char str[20] = ""; 
  strcat(str, "OrderSrv"); 
  csMsg.putField(msg_posi_identify, str); 
 
  if ( !csMsg.sendMessage(g_posiSocket) ) 
    { 
		DBG(TRACE_LOG_ERR, "Failed to send position message.");
		return false; 
    } 

  usleep(100); 

  return true;
}

/**
 * main process.
 */
int main(int argc, char *argv[])
{
  g_cli_num = 0;
  int iRes = 0;
	int iTmp = 0;
    order_client_num = 0;

	char *cPtr = NULL;
	char cCurPath[128];

	// get current path
	memset(cCurPath, 0, 128);
	cPtr = strrchr(argv[0], '/');
	if (cPtr == NULL)
	{
		printf("The process name is error.\n");
		exit(-1);
	}
	iTmp = cPtr - argv[0];
	memcpy(cCurPath, argv[0], iTmp);

	// avoid start again
	char cLockFileName[128];
	memset(cLockFileName, 0, 128);
	strcpy(cLockFileName, cCurPath);
	strcat(cLockFileName, "/lockfile");
	int iLockFd = 0;
	if ( !EtUtil::checkLock(&iLockFd, cLockFileName) )
	{
		printf("The process: %s maybe has been running.", argv[0]);
		exit(-1);
	}

	// init log 
    g_traceLogInit(cCurPath, "order", 0, 0, 1); 
    DBG(TRACE_LOG_DEBUG, "start process.");

	// read config
	if ( !EtReadConfig::loadConfigFile("./config") )
	{
		DBG_ERR("fail to read config file.");
		exit(-1);
	}

	// setup SIGTERM signal
	if (SIG_ERR == setupSignal(SIGTERM, sigHandle))
	{
		DBG(TRACE_LOG_ERR, "Fail to setup signal.");
		EtUtil::releaseLock(iLockFd);
		exit(-1);
	}

	if (SIG_ERR == setupSignal(SIGINT, sigHandle))
	{
		DBG(TRACE_LOG_ERR, "Fail to setup signal SIGINT.");
		EtUtil::releaseLock(iLockFd);
		exit(-1);
	}

    //init thread condition 
    iRes = pthread_cond_init(&m_condition, NULL); 
    if (iRes != 0) 
      { 
		  DBG_ERR("Fail to init the m_condition. %s.", strerror(iRes));
		  EtUtil::releaseLock(iLockFd); 
		  exit(-1); 
      }

    iRes = pthread_cond_init(&m_auditCondition, NULL);  
    if (iRes != 0)  
      {  
		  DBG_ERR("Fail to init the m_condition. %s.", strerror(iRes));
		  EtUtil::releaseLock(iLockFd);
		  exit(-1);  
      }

    iRes = pthread_cond_init(&m_traceCondition, NULL); 
    if (iRes != 0)   
      {   
		  DBG_ERR("Fail to init the m_traceCondition: %s", strerror(iRes));

		  EtUtil::releaseLock(iLockFd); 
		  exit(-1);   
      }

    et_g_posiFill = new PosiFill;
    et_g_auditTrail = new audit_trail;
    et_g_auditTrail->setAuditCondition(&m_auditCondition);
    et_g_auditTrail->startWriteAudit();

    g_socketListPtr = new EtTradeSocketList;
    et_g_orderBook = new order_book;

    et_g_readCme = new EtReadCmeData;   
    et_g_readCme->setThreadCondition(&m_condition);
    et_g_readCme->setAuditCondition(&m_auditCondition);

    et_g_tradeProcess = new EtTradeProcess;  
    et_g_tradeProcess->setThreadCondition(&m_condition);

    et_g_tradeProcess->startTradeProcess();

    et_g_globex = new exec_globex;
    et_g_globex->setAuditCondition(&m_auditCondition);

    et_g_readClient = new EtReadClientData[ACCOUNT_NUM * SUBID_NUM];

    sleep(3);

    if(!initConnect()){
		DBG(TRACE_LOG_DEBUG, "Fail at Initial Connection.");
		exit(-1);
    }

	// listen client connection
	if ( !listenConnection(EtReadConfig::m_listenPort) )
	{
		DBG_ERR("Fail to listen a client connection.");
		EtUtil::releaseLock(iLockFd);
		exit(-1);
	}

    sleep(5);

    // final clean up
    if(et_g_globex != NULL){
      delete et_g_globex;
	  DBG_DEBUG("deleted globex ptr.");
    }

    if(et_g_readCme != NULL){
      delete et_g_readCme;
	  DBG_DEBUG("deleted readCme ptr");
    }

    if(et_g_orderBook != NULL){
      delete et_g_orderBook;
	  DBG_DEBUG("deleted orderbook ptr.");
    }

    if(et_g_tradeProcess != NULL){ 
      delete et_g_tradeProcess; 
	  DBG_DEBUG("deleted tradeProcess ptr");
    }

    if( et_g_posiFill != NULL) 
      { 
        delete et_g_posiFill; 
		DBG_DEBUG("delete dropcopy ptr.");
      } 

    if(et_g_auditTrail != NULL){
      delete et_g_auditTrail;
	  DBG_DEBUG("deleted auditTrail ptr");
    }

	DBG(TRACE_LOG_DEBUG, "Leave main.");
	exit(0);
}


