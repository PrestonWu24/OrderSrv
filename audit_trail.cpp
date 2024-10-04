#include <iostream>
#include <queue>
#include <sys/time.h>
#include <time.h>
#include <string>
#include <string.h>
#include "audit_trail.h"
#include "trace_log.h"
#include "util.h"

extern bool et_g_stopProcess;

/*
static const char* exchange_code_type_array[]=
{
	"",
	"XOCH",
	"XCBT",
	"XCME",
	"XNYM",
	"XCEC",
	"XKBT",
	"XMGE",
	"DUMX",
	"XBMF"
};
*/

static const char* message_direction_type_array[]=
{
	"",
	"TO_CME",
	"FROM_CME",
	"TO_CLIENT",
	"FROM_CLIENT"
};

static const char* status_type_array[]=
{
	"",
	"OK",
	"REJECT"
};

static const char* message_type_array[]=
{
	"",
	"LOGIN",
	"RESET_SEQ_LOGIN",
	"LOGOUT",
	"RESEND_REQUEST",
	"NEW_ORDER",
	"MODIFY",
	"CANCEL",
	"RESEND_ORDER",
	"EXECUTION",
	"REJECTED",
	"ELIMINATED",
	"TRADE_CANCEL",
	"MASS_QUOTE",
	"QUOTE_CANCEL_INST_LIST",
	"QUOTE_CANCEL_INST_GROUP",
	"QUOTE_CANCEL_ALL",
	"INST_CREATION",
	"QUOTE_ACK",
	"QUOTE_PRATIAL_ACK",
	"QUOTE_REJECT",
	"QUOTE_CANCEL_ACK",
	"QUOTE_CANCEL_PRACTIAL_ACK",
	"QUOTE_CANCEL_REJECT"
};

/*
static const char* customer_type_array[]=
{
	"",
	"INDIVIDUAL_TRADING",
	"MEMBER_FIRM",
	"INDIVIDUAL_MEMBER",
	"CUSTOMER_ACCOUNT_OR_OTHER"
};
*/

static const char* give_up_indicator_array[]=
{
	"",
	"GU",
	"SX",
	"TA"
};

static const char* tif_type_array[]=
{
	"DAY",
	"GOOD_TILL_CANCEL",
	"",
	"FILL_OR_KILL",
	"",
	"",
	"GOOD_TILL_DATE"
};

audit_trail::audit_trail()
{
	transaction_number = 0;
	globex_audit_id = 0;
	char file[25];
	strcpy(file, "audit-");
	char time[18];
	memset(time, 0, sizeof(time));

	char date_buffer[9];
	memset(date_buffer, 0, sizeof(date_buffer));
	EtUtil::build_date(date_buffer, sizeof(date_buffer));

	char file_name[20];
	memset(file_name, 0, sizeof(file_name));
	sprintf(file_name, "database/%s_audit.csv", date_buffer);

	fp=fopen(file_name, "r");
	if(!fp)
	    {
          fp=fopen(file_name, "a");
          print_title_to_excel();
	    }
	else
	    {
          fclose(fp);
          fp = fopen(file_name, "a");
	    }
	
    pthread_mutex_init(&audit_mutex, NULL);
}

audit_trail::~audit_trail()
{
	fclose(fp);
    pthread_mutex_destroy(&audit_mutex);
}

void audit_trail::setAuditCondition(pthread_cond_t* _cond)
{
  m_audit_condition = _cond;
}

bool audit_trail::isEmpty() 
{ 
  bool empty = false; 
  pthread_mutex_lock(&audit_mutex); 
  empty = audit_queue.empty(); 
  pthread_mutex_unlock(&audit_mutex); 
  return empty; 
}

bool audit_trail::startWriteAudit() 
{
  int iRes = 0; 
  iRes = pthread_create(&m_startThread, NULL, startAuditThread, this);
  if (iRes != 0)  
    {  
		DBG_ERR("fail to create a thread. %s", strerror(iRes));
		return false;  
    }  
  
  return true; 
}

void* audit_trail::startAuditThread(void *arg) 
{ 
  audit_trail* myReadData = (audit_trail*)arg; 
  myReadData->mainLoop(); 
 
  return NULL; 
}

