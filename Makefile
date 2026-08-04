CFLAGS := -g -DDEBUG -lm

SECONDS := 2
SWEEP   := both

roofline_asm:
	nasm -f elf64 -o roofline.o roofline.asm
	ar rcs roofline.a roofline.o

observe: roofline_asm
	gcc ${CFLAGS} -O0 -DOBSERVE_FLOPS -DOBSERVE_MEMOPS reptest_spmm.c roofline.a -o reptest.x

observe_run: observe
	gcc ${CFLAGS} -O0 -DOBSERVE_FLOPS -DOBSERVE_MEMOPS reptest_spmm.c roofline.a -o reptest.x
	./reptest.x --verify --seconds_to_try_for_min=${SECONDS} --sweep=${SWEEP}

no_observe:
	gcc ${CFLAGS} -O3 reptest_spmm.c -o reptest.x

no_observe_run: no_observe
	./reptest.x --verify --seconds_to_try_for_min=${SECONDS} --sweep=${SWEEP}

sparse_blis:
	gcc ${CFLAGS} -O3 -fopenmp sparse_blis.c -o sparse_blis.x

sparse_blis_run: sparse_blis
	./sparse_blis.x --left_constant_blocking=MAT_CSC --right_constant_blocking=MAT_DENSE --dummy_solution

taco_bullshit:
	gcc ${CFLAGS} -O3 -fopenmp taco_bench.c -o taco_bench.x

taco_bullshit_run: taco_bullshit
	./taco_bench.x
