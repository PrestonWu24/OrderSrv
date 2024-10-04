  
#ifndef _GLOBEX_COMMON_H_
#define _GLOBEX_COMMON_H_
#include <string>
using namespace std;

#define SOH							    0x01

#define ACCOUNT_NUM                     10
#define SUBID_NUM                       2
#define QUOTE_INTERVAL                  30
#define ORDER_BUFFER_SIZE               40960
#define	GLOBEX_HEART_BEAT_INTERVAL		"30"
#define ADD_LIMIT 1
#define ADD_PANDL_LIMIT 500

#define SESSION_ID                      "W7P"
#define FIRM_ID                         "5S1"

#define ORDER_PASSWORD                  "altic"

#define GLOBEX_ACCT                     "16097"
#define EXPIRE_DATE					    "20101023"

#define	ORDER_DELAY					    2.00
#define	MAX_STRING_ORDER_ID			    32

#define ELEMENT_NUMBER    12
#define EXCHANGE_CODE                   "XNYM"

typedef enum
{
	NOT_LOGGED_IN = 0, 
	SENT_LOGIN,
	LOGIN_SUCCESS,
	SENT_LOGOUT
} login_state_type;

typedef enum
    {
        LOGIN_SENT,
        LOGOUT_SENT,
        ORDER_SENT,
        CANCEL_SENT,
        ALTER_SENT
    } sent_msg_type;

typedef enum
{
	DAY = 0,
	GOOD_TILL_CANCEL = 1,
	FILL_OR_KILL = 3,
	GOOD_TILL_DATE = 6
} tif_type;


typedef enum
{
	FLOOR_ROUND = 0, 
	REG_ROUND, 
	CEIL_ROUND
} dec_round_type;


typedef enum 
{
	ORDER_SENT_NEW,
	ORDER_IN_MARKET,
	ORDER_SENT_ALTER,
	ORDER_SENT_CANCEL,
	ORDER_REJECT
} order_stage_type;

string order_stage[] = 
    {
        "ORDER_SENT_NEW",
        "ORDER_IN_MARKET",
        "ORDER_SENT_ALTER",
        "ORDER_SENT_CANCEL",
        "ORDER_REJECT"
    };

// ---- side_type ----
#define BID_SIDE      0    // FIX: 1
#define ASK_SIDE      1    // FIX: 2
#define CROSS_SIDE    7    // FIX: 8
// ---- side_type end ----


// ---- order_execute_type ---------
#define MARKET_ORDER          '1'
#define LIMIT_ORDER           '2'
#define STOP_ORDER            '3'
#define STOP_LIMIT_ORDER      '4'
#define MARKET_LIMIT_ORDER    'K'
// ---- order_execute_type end ----


struct fix_field_struct
{
	char* field_id;
	char* field_val;
	struct fix_field_struct* next;
};


struct fix_message_struct
{
	char* msg_type;
	fix_field_struct* fields;
};

struct struct_account
{
  int account;
  int quote;
  int fill;
};

enum ProductType
    {
        CL,
        RB,
        HO,
        ES,
        SixE,
        GC,
        SixC
    };

string ProductTypeName[]=
    {
        "CL",
        "RB",
        "HO",
        "ES",
        "6E",
        "GC",
        "6C"
    };

#endif
