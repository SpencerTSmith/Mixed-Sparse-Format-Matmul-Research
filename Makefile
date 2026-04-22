CFLAGS := -g -DDEBUG -lm

SECONDS := 1
SWEEP   := both

roofline_asm:
	nasm -f elf64 -o roofline.o roofline.asm
	ar rcs roofline.a roofline.o

observe: roofline_asm
	gcc ${CFLAGS} -O0 -DOBSERVE_FLOPS -DOBSERVE_MEMOPS reptest_spmm.c roofline.a -o reptest.x
	./reptest.x --verify --seconds_to_try_for_min=${SECONDS} --sweep=${SWEEP}

run: roofline_asm
	gcc ${CFLAGS} -03 roofline.a reptest_spmm.c roofline.a -o reptest.x
	./reptest.x --verify --seconds_to_try_for_min=${SECONDS} --sweep=${SWEEP}

sparse_blis:
	gcc ${CFLAGS} sparse_blis.c -o sparse_blis.x
	./sparse_blis.x --verify
