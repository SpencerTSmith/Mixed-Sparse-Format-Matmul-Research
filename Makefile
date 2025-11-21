CFLAGS := -g -DDEBUG -O2 -DSIMULATE_FLOPS -DSIMULATE_MEMOPS

run:
	gcc ${CFLAGS} src/reptest_spmm.c -o reptest.x
	./reptest.x 5 16 16 16 verify
