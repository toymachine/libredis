CC=gcc
CFLAGS=-DNDEBUG -I"./include" -O3 -pedantic -Wall -c -fmessage-length=0 -std=gnu99 -fPIC
REDIS_HOME=$(CURDIR)
PHP_EXT_BUILD=$(REDIS_HOME)/php/build

UNAME := $(shell uname)

ifeq ($(UNAME), Linux)
 LIBS=-lm -lrt
endif
ifeq ($(UNAME), Darwin)
 LIBS=-lm
endif

libredis: src/batch.o src/connection.o src/ketama.o src/md5.o src/module.o src/parser.o src/buffer.o
	mkdir -p lib
	gcc -shared -o"lib/libredis.so" ./src/batch.o ./src/buffer.o ./src/connection.o ./src/ketama.o ./src/md5.o ./src/module.o ./src/parser.o $(LIBS)

php_ext:
	rm -rf $(PHP_EXT_BUILD)
	cp -a php/ext/libredis $(PHP_EXT_BUILD)
	cp src/*.c $(PHP_EXT_BUILD)
	cp src/*.h $(PHP_EXT_BUILD)
	cd $(PHP_EXT_BUILD); phpize
	cd $(PHP_EXT_BUILD); ./configure --with-libredis=$(REDIS_HOME)
	cd $(PHP_EXT_BUILD); sudo make install

c_test: libredis test.o
	gcc -o test test.o -Llib -lredis
	@echo "!! executing test, make sure you have redis running locally at 127.0.0.1:6379 !!"
	LD_LIBRARY_PATH=lib ./test
	
clean:
	cd src; rm -rf *.o
	rm -rf lib
	rm -rf php/build
	-find . -name *.pyc -exec rm -rf {} \;
	-find . -name *.so -exec rm -rf {} \;
	-find . -name '*~' -exec rm -rf {} \;
	
