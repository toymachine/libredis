CC=gcc
CFLAGS=-DNDEBUG -I"./include" -O3 -pedantic -Wall -c -fmessage-length=0 -std=gnu99 -fPIC
REDIS_HOME=$(CURDIR)

php_ext:
	rm -rf /tmp/libredis_php
	cp -a php/ext/redis /tmp/libredis_php
	cd /tmp/libredis_php; phpize
	cd /tmp/libredis_php; ./configure --with-redis=$(REDIS_HOME)
	cd /tmp/libredis_php; sudo make install

use_debug:
	rm -rf lib/libredis.so
	mkdir -p lib
	cd Debug; make clean; make
	cd lib; ln -s ../Debug/libredis.so libredis.so

use_release:
	rm -rf lib/libredis.so
	mkdir -p lib
	cd Release; make clean; make
	cd lib; ln -s ../Release/libredis.so libredis.so

libredis: src/alloc.o src/batch.o src/connection.o src/ketama.o src/md5.o src/module.o src/parser.o src/buffer.o
	mkdir -p lib
	gcc -shared -o"lib/libredis.so" ./src/alloc.o ./src/batch.o ./src/buffer.o ./src/connection.o ./src/ketama.o ./src/md5.o ./src/module.o ./src/parser.o   -levent -lm

clean:
	cd src; rm -rf *.o
	rm -rf lib
	
