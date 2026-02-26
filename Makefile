CFLAGS := -g -DDEBUG -O0 -lm

roofline_asm:
	nasm -f elf64 -o roofline.o roofline.asm
	ar rcs roofline.a roofline.o

observe: roofline_asm
	gcc ${CFLAGS} -DOBSERVE_FLOPS -DOBSERVE_MEMOPS reptest_spmm.c roofline.a -o reptest.x
	./reptest.x 3 16 16 256 verify

run: roofline_asm
	gcc ${CFLAGS} roofline.a src/reptest_spmm.c roofline.a -o reptest.x
	./reptest.x 3 16 16 256 verify
