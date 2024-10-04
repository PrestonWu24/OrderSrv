
#include "trace_log.h"
#include "util.h"
#include "order_book.h"

order_book::order_book()
{
	pthread_mutexattr_init(&order_list_lock_attr);
	pthread_mutexattr_settype(&order_list_lock_attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&order_list_lock, &order_list_lock_attr);

	orders = NULL;
	last_order = NULL;	
    remove_orders = NULL;
    last_remove_orders = NULL;
}


order_book::~order_book()
{
}

bool order_book::add_order(int _global_num, int _local_num, const char* _name, int _side, 
			   int _price, int _size, int _sent_seq_num)
{
	DBG_DEBUG("add order: global_num: %d, local_num: %d, name: %s, side: %d, price: %d, size: %d, sent_seq_num: %d", 
			  _global_num, _local_num, _name, _side, _price, _size, _sent_seq_num);
	order_struct* new_order = NULL;
	
	new_order = new order_struct;
	memset(new_order, 0, sizeof(order_struct));
	
    new_order->global_order_id = _global_num;
	new_order->local_order_id = _local_num;
	new_order->sent_seq_num = _sent_seq_num;
	strcpy(new_order->display_name, _name);
	new_order->side = _side;
	new_order->price = _price;
	new_order->total_size = _size;
	new_order->open_size = _size;
	new_order->stage = ORDER_SENT_NEW;

	pthread_mutex_lock(&order_list_lock);

	if (orders == NULL)
    {
		orders = new_order;
		last_order = new_order;
		pthread_mutex_unlock(&order_list_lock);
		return true;
    }
	
	last_order->next = new_order;
	last_order = new_order;
	pthread_mutex_unlock(&order_list_lock);
	
	return true;
}

bool order_book::copy_remove_order(int _local_num)
{
  order_struct removeOrder; 
  if (-1 == get_order(_local_num, &removeOrder)) 
    { 
		DBG_ERR("fail to find the order: %d", _local_num);
		return false; 
    } 

  order_struct* new_order = NULL; 
     
  new_order = new order_struct; 
  memset(new_order, 0, sizeof(order_struct)); 
     
  new_order->global_order_id = _local_num; 
  new_order->sent_seq_num = removeOrder.sent_seq_num; 
  strcpy(new_order->display_name, removeOrder.display_name); 
  new_order->side = removeOrder.side; 
  new_order->price = removeOrder.price; 
  new_order->total_size = removeOrder.total_size; 
  new_order->open_size = removeOrder.open_size; 
  new_order->stage = ORDER_IN_MARKET; 
  strcpy(new_order->ex_order_id, removeOrder.ex_order_id);
 
  if (remove_orders == NULL) 
    { 
      remove_orders = new_order; 
      last_remove_orders = new_order;
      return true; 
    } 

  last_remove_orders->next = new_order;
  last_remove_orders = new_order;
     
  return true;
}

bool order_book::remove_order(int _local_num)
{
	DBG_DEBUG("remove order: local_num=%d", _local_num);

	order_struct *to_remove = NULL;
	order_struct *prev = NULL;

	pthread_mutex_lock(&order_list_lock);

	for (to_remove = orders; to_remove != NULL; to_remove = to_remove->next)
    {
		if (to_remove->global_order_id == _local_num)
	    {
			if (prev == NULL)    // first node
		    {
				orders = to_remove->next;
		    }
			else
		    {
				prev->next = to_remove->next;
		    }

			if (last_order == to_remove)    // last node
		    {
				last_order = prev;
		    }
		    
			delete to_remove;

			pthread_mutex_unlock (&order_list_lock);
			return true;
	    }
		prev = to_remove;
    }

	pthread_mutex_unlock (&order_list_lock);
	DBG_ERR("do not find this order_id: %d", _local_num);

	return false;
}


/**
 * gat a order by order_id.
 */
