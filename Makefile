CC=gcc
CFLAGS=-DNDEBUG -I"./include" -O3 -pedantic -Wall -c -fmessage-length=0 -std=gnu99 -fPIC
REDIS_HOME=$(CURDIR)
PHP_EXT_BUILD=$(REDIS_HOME)/php/build

libredis: src/alloc.o src/batch.o src/connection.o src/ketama.o src/md5.o src/module.o src/parser.o src/buffer.o
	mkdir -p lib
	gcc -shared -o"lib/libredis.so" ./src/alloc.o ./src/batch.o ./src/buffer.o ./src/connection.o ./src/ketama.o ./src/md5.o ./src/module.o ./src/parser.o -lm -lrt

php_ext:
	rm -rf $(PHP_EXT_BUILD)
	cp -a php/ext/redis $(PHP_EXT_BUILD)
	cp src/*.c $(PHP_EXT_BUILD)
	cp src/*.h $(PHP_EXT_BUILD)
	cd $(PHP_EXT_BUILD); phpize
	cd $(PHP_EXT_BUILD); ./configure --with-redis=$(REDIS_HOME)
	cd $(PHP_EXT_BUILD); sudo make install

clean:
	cd src; rm -rf *.o
	rm -rf lib
	rm -rf php/build
	-find . -name *.pyc -exec rm -rf {} \;
	-find . -name *.so -exec rm -rf {} \;
	-find . -name '*~' -exec rm -rf {} \;
	
