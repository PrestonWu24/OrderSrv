/**
 * file name : Tracelog.h
 * author    : 
 * description : 
 * last modified date: 2008/03/28
 */

#ifndef _TRACE_LOG_H_
#define _TRACE_LOG_H_


/* log's priority */
#define TRACE_LOG_DEBUG    0
#define TRACE_LOG_WARN     1
#define TRACE_LOG_ERR      2


#ifdef __cplusplus
extern "C" {
#endif

extern void g_traceLogInit(const char* lpszLogFilePath, const char* lpszServiceName, 
	int dwFileSize, int dwLogTms, int bLogInfo);

extern void g_traceLog(unsigned int Priority, const char *sFile, const char *sFunc,
	int Line, const char *sFormat, ...);
	
extern void g_traceLog_print(const char *sFormat, ...);


#define DBG_PRINT(y...) do { g_traceLog_print(y); } while (0)


#define DBG(x,y...) do{ \
 	 g_traceLog(x, __FILE__, __FUNCTION__, __LINE__, y); \
} while (0)


#define DBG_DEBUG(y...) do{ \
 	 g_traceLog(TRACE_LOG_DEBUG, __FILE__, __FUNCTION__, __LINE__, y); \
} while (0)


#define DBG_ERR(y...) do{ \
 	 g_traceLog(TRACE_LOG_ERR, __FILE__, __FUNCTION__, __LINE__, y); \
} while (0)


#define DBG_WARN(y...) do{ \
 	 g_traceLog(TRACE_LOG_WARN, __FILE__, __FUNCTION__, __LINE__, y); \
} while (0)


#ifdef __cplusplus
}
#endif


#endif
