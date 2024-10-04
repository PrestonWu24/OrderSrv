#ifndef _POSI_FILL_H
#define _POSI_FILL_H

#include <queue>
#include <iostream>
#include <string>
#include <string.h>
#include <cstring>
#include "client_message.h"

using namespace std;

// -------------
// struct
// -------------

class PosiFill
{
public:
	PosiFill();
	~PosiFill();

	bool send_fill_outright(const char* _symbol, const char* _account, int _price, int _size);
    bool send_fill_spread(const char* _symbol, const char* _account, int _price, int _size);

private:

};

#endif