void audit_trail::mainLoop() 
{ 
  int iRes = 0; 
 
  struct timespec waitTime; 
  struct timeval nowTime; 
  pthread_mutex_t conditionMutex; 
 
  // thread mutex 
  iRes = pthread_mutex_init(&conditionMutex, NULL); 
  if (iRes != 0) 
    { 
		DBG_ERR("Failed to init the mutex of conditionMutex. %s.", strerror(iRes));
		return; 
    } 
 
  while (true) 
    {
      if (et_g_stopProcess)
        { 
          break; 
        } 
 
      gettimeofday(&nowTime, NULL);
      waitTime.tv_sec = nowTime.tv_sec + 30;
      waitTime.tv_nsec = nowTime.tv_usec;
 
      pthread_mutex_lock(&conditionMutex); 
 
      if (isEmpty())
        {
          pthread_cond_timedwait(m_audit_condition, &conditionMutex, &waitTime); 
        } 
      pthread_mutex_unlock(&conditionMutex); 
 
      // get new data 
      while ( !isEmpty() ) 
        {
          pop_audit();
        }
    } // End for 
 
  DBG_DEBUG("Leave Audit mainLoop");
}

bool audit_trail::add_audit(audit_struct& new_audit)
{
  pthread_mutex_lock(&audit_mutex);
  audit_queue.push(new_audit);
  pthread_mutex_unlock(&audit_mutex);
  
  return true;
}


bool audit_trail::pop_audit()
{
  pthread_mutex_lock(&audit_mutex);
	if (audit_queue.empty())
	{
        pthread_mutex_unlock(&audit_mutex);
		return false;
	}
	
	audit_struct new_audit = audit_queue.front();
    audit_queue.pop();
	pthread_mutex_unlock(&audit_mutex);

    print_to_excel(new_audit);
	return true;
}


bool audit_trail::print_to_excel(audit_struct& ad)
{
	if (fp == NULL) {
	  fprintf(stderr, "Can't open input file!\n");
	  exit(1);
	}

	fprintf(fp, "%d,", ad.audit_num);
	fprintf(fp, "%s,", ad.date);
	fprintf(fp, "%s,", ad.time_stamp);

	switch(ad.exchange_code)
	{
		case 1:
			fprintf(fp, "%s,", "XOCH");
			break;
		case 2:
			fprintf(fp, "%s,", "XCBT");
			break;
		case 3:
			fprintf(fp, "%s,", "XCME");
			break;
		case 4:
			fprintf(fp, "%s,", "XNYM");
			break;
		case 5:
			fprintf(fp, "%s,", "XCEC");
			break;
		case 6:
			fprintf(fp, "%s,", "XKBT");
			break;
		case 7:
			fprintf(fp, "%s,", "XMGE");
			break;
		case 8:
			fprintf(fp, "%s,", "DUMX");
			break;
		case 9:
			fprintf(fp, "%s,", "XBMF");
			break;
		default:
			fprintf(fp, "%s,", "");
			break;
	}

	fprintf(fp, "%s,", message_direction_type_array[ad.direction]);
	fprintf(fp, "%s,", status_type_array[ad.status]);

	fprintf(fp, "%s,", ad.reason);
	fprintf(fp, "%s,", ad.senderid);
	fprintf(fp, "%s,", ad.account);
	fprintf(fp, "%s,", ad.firm_num);
	fprintf(fp, "%s,", ad.session_id);
	fprintf(fp, "%s,", ad.clordid);
	fprintf(fp, "%s,", ad.correlation_clordid);
	fprintf(fp, "%s,", ad.orderid);

	fprintf(fp, "%s,", message_type_array[ad.msg_type]);

	if(ad.msg_type >= 5 && ad.msg_type <= 12)
	{
		if(ad.order_side == 0)
		{
			fprintf(fp, "%c,", 'B');
		}
		else if(ad.order_side == 1)
		{
			fprintf(fp, "%c,", 'S');
		}
		else if(ad.order_side == 7)
		{
			fprintf(fp, "%c,", 'C');
		}
		else
		{
			fprintf(fp, "%s,", "");
		}
	}
	else
	{
		fprintf(fp, "%s,", "");
	}

	if(ad.quantity != 0)
	{
		fprintf(fp, "%d,", ad.quantity);
	}
	else
	{
		fprintf(fp, "%s,", "");
	}

	if(ad.maxshow != 0)
	{
		fprintf(fp, "%d,", ad.maxshow);
	}
	else
	{
		fprintf(fp, "%s,", "");
	}

	fprintf(fp, "%s,", ad.security_des);
	fprintf(fp, "%s,", ad.inst_code);
	fprintf(fp, "%s,", ad.maturity_date);
	fprintf(fp, "%s,", ad.cfi_code);
	if(ad.strike_price != 0)
	{	
		fprintf(fp, "%lf,", ad.strike_price);
	}
	else
	{
		fprintf(fp, "%s,", "");
	}
	if(ad.limit_price != 0)
	{	
		fprintf(fp, "%lf,", ad.limit_price);
	}
	else
	{
		fprintf(fp, "%s,", "");
	}
	if(ad.stop_price != 0)
	{
		fprintf(fp, "%lf,", ad.stop_price);
	}
	else
	{
		fprintf(fp, "%s,", "");
	}
	if(ad.fill_price != 0)
	{
		fprintf(fp, "%lf,", ad.fill_price);
	}
	else
	{
		fprintf(fp, "%s,", "");
	}

	if(ad.order_type == '1')
	{
		fprintf(fp, "%s,", "MKT");
	}
	else if(ad.order_type == '2')
	{
		fprintf(fp, "%s,", "LMT");
	}
	else if(ad.order_type == '3')
	{
		fprintf(fp, "%s,", "STOP");
	}
	else if(ad.order_type == '4')
	{
		fprintf(fp, "%s,", "STOP-LMT");
	}
	else if(ad.order_type == 'K')
	{
		fprintf(fp, "%s,", "MKT-LMT");
	}
	else
	{
		fprintf(fp, "%s,", "");
	}

	if(ad.msg_type >= 5 && ad.msg_type <= 12)
	{
		fprintf(fp, "%s,", tif_type_array[ad.order_qualifier]);
	}
	else
	{
		fprintf(fp, "%s,", "");
	}
	if(ad.customer != 0)
	{
		fprintf(fp, "%d,", ad.customer);
	}
	else
	{
		fprintf(fp, "%s,", "");
	}
	if(ad.origin != 0)
	{
		fprintf(fp, "%d,", ad.origin);
	}
	else
	{
		fprintf(fp, "%s,", "");
	}
	fprintf(fp, "%s,", give_up_indicator_array[ad.give_up]);
	fprintf(fp, "%s,", ad.give_up_firm);
	fprintf(fp, "%s\n", ad.give_up_account);
	return true;
}

