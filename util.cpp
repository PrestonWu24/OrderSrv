
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/tcp.h>

#include "trace_log.h"
#include "util.h"

extern int g_monitor_socket;

int EtUtil::connect_client(const char *passed_ip, int passed_port)
{
	int orig_sock = 0;
	struct sockaddr_in server_address;

	int curr_buff_size = 0;
	unsigned int curr_buff_size_len = 0;
	
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_port	= htons(passed_port);
	if (inet_aton(passed_ip, &server_address.sin_addr) == 0)
	{   
		DBG_ERR("fail to get a IP address, %s.", passed_ip);
		return -1;
	}

	if ((orig_sock = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
		DBG_ERR("fail to create a socket. error number: %d", errno);
		return -1;
    }

	if (connect(orig_sock, (struct sockaddr*)&server_address, sizeof(server_address)) == -1) 
    {
		DBG_ERR("fail to connect to server. error number: %d", errno);
		close(orig_sock);
		return -1;
    }

	curr_buff_size_len = sizeof(curr_buff_size);
	getsockopt(orig_sock, SOL_SOCKET, SO_RCVBUF, (char*) &curr_buff_size, &curr_buff_size_len);

	DBG_DEBUG("READ_BUFFER_SIZE: %d", curr_buff_size);

	return (orig_sock);
}


bool EtUtil::set_non_block(int des)
{
	if (-1 == fcntl(des, F_SETFL, O_NONBLOCK))
    {
		DBG_ERR("fail to set no block, error number: %d (%s)", errno, strerror(des));
		return false;
    }
	return true;
}

bool EtUtil::build_time_buffer(char* _buffer, int _len)
{
	struct timeval tv;
	struct tm local_time;
	time_t second;

	char time_format[] = {
		'%', 'Y',
		'%', 'm',
		'%', 'd', '-',
		'%', 'H', ':',
		'%', 'M', ':',
		'%', 'S', 0 };

	gettimeofday(&tv, NULL);
	
    second = tv.tv_sec;
	
	memcpy(&local_time, gmtime(&second), sizeof(local_time));
	strftime(_buffer, _len, time_format, &local_time);	
	
	char tmp[10];
	memset(tmp, 0, 10);
	sprintf(tmp, ".%03d", (int)(tv.tv_usec / 1000));
	strcat(_buffer, tmp);

	return true;
}

bool EtUtil::sendData(const int _iSockFd, const char *_buffer, const int _iLength)
{
	if (_iSockFd < 0 || _buffer == NULL || _iLength < 0)
	{
		DBG(TRACE_LOG_ERR, "The parameter is error, socket=%d, len=%d", _iSockFd, _iLength);
		return false;
	}
	
	const char *cPtr = _buffer;
	int iTotalLen = 0;
	int iSendLen = 0;
	
	while (iTotalLen < _iLength)
	{
		iSendLen = send(_iSockFd, cPtr, _iLength - iTotalLen, 0);
		if (iSendLen < 0)
		{
			DBG(TRACE_LOG_ERR, "Failed to send data to server. %s", strerror(errno));
			return false;
		}

        if(_iSockFd != g_monitor_socket){
			DBG_DEBUG("sent data to socket %d, %s", _iSockFd, cPtr);
        }
		iTotalLen += iSendLen;
		cPtr += iSendLen;
	}
	return true;
}


/** 
 * check Lock function 
 *    true: ok
 *   false: error
 */
bool EtUtil::checkLock(int* fileid, char* _fileName)
{
	if ((*fileid = open(_fileName, O_WRONLY)) == -1)
	{
		printf("Failed to open the lock file: %s. error: %s\n", 
			   _fileName, strerror(errno));
		return false;
	}

	if (lockf(*fileid, F_TLOCK, 0) == -1)
	{
		printf("Failed to lock the file: %s, eror: %s\n", _fileName, 
			   strerror(errno));
		if (close(*fileid) == -1)
		{
			printf("Failed to close the lock file: %s. error: %s\n", 
				   _fileName, strerror(errno));
		}		
		return false;
	}
	else
	{
		return true;
	}
}


bool EtUtil::releaseLock(int fileid)
{    
	if (lockf(fileid, F_ULOCK , 0) == -1)
	{
		DBG(TRACE_LOG_ERR, "Fail to unlock the lock file.");
		return false;
	}

	if (close(fileid) == -1) 
	{
		DBG(TRACE_LOG_ERR, "Fail to close the lock file.");
		return false;
	}
	return true;
}


int EtUtil::doubleToInt(double _dVal, int _dot_count)
{
	int inum = 0;
	char strNum[32];
	memset(strNum, 0, 32);

	sprintf(strNum, "%lf", _dVal * _dot_count);
	inum  = atol(strNum);
	return inum;
}


/**
 * CL: 100
 * HO, RB: 10000
 */
double EtUtil::intToDouble(int _inum, int _dot_count)
{
	double dnum = 0.0;
	dnum = (double)_inum / _dot_count;
	return dnum;
}


/**
 * int to string.
 */
string EtUtil::intToStr(int _val)
{
    stringstream ss;
    ss << _val;
    return ss.str();
}


/**
 * string to int.
 */
int EtUtil::strToInt(string _str) 
{
	int iVal = 0;
	stringstream ss(_str);
	ss >> iVal;
	return iVal;
}


/**
 * double to string.
 */
string EtUtil::doubleToStr(double _val)
{
    stringstream ss;
    ss << _val;
    return ss.str();
}


/**
 * string to double.
 */
double EtUtil::strToDouble(string _str)
{
	double dVal;
	stringstream ss(_str);
	ss >> dVal;
	return dVal;
}


/**
 * trim the space.
 */
void EtUtil::trimSpace(char *_str)
{
	long iFirst = 0;
	long iLast = 0;
	long iLen = 0;
	long i;
	
	iLen = strlen(_str);
	if (NULL == _str || 0L == iLen) { 
		return;
	}

	while (isspace(_str[iFirst])) { 
		iFirst++;
	}
	if (iFirst == iLen) {
		_str[0] = '\0';
		return;
	}

	iLast = iLen - 1;
	while (isspace(_str[iLast])) { 
		iLast--;
	}
	
	if (0 == iFirst && (iLen - 1) == iLast) { 
		return;
	}

	for (i = iFirst; i <= iLast; i++) {
		_str[i-iFirst] = _str[i];
	}
	_str[iLast-iFirst+1] = '\0';
	return;
}

bool EtUtil::getDate(char* _buffer, int _len)  
{ 
  time_t timeNow;  
  struct tm* structTime;  
  struct timeval tv;  
 
  timeNow = time(NULL);  
  structTime = localtime(&timeNow);  
  gettimeofday(&tv, NULL); 
 
  memset(_buffer, 0, sizeof(_buffer)); 
  sprintf(_buffer, "%02d/%02d/%04d", structTime->tm_mon+1, structTime->tm_mday, structTime->tm_year + 1900); 

  return true;
}

bool EtUtil::getTime(char*& curr_time, int len) 
{ 
  time_t timeNow; 
  struct tm* structTime; 
  struct timeval tv; 

  timeNow = time(NULL); 
  structTime = localtime(&timeNow); 
  gettimeofday(&tv, NULL);

  memset(curr_time, 0, sizeof(curr_time));
  sprintf(curr_time, "%02d:%02d:%02d.%3d", structTime->tm_hour, structTime->tm_min, structTime->tm_sec, (int)(tv.tv_usec / 1000));

  return true;
} 

bool EtUtil::build_date(char* _buffer, int _len)
{
    time_t sec;
    sec = time(NULL);

    struct tm* timeinfo;
    timeinfo = localtime(&sec);
    strftime(_buffer, _len, "%Y%m%d", timeinfo);

    return true;
}

bool EtUtil::build_weekday(char* _buffer, int _len)
{
    time_t sec;
    sec = time(NULL);

    struct tm* timeinfo;
    timeinfo = localtime(&sec);
    strftime(_buffer, _len, "%w", timeinfo);

    return true;
}

int EtUtil::diff_time(char* _buffer)
{
    struct tm* currtime;
    char temp[5];

    time_t sec;
    sec = time(NULL);
    currtime = localtime(&sec);

    memset(temp, 0, sizeof(temp));
    strncpy(temp, _buffer, 4);
    int year = atoi(temp);

    memset(temp, 0, sizeof(temp));
    strncpy(temp, _buffer+4, 2);
    int month = atoi(temp);

    memset(temp, 0, sizeof(temp));
    strncpy(temp, _buffer+6, 2);
    int day = atoi(temp);

    int difference = -1;

    // do not consider multiple months and years                                                                                                                                                              
    if(year == currtime->tm_year + 1900)
        {
            if(month == currtime->tm_mon + 1)
                {
                    difference = currtime->tm_mday - day;
                }
            else
                {
                    int month_date = get_month_date(month, year);
                    difference = currtime->tm_mday + (month_date - day);
                }
        }
    else
        {
            difference = currtime->tm_mday + (31 - day);
        }

    return difference;
}

int EtUtil::get_month_date(int _month, int _year)
{
    if(_month < 1 || _month > 12)
        {
			DBG_ERR("month error!");
			return -1;
        }
    if(_month % 2 == 1)
        {
            return 31;
        }
    else if(_month == 2)
        {
            if(is_leapyear(_year))
                {
                    return 29;
                }
            else
                {
                    return 28;
                }
        }
    else
        {
            return 30;
        }
}

bool EtUtil::is_leapyear(int _year)
{
    bool leapyear = false;

    if (_year % 4 == 0) {
        if (_year % 100 == 0) {
            if (_year % 400 == 0)
                leapyear = true;
            else leapyear = false;
        }
        else leapyear = true;
    }
    else leapyear = false;

    return leapyear;
}
