CFLAGS := -g -DDEBUG -O0 -lm

observe:
	gcc ${CFLAGS} -DOBSERVE_FLOPS -DOBSERVE_MEMOPS reptest_spmm.c -o reptest.x
	./reptest.x 3 16 16 256 verify

run:
	gcc ${CFLAGS} src/reptest_spmm.c -o reptest.x
	./reptest.x 3 16 16 256 verify
