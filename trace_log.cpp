/**
 * file name: trace_log.cpp
 * author: none
 * description: write log to file.
 * last modified date: 2008/03/28
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <time.h>
#include <errno.h>

#include "trace_log.h"


#define TRACELOG_MAX_PATH       512
#define TRACELOG_MAX_LINE       4096

#define TRACELOG_MAX_FILESIZE   0x6400000    // 100MB
#define TRACELOG_MIN_FILESIZE   4096

#define TRACELOG_MAX_LOGTMS     10
#define TRACELOG_MIN_LOGTMS     0

#define PROC_NAME_LENGTH        128


extern int errno;

// local variable
// 0: not output, 1: output.
int m_debugFlag = 0;

char       m_LogFilePath[TRACELOG_MAX_PATH + 1];         // log file path
int        m_uFileSize = 0x400000;                       // 4MB
int        m_uLogTms = 5;                                // 5 bak file
char       m_sProcName[PROC_NAME_LENGTH + 1];            // process name
char       m_sFileName[TRACELOG_MAX_PATH + 1];           // log file

static pthread_mutex_t mut = PTHREAD_MUTEX_INITIALIZER;
// local variable, end


/* declear function */
static int logSetBakFileName(char *cFileName,char *inPth,unsigned int iFileNo);
static int logStandbyFile(char *inPth, unsigned int inTms);
static FILE* logOpenFile(unsigned long dwBufSize);
//static void logGetProcessName();
/* declear function end */


/**
 * init tracelog.
 */
void g_traceLogInit(
	const char* lpszLogFilePath,        // log file's path
	const char* lpszServiceName,        // the process name
	int dwFileSize,                     // log file's size
	int dwLogTms,                       // size of bak files
	int bLogInfo                        // debug log
	)
{
	int iLenPath = 0;
    struct stat stat_buf;
	int iPathMaxLength = 0;
	int fSize = 0;

    // file size
	fSize = dwFileSize * 1024;
	if (fSize <= TRACELOG_MAX_FILESIZE && fSize >= TRACELOG_MIN_FILESIZE)
	{
		m_uFileSize = fSize;
	}
	
	// bak file
	if (dwLogTms <= TRACELOG_MAX_LOGTMS && dwLogTms > 1)
	{
		m_uLogTms = dwLogTms;
	}

	// process name
	memset(m_sProcName, 0, sizeof(m_sProcName));
	if (lpszServiceName != NULL && strlen(lpszServiceName) != 0)
	{
		strncpy(m_sProcName, lpszServiceName, sizeof(m_sProcName));
	}
	else
	{
		strcpy(m_sProcName, "trace_log");
	}
	
	// path
	memset(m_LogFilePath, 0, sizeof(m_LogFilePath));
	if (lpszLogFilePath != NULL && strlen(lpszLogFilePath) != 0)
	{
		iLenPath = strlen(lpszLogFilePath);
	
		// 10byte for character: '.'; '/';"log" and so on
		iPathMaxLength = TRACELOG_MAX_PATH - strlen(m_sProcName) - 10;

		if ( (stat(lpszLogFilePath, &stat_buf) != -1) && 
			(iLenPath <= iPathMaxLength) )
		{
			if ( (stat_buf.st_mode & S_IFDIR) != 0)
			{
			    sprintf(m_LogFilePath, "%s", lpszLogFilePath);
            }
		}
	}
	else
	{
		strcpy(m_LogFilePath, "/tmp/tracelog.log");
	}
	
	// log file
	memset(m_sFileName, 0, TRACELOG_MAX_PATH + 1);
	snprintf(m_sFileName, TRACELOG_MAX_PATH, "%s/%s.log", m_LogFilePath, m_sProcName);

	// output the debug log or not.
	if (bLogInfo == 1)
	{
		m_debugFlag = 1;
	}
}


/**
 * write the log.
 */
