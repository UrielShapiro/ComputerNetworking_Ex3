# Use the gcc compiler.
CC = gcc

# Flags for the compiler.
CFLAGS = -Wall -Wextra -Werror -ggdb -std=c99 -pedantic

# Command to remove files.
RM = rm -f

EXECUTABLES = TCP_Sender TCP_Receiver RUDP_Sender RUDP_Receiver rudp_sender_test rudp_receiver_test

# Phony targets - targets that are not files but commands to be executed by make.
.PHONY: all default clean runuc runus

# Default target - compile everything and create the executables and libraries.
all: TCP_Sender TCP_Receiver RUDP_Sender RUDP_Receiver

# Alias for the default target.
default: all


############
# Programs #
############

# Compile the udp server.
TCP_Sender: TCP_Sender.o
	$(CC) $(CFLAGS) -o $@ $^

# Compile the udp client.
TCP_Receiver: TCP_Receiver.o
	$(CC) $(CFLAGS) -o $@ $^

TCP_Receiver.o: TCP_Receiver.c
	$(CC) $(CFLAGS) -c $< -o $@

TCP_Sender.o: TCP_Sender.c
	$(CC) $(CFLAGS) -c $< -o $@

# Compile the udp server.
RUDP_Sender: RUDP_Sender.o rudp.o
	$(CC) $(CFLAGS) -o $@ $^

# Compile the udp client.
RUDP_Receiver: RUDP_Receiver.o rudp.o
	$(CC) $(CFLAGS) -o $@ $^

RUDP_Receiver.o: RUDP_Receiver.c rudp.h
	$(CC) $(CFLAGS) -c $< -o $@

RUDP_Sender.o: RUDP_Sender.c rudp.h
	$(CC) $(CFLAGS) -c $< -o $@

rudp.o: rudp.c rudp.h
	$(CC) $(CFLAGS) -c $< -o $@

################
# System Trace #
################

# Run the udp server with system trace.
runus_trace: udp_server
	strace ./udp_server

# Run the udp client with system trace.
runuc_trace: udp_client
	strace ./udp_client

#################
# Cleanup files #
#################

# Remove all the object files, shared libraries and executables.
clean:
	$(RM) *.o *.so $(EXECUTABLES)

rudp_sender_test.o: rudp_sender_test.c rudp.h
	$(CC) $(CFLAGS) -c -o $@ $<

rudp_sender_test: rudp_sender_test.o rudp.o
	$(CC) $(CFLAGS) -o $@ $^

rudp_receiver_test.o: rudp_receiver_test.c rudp.h
	$(CC) $(CFLAGS) -c -o $@ $<

rudp_receiver_test: rudp_receiver_test.o rudp.o
	$(CC) $(CFLAGS) -o $@ $^

tests: rudp_receiver_test rudp_sender_test