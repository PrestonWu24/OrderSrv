
#ifndef _ET_READ_CONFIG_H_
#define _ET_READ_CONFIG_H_


#include <vector>
#include <string>
#include "globex_common.h"
using namespace std;


struct instrument_struct
{
	int security_id;
	string symbol;
};


class EtReadConfig
{
public:

	// ---- function ----
	static bool loadConfigFile(const char* _configFile);
	static bool loadInstrument(const char* _product);
	static int getSecurityId(const string& _symbol);
    static int getAccountId(const string _account);
	
	// ---- data ----
	static int m_listenPort;
	
	static string m_orderSrvIp;
	static int m_orderSrvPort;

    static string m_positionSrvIp;
    static int m_positionSrvPort;
	
	static string m_instrumentFile;
    static int m_fill_limit;
    static int m_fill_sum;
    static vector<struct_account> m_account;

private:
	
	static vector<instrument_struct> m_instrumentList;
};


#endif
