#
#
# If you have rhel/centos/rocky/fedora
# 12th Gen Intel(R) Core(TM) i7-12800H


build: mkl_conv.c
	gcc -std=c99 -m64  mkl_conv.c -I"${MKLROOT}/include"  -m64  -Wl,--start-group ${MKLROOT}/lib/libmkl_intel_lp64.a ${MKLROOT}/lib/libmkl_sequential.a ${MKLROOT}/lib/libmkl_core.a -Wl,--end-group -lpthread -lm -ldl -static -o ./run.x

clean:
	rm -f ./run.x *~
