
#ifndef __CLIENT_MESSAGE_H__
#define __CLIENT_MESSAGE_H__


#include <string>
#include <map>

using namespace std;

#define CLIENT_BUFFER_LEN    1024


class EtClientMessage
{
public:

	// constructor
	EtClientMessage();
	
	// function
	bool putField(int _field, const char* _val);
	bool putField(int _field, string& _val);
	bool putField(int _field, int _val);
	bool putField(int _ield, double _val);
	
	bool getFieldValue(int _field, string& _returnVal);
	bool getFieldValue(int _field, int& _returnVal);
	bool getFieldValue(int _field, double& _returnVal);
	
	bool getMessage(char** _buff, int& _len);
	bool sendMessage(int _socket);
    bool sendMessageWithId(string _id);
	bool parseMessage(const char* _buffer, int _iLen);
	void clearInMap();
	
	void printOutMap();
	
private:
	
	// function
	bool buildMessage();
	
	// data
	char buffer[CLIENT_BUFFER_LEN];
	int buffer_len;
	
	map<int, string> m_outMap;
	map<int, string> m_inMap;
};

#endif