bool order_book::get_order(int _global_num, order_struct* _re_order)
{
	order_struct* curr_order = NULL;

	pthread_mutex_lock(&order_list_lock);

	for (curr_order = orders; curr_order != NULL; curr_order = curr_order->next)
    {
		if ((curr_order->global_order_id == _global_num))
	    {
			memcpy(_re_order, curr_order, sizeof(order_struct));
			pthread_mutex_unlock(&order_list_lock);
			return true;
	    }
    }

	pthread_mutex_unlock(&order_list_lock);
	DBG_ERR("do not find this order_id: %d", _global_num);

	return false;
}


bool order_book::get_order_by_client_order_id(const char* _cli_order_id, order_struct* _return_order)
{
	if (_cli_order_id == NULL || _return_order == NULL)
	{
		DBG_ERR("The parameter is error.");
		return false;
	}
	
	order_struct* curr_order = NULL;
	pthread_mutex_lock(&order_list_lock);

	for (curr_order = orders; curr_order != NULL; curr_order = curr_order->next)
    {
		if (strcmp(curr_order->orig_cli_order_id, _cli_order_id) == 0)
		{
			memcpy(_return_order, curr_order, sizeof(order_struct));
			pthread_mutex_unlock (&order_list_lock);
			return true;
		}
	}

	pthread_mutex_unlock(&order_list_lock);
	return false;
}

bool order_book::get_order_by_client_order_id_in_remove_list(const char* _cli_order_id, order_struct* _return_order) 
{ 
  if (_cli_order_id == NULL || _return_order == NULL) 
    { 
		DBG_ERR("The parameter is error."); 
		return false; 
    } 
     
  order_struct* curr_order = NULL; 
 
  for (curr_order = remove_orders; curr_order != NULL; curr_order = curr_order->next) 
    { 
      if (strcmp(curr_order->orig_cli_order_id, _cli_order_id) == 0) 
        { 
          memcpy(_return_order, curr_order, sizeof(order_struct)); 
          return true; 
        } 
    } 
 
  return false; 
} 


bool order_book::get_order_by_seq_num(int _last_seq_num, order_struct* _return_order)
{
    if (_last_seq_num == -1 || _return_order == NULL)
        {
			DBG_ERR("The parameter is error.");
			return false;
        }

    order_struct* curr_order = NULL;
    pthread_mutex_lock(&order_list_lock);

    for (curr_order = orders; curr_order != NULL; curr_order = curr_order->next)
	{
	    if (curr_order->sent_seq_num == _last_seq_num)
                {
                  memcpy(_return_order, curr_order, sizeof(order_struct));
                  pthread_mutex_unlock (&order_list_lock);
                  return true;
                }
        }

    pthread_mutex_unlock(&order_list_lock);
    return false;
}

bool order_book::filled_modify_order(const char* _ex_order_id, int _fill_size, order_struct* _re_order, const char* _symbol) 
{ 
	DBG_DEBUG("filled order: _ex_order_id=%s", _ex_order_id);
  order_struct* curr_order = NULL; 
 
  pthread_mutex_lock(&order_list_lock); 
 
  for (curr_order = orders; curr_order != NULL; curr_order = curr_order->next) 
    { 
      if (strcmp(curr_order->ex_order_id, _ex_order_id) == 0 && strcmp(curr_order->display_name, _symbol) == 0) 
        { 
          curr_order->open_size -= _fill_size; 
          memcpy(_re_order, curr_order, sizeof(order_struct)); 
          pthread_mutex_unlock(&order_list_lock); 
             
          return true; 
        } 
    } 
 
  pthread_mutex_unlock(&order_list_lock); 
  DBG_ERR("do not find the order, ex_order_id: %s", _ex_order_id);
  return false; 
} 

