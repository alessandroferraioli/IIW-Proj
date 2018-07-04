CC = gcc
CFLAGS =-Wall -Wextra -Wpedantic -o2
CFILES=$(shell ls *.c)
PROGS=$(CFILES:%.c=%)

install:server client
	
server:Server.c Server.h basic.c lock_functions.c lock_functions.h basic.h manage_io.c manage_io.h dynamic_list.c dynamic_list.h parser.c parser.h timer_functions.c timer_functions.h get_server.c get_server.h functions_communication.h functions_communication.c list_server.c list_server.h put_server.h put_server.c
	$(CC) $(CFLAGS) -pthread -o  $@ $^ -lrt

client:Client.c Client.h basic.c basic.h lock_functions.c lock_functions.h manage_io.c manage_io.h dynamic_list.c dynamic_list.h parser.c  parser.h timer_functions.c timer_functions.h get_client.c get_client.h functions_communication.h functions_communication.c list_client.c list_client.h put_client.h put_client.c
	$(CC) $(CFLAGS) -pthread  -o  $@ $^ -lrt
