#include "string.h"
#include "trace_log.h"
#include "package_store.h"

#define TOTAL_PACKAGE_COUNT     50
#define DELETE_PACKAGE_COUNT    10

int PackageStore::current_count = 0;
package_struct* PackageStore::begin_ptr = NULL;
package_struct* PackageStore::last_ptr = NULL;


PackageStore::PackageStore()
{
}


PackageStore::~PackageStore()
{
	package_struct* package_ptr = begin_ptr;
	package_struct* delete_ptr = NULL;
	
	while (package_ptr != NULL)
	{
		delete_ptr = package_ptr;
		package_ptr = package_ptr->next;
		
		if (delete_ptr->buffer != NULL)
		{
			delete delete_ptr->buffer;
		}
		delete_ptr->buffer = NULL;
		delete delete_ptr;
	}
}


bool PackageStore::add_package(const char* _buffer, int _length, int _seq_num, const char* _msg_type)
{
	if (_buffer == NULL || _length <= 0 || _msg_type == NULL)
	{
		DBG_ERR("parameter is error.");
		return false;
	}

	if (_seq_num <= 0)
	{
		DBG_ERR("The seq number is error.");
		return false;
	}
	
	if (current_count > TOTAL_PACKAGE_COUNT)
	{
		if ( !remove_from_begin(DELETE_PACKAGE_COUNT) )
		{
			DBG_ERR("fail to remove from list's beginning.");
			return false;
		}
		current_count -= DELETE_PACKAGE_COUNT;
	}

    package_struct* new_package = new package_struct;
	memset(new_package, 0, sizeof(package_struct));
	
	new_package->buffer_len = _length;
	new_package->seq_num = _seq_num;
	strcpy(new_package->msg_type, _msg_type);
	new_package->buffer = new char[_length];
	memset(new_package->buffer, 0, _length);
	memcpy(new_package->buffer, _buffer, _length);
	new_package->next = NULL;

	if (begin_ptr == NULL)
    {
		begin_ptr = new_package;
		last_ptr = new_package;
    }
    else
    {
    	last_ptr->next = new_package;
		last_ptr = new_package;
	}
	current_count++;
	
	return true;
}


package_struct* PackageStore::get_package(int _seq_num)
{
	if (_seq_num < 0)
	{
		DBG_ERR("seq_num is error.");
		return NULL;
	}
	
	package_struct* ptr = begin_ptr;
	
	while (ptr != NULL)
	{
		if (ptr->seq_num == _seq_num)
		{
			return ptr;
		}
		ptr = ptr->next;
	}

	DBG_ERR("do not find this seq_num: %d", _seq_num);
	return NULL;	
}


bool PackageStore::remove_from_begin(int _count)
{
	if (_count <= 0 || _count > TOTAL_PACKAGE_COUNT)
	{
		DBG_ERR("parameter is error.");
		return false;
	}
	
	int i = 0;
	package_struct* package_ptr = begin_ptr;
	package_struct* delete_ptr = NULL;
	
	while (package_ptr != NULL && i < _count)
	{
		delete_ptr = package_ptr;
		package_ptr = package_ptr->next;
		begin_ptr = package_ptr;
		i++;
		
		if (delete_ptr->buffer != NULL)
		{
			delete delete_ptr->buffer;
		}
		delete_ptr->buffer = NULL;
		delete delete_ptr;
	}
	return true;
}


int PackageStore::get_seq_num(const char* _buffer)
{
	if (_buffer == NULL)
	{
		DBG_ERR("parameter is error.");
		return false;
	}
	
	const char* ptr = NULL;
	const char* soh_ptr = NULL;
	char temp[13];
	int i = 0;
	
	ptr = strstr(_buffer, "34=");
	if (ptr == NULL)
	{
		DBG_ERR("do not find the seq number field.");
		return -1;
	}
	
	// SOH: 0x01
	soh_ptr = strchr(ptr, 0x01);
	if (soh_ptr == NULL)
	{
		DBG_ERR("do not find the SOH.");
		return -1;
	}
	
	memset(temp, 0, 13);
	ptr += 3;
	while (ptr < soh_ptr)
	{
		temp[i] = *ptr;
		i++;
		ptr++;
		
		if (i > 12)
		{
			DBG_ERR("seq number is to long.");
			return -1;
		}
	}
	return atoi(temp);
}


void PackageStore::print_list()
{
	package_struct* ptr = begin_ptr;
	
	DBG_DEBUG("-----------------------");
	while (ptr != NULL)
	{
		DBG_DEBUG("buffer: %s", ptr->buffer);
      ptr = ptr->next;
	}

	DBG_DEBUG("-----------------------");
}
