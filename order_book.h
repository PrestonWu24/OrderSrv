#ifndef __ORDER_BOOK_INCLUDED
#define __ORDER_BOOK_INCLUDED


#include <pthread.h>
#include <string.h>
#include "audit_trail.h"
#include "globex_common.h"

struct order_struct
{
	int						global_order_id;              // global order number
  int                     local_order_id;               // each program local order number
    int                     sent_seq_num;
	char                    display_name[50];      // name
	
	char                    ex_order_id[MAX_STRING_ORDER_ID];                // [0] 37: OrderID
	char                    correlation_cli_order_id[MAX_STRING_ORDER_ID];   // [1] 9717: CorrelationClOrdID
	char                    orig_cli_order_id[MAX_STRING_ORDER_ID];          // [2] 41: OrigClOrdID

	int                     side;
	int					    price;
	double 					stop_price;
	int                     total_size;
	int                     open_size;
	order_stage_type		stage;
	tif_type				tif;
	char					execute_type;
	bool		giveup;
    char            expire_date[16];
	
	struct order_struct     *next;
};


class order_book
{
public:
	// constructors
	order_book();

	// destructor
	~order_book();

	// functions
	bool add_order(int _global_num, int _local_num, const char* _name, int _side, 
	               int _price, int _size, int sent_seq_num);
	bool remove_order(int _global_num);
	bool get_order(int _global_num, order_struct* _re_order);
	bool get_order_by_client_order_id(const char* _cli_order_id, order_struct* _return_order);
    bool get_order_by_client_order_id_in_remove_list(const char* _cli_order_id, order_struct* _return_order);
	bool get_order_by_seq_num(int _last_seq_num, order_struct* _return_order);
	bool filled_modify_order(const char* _ex_order_id, int _fill_size, order_struct* _re_order, const char* _symbol);
	bool accepted_modify_order(const char* _cli_order_id, const char* _ex_order_id, 
	                            order_struct* _re_order);
	bool altered_modify_order(const char* _cli_order_id, int _size, int _price, const char* _new_ex_order_id,
                              order_struct* _re_order);
	bool set_order_state(int _global_num, order_stage_type _stage);
	
	bool set_orig_cli_order_id(int _global_num, const char* _id);
	bool set_correlation_cli_order_id(int _global_num, const char* _id);
        bool set_expire_date(int _global_num, const char* _date);
    int get_order_num();
    int get_order_num_in_remove_list();
    bool copy_remove_order(int _global_num);
    bool find_in_remove_list(const char* _ex_order_id, int _fill_size, order_struct* _re_order, const char* _symbol);
    bool remove_order_in_remove_list(int _global_num);
	void print_orders();

private:

	// data
	pthread_mutex_t order_list_lock;
	pthread_mutexattr_t order_list_lock_attr;

	order_struct *orders;
	order_struct *last_order;
    order_struct *remove_orders;
    order_struct *last_remove_orders;

};


#endif