void g_traceLog(
    unsigned int Priority,    // priority
	const char *sFile,        // source file name
	const char *sFunc,        // function name
    int Line,                 // line number
    const char *sFormat, ...        // content
    )
{
	va_list ap;
	int errcode = 0;
	int iRetryCount = 2;
	int len = 0;
	int iFlag = 0;
	
	char buf[TRACELOG_MAX_LINE + 1];
	char *cPtr = NULL;
	const char *pformat = sFormat;
    time_t timeNow;
	struct tm* structTime;
	FILE* fp = NULL;
    struct timeval tv;

	/* none of content */
	if ((pformat == NULL) || (strlen(pformat) <= 0 )) {
		return;
	}
	
	if (m_debugFlag == 0 && Priority == TRACE_LOG_DEBUG) {
	    return;
	}

	memset(buf, 0, TRACELOG_MAX_LINE + 1);

	// current time
	timeNow = time(NULL);
	structTime = localtime(&timeNow);
    gettimeofday(&tv, NULL);

    /*
	len += snprintf(buf + len, TRACELOG_MAX_LINE - len ,"[%04d-%02d-%02d %02d:%02d:%02d]",
		(structTime->tm_year + 1900), (structTime->tm_mon + 1), (structTime->tm_mday), structTime->tm_hour, 
		structTime->tm_min, structTime->tm_sec);
    */

    len += snprintf(buf + len, TRACELOG_MAX_LINE - len ,"[%02d-%02d %02d:%02d:%02d.%3d]",  
                    (structTime->tm_mon + 1), (structTime->tm_mday), structTime->tm_hour,   
                    structTime->tm_min, structTime->tm_sec, (int)(tv.tv_usec / 1000));

	// the process name
	len += snprintf(buf + len, TRACELOG_MAX_LINE - len, "[%s]", m_sProcName);
		
	// log priorty
	if (Priority == TRACE_LOG_DEBUG)
	{
		len += snprintf(buf + len, TRACELOG_MAX_LINE - len, "[%s]", "DEBUG");
	}
	else if (Priority == TRACE_LOG_WARN)
	{
	    len += snprintf(buf + len, TRACELOG_MAX_LINE - len, "[%s]", "WARN");
	}
	else
	{
	    len += snprintf(buf + len, TRACELOG_MAX_LINE - len, "[%s]", "ERR");
	}

	// file name
	if ((sFile != NULL) && (strlen(sFile) > 0))
	{
		len += snprintf(buf + len,TRACELOG_MAX_LINE - len,"[%s",sFile);
		iFlag = 1;
	}
	
	// function name
	if ((sFunc != NULL) && ( strlen(sFunc) > 0))
	{
		if (!iFlag)
		{
			len += snprintf(buf + len,TRACELOG_MAX_LINE - len,"[");
		}
		len += snprintf(buf + len,TRACELOG_MAX_LINE - len,":%s()", sFunc);
		iFlag = 1;
	}
	if (iFlag)
	{
		len += snprintf(buf + len,TRACELOG_MAX_LINE - len, "]");
	}
	
	// line number
	if (Line >= 0)
	{
		len += snprintf(buf + len,TRACELOG_MAX_LINE - len,"[line: %d]", Line);
	}
	
	// content
	va_start(ap, sFormat);
	cPtr = buf + len;
	len += vsnprintf(cPtr, TRACELOG_MAX_LINE - len, pformat, ap); 
	va_end(ap);
	cPtr = buf + len;
	*cPtr = '\n';
	
	// write
	while (iRetryCount > 0)
	{
		errcode = pthread_mutex_trylock(&mut);
		// trylock failed
		if (errcode != 0)
		{
			if (errcode == EBUSY)
			{
				iRetryCount-- ;
				if (iRetryCount > 0)
				{
					// 0.1s
					usleep(100 * 1000);
				}
				continue;
			}
			else
			{
				// other failed, do not retry.
				iRetryCount = 0;
				break;
			}
		}
		else
		{
			// try lock success
			fp = logOpenFile(strlen(buf));
			if (NULL == fp)
			{
				pthread_mutex_unlock(&mut);
				return;
			}
			fputs(buf, fp);
			fclose(fp);
			fp = NULL;
			pthread_mutex_unlock(&mut);
			break;
		} // end else
	} // end while iRetryCount
}


