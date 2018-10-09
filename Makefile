CC = g++
CFLAGS = -g -Wall -std=c++11

all: Proposer Acceptor Learner

Proposer: Proposer.cpp
	$(CC) $(CFLAGS) $< -o $@

Acceptor: Acceptor.cpp
	$(CC) $(CFLAGS) $< -o $@

Learner: Learner.cpp
	$(CC) $(CFLAGS) $< -o $@
