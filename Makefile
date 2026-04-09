CFLAGS := -g -DDEBUG -O0 -lm

SECONDS := 2
SWEEP   := both

roofline_asm:
	nasm -f elf64 -o roofline.o roofline.asm
	ar rcs roofline.a roofline.o

observe: roofline_asm
	gcc ${CFLAGS} -DOBSERVE_FLOPS -DOBSERVE_MEMOPS reptest_spmm.c roofline.a -o reptest.x
	./reptest.x --verify --seconds_to_try_for_min=${SECONDS} --sweep=${SWEEP}

run: roofline_asm
	gcc ${CFLAGS} roofline.a src/reptest_spmm.c roofline.a -o reptest.x
	./reptest.x --verify --seconds_to_try_for_min=${SECONDS} --sweep=${SWEEP}

sparse_blis:
	gcc ${CFLAGS} sparse_blis.c -o sparse_blis.x
	./sparse_blis.x --verify
