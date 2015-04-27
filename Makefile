all: shared static

shared:
	gcc -g -c -fpic privsep_malloc.c
	gcc -g -shared -o libprivsep_malloc.so privsep_malloc.o
	cp libprivsep_malloc.so lib
	cp libprivsep_malloc.h include

static:
	gcc -g -c privsep_malloc.c
	ar rs libprivsep_malloc.a privsep_malloc.o
	cp libprivsep_malloc.a lib
	cp libprivsep_malloc.h include
.PHONY: test
test:
	$(MAKE) -C test
clean:
	rm -f *.a *.o *.so lib/* include/* test/test_lib_shared test/test_lib_static
