#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "trace_log.h"
#include "util.h"
#include "read_config.h"

int EtReadConfig::m_listenPort = 0;

string EtReadConfig::m_orderSrvIp = "";
int EtReadConfig::m_orderSrvPort = -1;
	
string EtReadConfig::m_positionSrvIp = "";
int EtReadConfig::m_positionSrvPort = -1;

vector<instrument_struct> EtReadConfig::m_instrumentList;
vector<struct_account> EtReadConfig::m_account;
string EtReadConfig::m_instrumentFile = "";

int EtReadConfig::m_fill_limit = 0;
int EtReadConfig::m_fill_sum = 0;

bool EtReadConfig::loadConfigFile(const char* _configFile)
{
    FILE *fd;
    char sBuf[128];
    char* ptr = NULL;
    int paramCount = 0;
    
    char param[32];
    char val[32];
    int account = 0;

	fd = fopen(_configFile, "r");
	if (NULL == fd)
	{
		DBG(TRACE_LOG_ERR, "Failed to open configuration file: %s", _configFile);
		return false;
	}

	memset(sBuf, 0, 128);
	while (fgets(sBuf, 128, fd) != NULL)
	{
		// line is too long and do not continue to read the next line
		if (strlen(sBuf) > 127)
		{
			DBG(TRACE_LOG_ERR, "A line in the config is too long.");
			break;
		}
		
		EtUtil::trimSpace(sBuf);
		
		// comment line or a line with space only
		if (('#' == sBuf[0]) || ('\0' == sBuf[0])) 
		{
			continue;
		}
		
		ptr = strchr(sBuf, '=');
		if (ptr == NULL)
		{
			DBG_ERR("do not find = sign.");
			break;
		}
		
		memset(param, 0, 32);
		memset(val, 0, 32);
		strncpy(param, sBuf, ptr - sBuf);
		ptr++;
		strcpy(val, ptr);
		
		if (strcmp(param, "order_srv_ip") == 0)
		{
			m_orderSrvIp = val;
			paramCount++;
		}
		else if (strcmp(param, "order_srv_port") == 0)
		{
			m_orderSrvPort = atoi(val);
			paramCount++;
		}
        else if (strcmp(param, "position_srv_ip") == 0)
          {
            m_positionSrvIp = val;
            paramCount++;
          }
        else if (strcmp(param, "position_srv_port") == 0)
          {
            m_positionSrvPort = atoi(val);
            paramCount++;
          }
		else if (strcmp(param, "listen_port") == 0)
		{
			m_listenPort = atoi(val);
			paramCount++;
		}
		else if (strcmp(param, "instrument_list_file") == 0)
		{
			m_instrumentFile = val;
			paramCount++;
		}
        else if(strcmp(param, "fill_limit") == 0)
          {
            m_fill_limit = atoi(val);
            paramCount++;
          }
        else if(strncmp(param, "account", 7) == 0){ 
          account = atoi(val);
          struct_account new_account;
          new_account.account = account;
          new_account.quote = 0;
          new_account.fill = 0;
          m_account.push_back(new_account);
          paramCount++; 
        }
		else
		{
			DBG_ERR("parameter is error: %s", param);
			break;
		}
	}

	fclose(fd);
	
	if (paramCount >= 16)
	{
		return true;
	}

	DBG_ERR("parameter count not matched: got %d.",paramCount);	

	return false;
}

int EtReadConfig::getAccountId(string _account) 
{ 
  int i = 0; 
 
  vector<struct_account>::iterator it; 
  it = EtReadConfig::m_account.begin(); 
  while(it != EtReadConfig::m_account.end()){ 
    if( (*it).account == EtUtil::strToInt(_account)){ 
      return i; 
    } 
     
    it++; 
    i++; 
  } 
 
  return -1; 
}

int EtReadConfig::getSecurityId(const string& _symbol)
{
	vector<instrument_struct>::iterator iter;
	for (iter = m_instrumentList.begin(); iter != m_instrumentList.end(); iter++)
	{
		if (iter->symbol == _symbol)
		{
			return iter->security_id;
		}
	}
	
	DBG_ERR("do not find this instrument: %s", _symbol.c_str());
	return -1;
}


bool EtReadConfig::loadInstrument(const char* _product)
{
    FILE *fd;
    char buff[64];
    char* ptr = NULL;
    
    char id[16];
    char symbol[48];
    char product[3];

	fd = fopen(m_instrumentFile.c_str(), "r");
	if (NULL == fd)
	{
		DBG(TRACE_LOG_ERR, "Failed to open instrument file: %s", m_instrumentFile.c_str());
		return false;
	}

	memset(buff, 0, 64);
	while (fgets(buff, 64, fd) != NULL)
	{
		// line is too long and do not continue to read the next line
		if (strlen(buff) > 63)
		{
			DBG(TRACE_LOG_ERR, "A line in the instrument file is too long.");
			return false;
		}
		
		EtUtil::trimSpace(buff);
		
		// comment line or a line with space only
		if (('#' == buff[0]) || ('\0' == buff[0])) 
		{
			continue;
		}
		
		ptr = strchr(buff, '=');
		if (ptr == NULL)
		{
			DBG_ERR("did not find a sign.");
			return false;
		}
		
		memset(id, 0, 16);
		memset(symbol, 0, 48);
		strncpy(id, buff, ptr - buff);
		ptr++;
		strcpy(symbol, ptr);
		
		product[0] = symbol[0];
		product[1] = symbol[1];
		product[2] = '\0';
		
		if (strcmp(product, _product) != 0)
		{
			continue;
		}
		
		instrument_struct newInstr;
		newInstr.security_id = atoi(id);
		newInstr.symbol = symbol;
		m_instrumentList.push_back(newInstr);
		
	}

	DBG(TRACE_LOG_DEBUG, "%d symbols loaded.",m_instrumentList.size() );
	fclose(fd);	
	return true;
}

