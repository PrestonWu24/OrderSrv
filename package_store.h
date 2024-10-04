
#ifndef _PACKAGE_STORE_H_
#define _PACKAGE_STORE_H_


#include <stdio.h>
#include <string.h>
#include <cstdlib>

// -------------
// struct
// -------------
struct package_struct
{
	int buffer_len;
	int seq_num;
	char msg_type[5];
	char* buffer;
	struct package_struct* next;
};


class PackageStore
{
public:

	// constructors
	PackageStore();

	// destructor
	~PackageStore();

	// functions
	static bool add_package(const char* _buffer, int _length, int _seq_num, const char* _msg_type);
	static package_struct* get_package(int _seq_num);
	
	void static print_list();

private:
	
	// function
	static bool remove_from_begin(int _count);
	static int get_seq_num(const char* _buffer);
    
    // data
    static int current_count;
    static package_struct* begin_ptr;
    static package_struct* last_ptr;
};


#endif