bool order_book::accepted_modify_order(const char* _cli_order_id, const char* _ex_order_id, 
	order_struct* _re_order)
{
	DBG_DEBUG("accept order: _cli_order_id=%s", _cli_order_id);

	order_struct* curr_order = NULL;

	pthread_mutex_lock(&order_list_lock);

	for (curr_order = orders; curr_order != NULL; curr_order = curr_order->next)
    {
		if (strcmp(curr_order->orig_cli_order_id, _cli_order_id) == 0)
	    {
	    	strcpy(curr_order->ex_order_id, _ex_order_id);
			curr_order->stage = ORDER_IN_MARKET;

			memcpy(_re_order, curr_order, sizeof(order_struct));
			pthread_mutex_unlock(&order_list_lock);
			return true;
	    }
    }

	pthread_mutex_unlock(&order_list_lock);
    
	DBG_ERR("do not find the order, cli_order_id: %s", _cli_order_id);
	return false;
}


bool order_book::altered_modify_order(const char* _cli_order_id, int _size, int _price, const char* _new_ex_order_id,
	order_struct* _re_order)
{
	DBG_DEBUG("altered modify order: _cli_order_id=%s", _cli_order_id);

	order_struct* curr_order = NULL;

	pthread_mutex_lock(&order_list_lock);

	for (curr_order = orders; curr_order != NULL; curr_order = curr_order->next)
    {
		if (strcmp(curr_order->orig_cli_order_id, _cli_order_id) == 0)
	    {
	    	curr_order->price = _price;
            if(_size != 0){
              curr_order->total_size = _size;
              curr_order->open_size = _size;
            }
			strcpy(curr_order->ex_order_id, _new_ex_order_id);
			curr_order->stage = ORDER_IN_MARKET;

			memcpy(_re_order, curr_order, sizeof(order_struct));
			pthread_mutex_unlock(&order_list_lock);
			return true;
	    }
    }

	pthread_mutex_unlock(&order_list_lock);
	DBG_ERR("do not find the order, cli_order_id: %s", _cli_order_id);

	return false;
}


bool order_book::set_order_state(int _global_num, order_stage_type _stage)
{
	order_struct* curr_order = NULL;
	
	pthread_mutex_lock(&order_list_lock);

	for (curr_order = orders; curr_order != NULL; curr_order = curr_order->next)
	{
		if (curr_order->global_order_id == _global_num)
	    {
			curr_order->stage = _stage;
			pthread_mutex_unlock(&order_list_lock);
			return true;
	    }
    }
    
    pthread_mutex_unlock(&order_list_lock);
	DBG_ERR("do not find the order_id: %d", _global_num);

	return false;
}


/**
 * set orig_cli_order_id.
 */
bool order_book::set_orig_cli_order_id(int _global_num, const char* _id)
{
	order_struct* curr_order = NULL;
	
	if (_id == NULL || strlen(_id) >= MAX_STRING_ORDER_ID)
	{
		DBG_ERR("parameter is error.");
		return false;
	}

	pthread_mutex_lock(&order_list_lock);

	for (curr_order = orders; curr_order != NULL; curr_order = curr_order->next)
    {
		if (curr_order->global_order_id == _global_num)
	    {
			strcpy(curr_order->orig_cli_order_id, _id);
			pthread_mutex_unlock(&order_list_lock);

			return true;
		}
    }
	
	pthread_mutex_unlock(&order_list_lock);
	DBG_ERR("not find the local number: %d", _global_num);

	return false;
}


/**
 * set correlation_cli_order_id.
 */
bool order_book::set_correlation_cli_order_id(int _global_num, const char* _id)
{
	order_struct* curr_order = NULL;
	
	if (_id == NULL || strlen(_id) >= MAX_STRING_ORDER_ID)
	{
		DBG_ERR("parameter is error.");
		return false;
	}

	pthread_mutex_lock(&order_list_lock);

	for (curr_order = orders; curr_order != NULL; curr_order = curr_order->next)
    {
		if (curr_order->global_order_id == _global_num)
	    {
			strcpy(curr_order->correlation_cli_order_id, _id);
			pthread_mutex_unlock(&order_list_lock);
			return true;
		}
    }
	
	pthread_mutex_unlock(&order_list_lock);
	DBG_ERR("not find the local number: %d", _global_num);

	return false;
}

