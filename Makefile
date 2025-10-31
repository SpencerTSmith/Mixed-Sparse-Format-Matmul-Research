run:
	gcc -g -DDEBUG -O2 src/reptest_spmm.c -o reptest.x
	./reptest.x 5 1024 1024 0.99
