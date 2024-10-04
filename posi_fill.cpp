
#include <iostream>
#include <queue>
#include <string>
#include <string.h>

#include "posi_fill.h"
#include "trace_log.h"
#include "msg_protocol.h"

extern int g_posiSocket;

PosiFill::PosiFill()
{

}


PosiFill::~PosiFill()
{

}


bool PosiFill::send_fill_outright(const char* _symbol, const char* _account, int _price, int _size)
{

  EtClientMessage cm; 
  cm.clearInMap(); 
  cm.putField(fid_msg_type, msg_posi_fill_outright); 
  cm.putField(fid_posi_account, _account); 
  cm.putField(fid_posi_symbol, _symbol); 
  cm.putField(fid_posi_price, _price); 
  cm.putField(fid_posi_fix_qty, _size); 
 
  if ( !cm.sendMessage(g_posiSocket) ) {  
	  DBG_ERR("Fail to send package to client");
	  return false;   
  }

	return true;
}

bool PosiFill::send_fill_spread(const char* _symbol, const char* _account, int _price, int _size)
{
  EtClientMessage cm;
  cm.clearInMap();
  cm.putField(fid_msg_type, msg_posi_fill_spread);
  cm.putField(fid_posi_account, _account);
  cm.putField(fid_posi_symbol, _symbol);
  cm.putField(fid_posi_price, _price);
  cm.putField(fid_posi_fix_qty, _size);

  if ( !cm.sendMessage(g_posiSocket) ) { 
	  DBG_ERR("Fail to send package to client.");
	  return false;  
  }

  return true;
}