int order_book::get_order_num() 
{
  int order_num = 0; 
  order_struct* curr_order = NULL; 
 
  pthread_mutex_lock(&order_list_lock); 
 
  for (curr_order = orders; curr_order != NULL; curr_order = curr_order->next) 
    { 
      order_num++; 
    } 
  pthread_mutex_unlock(&order_list_lock); 
  return order_num; 
} 

int order_book::get_order_num_in_remove_list()  
{
  int order_num = 0;  
  order_struct* curr_order = NULL;  
  
  for (curr_order = remove_orders; curr_order != NULL; curr_order = curr_order->next)  
    {  
      order_num++;  
    }  

  return order_num;  
}  

void order_book::print_orders()
{
	int order_num = 0;
	order_struct* curr_order = NULL;

	pthread_mutex_lock(&order_list_lock);

	for (curr_order = orders; curr_order != NULL; curr_order = curr_order->next)
    {
		DBG_ERR("%d, %s, %s, %d, %d, %d, %d", curr_order->global_order_id, curr_order->display_name, 
				 curr_order->ex_order_id, curr_order->side, curr_order->price, curr_order->total_size, curr_order->open_size);
      order_num++;
    }

	pthread_mutex_unlock(&order_list_lock);
}

bool order_book::set_expire_date(int _global_num, const char* _date)
{
    order_struct* curr_order = NULL;

    pthread_mutex_lock(&order_list_lock);

    for (curr_order = orders; curr_order != NULL; curr_order = curr_order->next)
	{
	    if (curr_order->global_order_id == _global_num)
		{
		    strcpy(curr_order->expire_date, _date);
		    pthread_mutex_unlock(&order_list_lock);
		    return true;
                }
	}

    pthread_mutex_unlock (&order_list_lock);
	DBG_ERR("not find the local number: %d", _global_num);

    return false;
}

bool order_book::find_in_remove_list(const char* _ex_order_id, int _fill_size, order_struct* _re_order, const char* _symbol)  
{  
	DBG_DEBUG("find_in_remove_list: filled_order: _ex_order_id=%s", _ex_order_id);
  order_struct* curr_order = NULL;  
  
  for (curr_order = remove_orders; curr_order != NULL; curr_order = curr_order->next)  
    {  
      if (strcmp(curr_order->ex_order_id, _ex_order_id) == 0 && strcmp(curr_order->display_name, _symbol) == 0)  
        {  
          curr_order->open_size -= _fill_size;  
          memcpy(_re_order, curr_order, sizeof(order_struct));  
              
          return true;  
        }  
    }  
  
  DBG_ERR("do not find the order, ex_order_id: %s", _ex_order_id);
  return false;  
}  

bool order_book::remove_order_in_remove_list(int _global_num) 
{ 
	DBG_DEBUG("remove order in remove list: global_num=%d", _global_num);
  order_struct *to_remove = NULL; 
  order_struct *prev = NULL; 
 
  for (to_remove = remove_orders; to_remove != NULL; to_remove = to_remove->next) 
    { 
      if (to_remove->global_order_id == _global_num) 
        { 
          if (prev == NULL)    // first node 
            { 
              remove_orders = to_remove->next; 
            } 
          else 
            { 
              prev->next = to_remove->next; 
            } 
 
          if (last_remove_orders == to_remove)    // last node 
            { 
              last_remove_orders = prev; 
            } 
             
          delete to_remove; 
 
          return true; 
        } 
      prev = to_remove; 
    } 

  DBG_ERR("do not find this order_id: %d", _global_num);
  return false; 
} 
 