void g_traceLog_print(
    const char *sFormat, ...        // content
    )
{
	va_list ap;
	int errcode = 0;
	int iRetryCount = 2;
	int len = 0;
	
	char buf[TRACELOG_MAX_LINE + 1];
	char *cPtr = NULL;
	const char *pformat = sFormat;
	FILE* fp = NULL;

	/* none of content */
	if ((pformat == NULL) || (strlen(pformat) <= 0 )) {
		return;
	}
	
	memset(buf, 0, TRACELOG_MAX_LINE + 1);
	
	// content
	va_start(ap, sFormat);
	cPtr = buf + len;
	len += vsnprintf(cPtr, TRACELOG_MAX_LINE - len, pformat, ap); 
	va_end(ap);
	cPtr = buf + len;
	
	// write
	while (iRetryCount > 0)
	{
		errcode = pthread_mutex_trylock(&mut);
		// trylock failed
		if (errcode != 0)
		{
			if (errcode == EBUSY)
			{
				iRetryCount-- ;
				if (iRetryCount > 0)
				{
					// 0.1s
					usleep(100 * 1000);
				}
				continue;
			}
			else
			{
				// other failed, do not retry.
				iRetryCount = 0;
				break;
			}
		}
		else
		{
			// try lock success
			fp = logOpenFile(strlen(buf));
			if (NULL == fp)
			{
				pthread_mutex_unlock(&mut);
				return;
			}
			fputs(buf, fp);
			fclose(fp);
			fp = NULL;
			pthread_mutex_unlock(&mut);
			break;
		} // end else
	} // end while iRetryCount
}



/**
 * open log's file.
 * parameter:
 *    dwBufSize: the size of buffer what will be wrote to file.
 */
static FILE* logOpenFile(unsigned long dwBufSize)
{
	struct stat stat_buf;
	FILE* fp = NULL;

	if ((stat(m_sFileName, &stat_buf)) == -1)
	{
		if (errno == ENOENT)
		{
		    // file not find.
			fp = fopen(m_sFileName, "w+");
			return fp;
		}
		else
		{
			return NULL;
		}
	}
	
	// the file size maybe overflow.
	if (stat_buf.st_size + dwBufSize > m_uFileSize)
	{
		logStandbyFile(m_sFileName, m_uLogTms);
		fp = fopen(m_sFileName, "w+");
		return fp;
	}
	
	// all right
	fp = fopen(m_sFileName, "a+");
	return fp;
}


/**
 * standby the log file to bak file.
 */
static int logStandbyFile(char *inPth, unsigned int inTms) 
{
	int  wCnt;				                    // Count
	char wPthBfr[TRACELOG_MAX_PATH + 1];		// Path Before
	char wPthAft[TRACELOG_MAX_PATH + 1];		// Path After

	struct stat stat_buf;
	
	if (inPth == NULL || strlen(inPth) == 0)
	{
		return -1;
	}
	
	for (wCnt = inTms; wCnt > 0; wCnt--)
	{
		int flag = -1;
		logSetBakFileName(wPthBfr, inPth, (wCnt - 1));
		logSetBakFileName(wPthAft, inPth, wCnt);
		// delete the next file.
		remove(wPthAft);
		flag = stat(wPthBfr, &stat_buf);
		if (flag == 0)
		{
			rename(wPthBfr, wPthAft);
		}
	}
	// delete the primary log file
	remove(inPth);
	return 0;
}


/**
 * get the process's name.
 * if can not find the process name, raplace with pid. 
 */
/*
static void logGetProcessName()
{
	char exePath[64];
	char procPath[256];
	char* ch = NULL;
	int ret = 0;
	
	// if the m_sProcName has been set when TraceLogInit invokes
	if (m_sProcName != NULL && strlen(m_sProcName) != 0)
	{
		return;
	}
	
	memset(exePath, 0, sizeof(exePath));
	memset(procPath, 0, sizeof(procPath));
	sprintf(exePath, "/proc/%d/exe", getpid());
	
	ret = readlink(exePath, procPath, sizeof(procPath));
	if (ret > 0)
	{
		ch = strrchr(procPath, '/');
		if (ch == NULL)
		{
			// not find '/' in the path
			snprintf(m_sProcName, sizeof(m_sProcName), "%s", procPath);
			return;
		}
		else
		{
			ch++;
			if (ch != NULL && strlen(ch) > 0)
			{
				//get the proc name after the last '/'
				snprintf(m_sProcName, sizeof(m_sProcName), "%s", ch);
				return;
			}
		}
	}
	snprintf(m_sProcName, sizeof(m_sProcName), "pid_%d", getpid());
}
*/


/**
 * get the name of bak file.
 */
static int logSetBakFileName(char *cFileName, char *inPth, unsigned int iFileNo)
{
	if (cFileName == NULL || inPth == NULL || strlen(inPth) == 0)
	{
		return -1;
	}
	
	if (iFileNo > 0)
	{
		sprintf(cFileName, "%s.%d", inPth, iFileNo);
	}
	else
	{
		strcpy(cFileName, inPth);
	}
	return 0;
}
