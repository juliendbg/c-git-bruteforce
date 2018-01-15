.PHONY: all

all: git-bruteforce

git-bruteforce:
	mkdir -p bin/
	cc -Wall -L/usr/local/lib -lgit2 -I/usr/local/include -O2 -pipe src/*.c -o bin/git-bruteforce

install: git-bruteforce
	install bin/git-bruteforce /usr/local/bin

debug:
	mkdir -p bin/
	cc -L/usr/local/lib -lgit2 -I/usr/local/include -O0 -g -pipe src/*.c -o bin/git-bruteforce
