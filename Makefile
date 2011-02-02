CC=gcc
CFLAGS=-DNDEBUG -I"./include" -O3 -pedantic -Wall -c -fmessage-length=0 -std=gnu99 -fPIC -fvisibility=hidden
REDIS_HOME=$(CURDIR)
PHP_EXT_BUILD=$(REDIS_HOME)/php/build

UNAME := $(shell uname)

ifeq ($(UNAME), Linux)
 LIBS=-lm -lrt
endif
ifeq ($(UNAME), Darwin)
 LIBS=-lm
endif

ifdef SINGLETHREADED
 CFLAGS += -DSINGLETHREADED
endif

libredis: libredis/batch.o libredis/connection.o libredis/ketama.o libredis/md5.o libredis/module.o libredis/parser.o libredis/buffer.o
	mkdir -p lib
	gcc -shared -o "lib/libredis.so" ./libredis/batch.o ./libredis/buffer.o ./libredis/connection.o ./libredis/ketama.o ./libredis/md5.o ./libredis/module.o ./libredis/parser.o $(LIBS)

php_ext:
	rm -rf $(PHP_EXT_BUILD)
	cp -a php/ext/libredis $(PHP_EXT_BUILD)
	cp libredis/*.c $(PHP_EXT_BUILD)
	cp libredis/*.h $(PHP_EXT_BUILD)
	cd $(PHP_EXT_BUILD); phpize
	cd $(PHP_EXT_BUILD); ./configure --with-libredis=$(REDIS_HOME)
	cd $(PHP_EXT_BUILD); sudo make install

c_test: libredis test.o
	gcc -o test test.o -Llib -lredis
	@echo "!! executing test, make sure you have redis running locally at 127.0.0.1:6379 !!"
	LD_LIBRARY_PATH=lib ./test
	
clean:
	cd libredis; rm -rf *.o
	rm -rf lib
	rm -rf php/build
	rm -rf test
	rm -rf test.o
	-find . -name *.pyc -exec rm -rf {} \;
	-find . -name *.so -exec rm -rf {} \;
	-find . -name '*~' -exec rm -rf {} \;
	
