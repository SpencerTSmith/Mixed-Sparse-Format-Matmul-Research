CFLAGS := -g -DDEBUG -O0

observe:
	gcc ${CFLAGS} -DOBSERVE_FLOPS -DOBSERVE_MEMOPS src/reptest_spmm.c -o reptest.x
	./reptest.x 5 16 16 256 verify

run:
	gcc ${CFLAGS} src/reptest_spmm.c -o reptest.x
	./reptest.x 5 16 16 256 verify
