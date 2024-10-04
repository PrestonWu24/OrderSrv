
#ifndef __UTIL_HPP__
#define __UTIL_HPP__


#include <sstream>
using namespace std;


class EtUtil
{
public:
	
	/**
	 * build a TCP connection.
	 * return:
	 *   >0: socket code
	 *   -1: error
	 */
	int static connect_client(const char *passed_ip, int passed_port);

	/**
	 * set a socket with non block.
	 */
	static bool set_non_block(int des);

	/**
	 * convert current time to buffer.
	 */

	static bool build_time_buffer(char* _buffer, int _len);
	static bool getDate(char* curr_date, int len);
	static bool getTime(char*& curr_time, int len);
	static bool build_date(char* _buffer, int _len);
	static bool build_weekday(char* _buffer, int _len);
	static int diff_time(char* _buffer);
	static int get_month_date(int _month, int _year);
    static bool is_leapyear(int _year);
	
	static bool sendData(const int _iSockFd, const char *_buffer, const int _iLength);

	static bool checkLock(int* fileid, char* _fileName);
	static bool releaseLock(int fileid);
	
	static int doubleToInt(double _dVal, int _dot_count);
	static double intToDouble(int _inum, int _dot_count);
	
	static string intToStr(int _val);
	static int strToInt(string _str);
	static string doubleToStr(double _val);
	static double strToDouble(string _str);
	
	static void trimSpace(char *_str);

private:

};

#endif
