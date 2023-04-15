DOXYGEN_EXEC=doxygen
SPHINX_EXEC=sphinx-build

BUILD_DIR=build

all: build/index.html

build/xml/index.xml: ./Doxyfile ../include/sdc-base.hh
	$(DOXYGEN_EXEC) $<

build/index.html: build/xml/index.xml \
				  conf.py \
				  index.rst \
				  tutorial.rst \
				  grammar.rst \
				  api.rst
	$(SPHINX_EXEC) -b html \
		-Dbreathe_projects.na64sw=$(BUILD_DIR)/xml \
		. build

clean:
	rm -rf build/xml/*
	rm -rf build/*

.PHONY: all clean

