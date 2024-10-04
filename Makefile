EXENAME = order

CC = g++
CFLAGS = -Wall -g -I./include
LIBS = -lpthread

OBJS = audit_trail.o \
       client_message.o \
       exec_globex.o \
       exec_globex_cmd.o \
       main.o \
       trade_process.o \
       order_book.o \
       package_store.o \
       read_thread.o \
       read_client_data.o \
       read_cme_data.o \
       monitor.o \
       read_config.o \
       posi_fill.o \
       trade_socket_list.o \
       trace_log.o \
       util.o 

all: $(EXENAME)

$(EXENAME): $(OBJS)
	$(CC) $(CFLAGS)  -Xlinker -zmuldefs -o $@ $(OBJS) $(LIBS)

%.o: %.cpp
	$(CC) $(CFLAGS) $< -c -o $@

.PHONY: clean

clean:
	rm -rf $(EXENAME) *.o *.bak *~
