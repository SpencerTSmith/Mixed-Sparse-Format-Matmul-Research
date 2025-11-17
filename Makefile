CFLAGS := -g -DDEBUG -O2 -DSIMULATE_MEMOPS -DSIMULATE_FLOPS

run:
	gcc -g -DDEBUG -O2 src/reptest_spmm.c -o reptest.x
	./reptest.x 5 1024 1024 verify
