CC=clang
CFLAGS=-Wall -g -O3

k: k.c
	$(CC) $(CFLAGS) -o $@ $<

check: k
	./k < fib.k

clean:
	-rm -rf k k.dSYM
