# This Makefile can be used on older platforms with CMake issues. If possible,
# please rely on standard CMake procedure.

CFLAGS=$(shell root-config --cflags)

all: libsdc.a

libsdc.a: sdc.o
	ar rcs libsdc.a $^

sdc.o: src/sdc.cc include/sdc-base.hh include/sdc.hh
	g++ -c $< -Iinclude $(CFLAGS)

include/sdc.hh: include/sdc.hh.in
	sed -e 's/#cmakedefine\ SDC_VERSION\ \"@SDC_VERSION@\"/#define SDC_VERSION "0.1"/g' \
	-e 's/# *cmakedefine01\ ROOT_FOUND/#define ROOT_FOUND\ 1/g' \
	include/sdc.hh.in > $@

clean:
	rm -f libsdc.a sdc.o include/sdc.hh

install: libsdc.a
	mkdir -p lib
	mv -v $< lib
	ln -s lib/libsdc.a $<

.PHONY: all clean
