REDIS_HOME=/home/henk/workspace/libredis

php_ext:
	rm -rf /tmp/libredis_php
	cp -a php/ext/redis /tmp/libredis_php
	cd /tmp/libredis_php; phpize
	cd /tmp/libredis_php; ./configure --with-redis=$(REDIS_HOME)
	cd /tmp/libredis_php; sudo make install

php_fast:
	cp -a php/ext/redis /tmp/libredis_php
	cd /tmp/libredis_php; sudo make install

php_test: php_fast
	php test.php