void audit_trail::print_title_to_excel()
{
	if (fp == NULL) {
	  fprintf(stderr, "Can't open input file!\n");
	  exit(1);
	}

	fprintf(fp, "%s,", "Server Transaction Number");
	fprintf(fp, "%s,", "Server Process Date");
	fprintf(fp, "%s,", "Server Time stamp");

	fprintf(fp, "%s,", "Exchange Code");
	fprintf(fp, "%s,", "Message Direction");
	fprintf(fp, "%s,", "Status");

	fprintf(fp, "%s,", "Reason Code/Error Code");
	fprintf(fp, "%s,", "TAG 50 ID");
	fprintf(fp, "%s,", "Account Number");
	fprintf(fp, "%s,", "Executing Firm");
	fprintf(fp, "%s,", "Session Id");
	fprintf(fp, "%s,", "Client Order ID");
	fprintf(fp, "%s,", "CorrelationClOrdID");
	fprintf(fp, "%s,", "Host Order Number");

	fprintf(fp, "%s,", "Message Type");
	fprintf(fp, "%s,", "Buy/Sell Indicator");

	fprintf(fp, "%s,", "Quantity");
	fprintf(fp, "%s,", "Maxshow");
	fprintf(fp, "%s,", "Instrument/Security Description");
	fprintf(fp, "%s,", "Product/Instrument Code");
	fprintf(fp, "%s,", "Maturity Date/Expiration Date");
	fprintf(fp, "%s,", "CFI Code");
	fprintf(fp, "%s,", "Strike Price");
	fprintf(fp, "%s,", "Limit Price");
	fprintf(fp, "%s,", "Stop Price");
	fprintf(fp, "%s,", "Fill Price");
	fprintf(fp, "%s,", "Order Type");
	fprintf(fp, "%s,", "Order Qualifer");
	fprintf(fp, "%s,", "Customer Type Indicator");
	fprintf(fp, "%s,", "Origin");
	fprintf(fp, "%s,", "Give-up Indicator");
	fprintf(fp, "%s,", "Give-up Firm");
	fprintf(fp, "%s\n", "Give-up Account");
}

