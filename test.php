<?php

define("RT_ERROR", -1);
define("RT_NONE", 0);
define("RT_OK", 1);
define("RT_BULK_NIL", 2);
define("RT_BULK", 3);
define("RT_MULTIBULK_NIL", 4);
define("RT_MULTIBULK", 5);
define("RT_INTEGER", 6);

$ip = "127.0.0.1";
//$ip = "192.168.13.92";

$libredis = Libredis();

function test_ketama() {
    global $libredis;
    $ketama = $libredis->create_ketama();
    $ketama->add_server("10.0.1.1", 11211, 600);
    $ketama->add_server("10.0.1.2", 11211, 300);
    $ketama->add_server("10.0.1.3", 11211, 200);
    $ketama->add_server("10.0.1.4", 11211, 350);
    $ketama->add_server("10.0.1.5", 11211, 1000);
    $ketama->add_server("10.0.1.6", 11211, 800);
    $ketama->add_server("10.0.1.7", 11211, 950);
    $ketama->add_server("10.0.1.8", 11211, 100);
    
    $ketama->create_continuum();
    $ordinal = $ketama->get_server("piet5234");
	echo "ord: ", $ordinal, PHP_EOL;
	echo "addr: ", $ketama->get_server_addr($ordinal), PHP_EOL;
	
	//test speed of creating continuum:
    $start = microtime(true);
	$N = 1000;
	for($j = 0; $j < $N; $j++) {
    	$ketama = $libredis->create_ketama();
    	$M = 100;
    	for($i = 0; $i < $M; $i++) {
    	   $ketama->add_server("10.0.1.$i", 11211, 100);
    	}
        $ketama->create_continuum();
	}
	
    $end = microtime(true);
    echo "speed creating $N times a continuum for $M servers: ", ($end - $start) / $N, PHP_EOL;
}


function mget($keys, $ketama) {
    global $libredis;
	$batches = array();
	$batch_keys = array();
    //add all keys to batches
    foreach($keys as $key) {
        $server_ordinal = $ketama->get_server($key);
        if(!isset($batches[$server_ordinal])) {
            $batch = $libredis->create_batch("MGET");
            $batches[$server_ordinal] = $batch;
    	}
    	else {
    		$batch = $batches[$server_ordinal];
		}
        $batch->write(" $key");
        $batch_keys[$server_ordinal][] = $key;
    }
    //finalize batches, add them to executor
    $executor = $libredis->create_executor();
    foreach($batches as $server_ordinal=>$batch) {
        $batch->write("\r\n", 1);
        $server_addr = $ketama->get_server_addr($server_ordinal);
		$connection = $libredis->get_connection($server_addr);	
        $executor->add($connection, $batch);
    }
    //execute in parallel until all complete
	$executor->execute(500);
    //build up results
    $results = array();
    foreach($batches as $server_ordinal=>$batch) {
    	//read the single multibulk reply
	    $batch->next_reply(&$mb_reply_type, &$mb_reply_value, &$mb_reply_length);
	    if(RT_ERROR != $mb_reply_type) {
	        //read the individual bulk replies in the multibulk reply
    	    foreach($batch_keys[$server_ordinal] as $key) {
        	    $batch->next_reply(&$reply_type, &$reply_value, &$reply_length);
        	    if(RT_BULK == $reply_type) {
        	       $results[$key] = $reply_value;
        	    }
    		}
	    }
	}
	
	return $results;
}

function test_mget()
{
    global $libredis; 
    global $ip;
    $ketama = $libredis->create_ketama();
    $ketama->add_server($ip, 6379, 100);
    $ketama->add_server($ip, 6380, 100);
    $ketama->create_continuum();

    //$N = 200000;
    $N = 2000;
    $M = 200;
    
    $connection1 = $libredis->get_connection("$ip:6379");;
    $connection2 = $libredis->get_connection("$ip:6380");
    $keys = array();
    for($i = 0; $i < $M; $i++) {
        $key = "piet$i";
        $value = "blaat$i";
        $connection1->execute($libredis->create_batch()->set($key, $value));
        $connection2->execute($libredis->create_batch()->set($key, $value));
        $keys[] = $key;
    }
    
    $start = microtime(true);
    for($i = 0; $i < $N; $i++) {
        mget($keys, $ketama);
        //print_r(mget($keys, $ketama));
        //echo $i, PHP_EOL;
    }
    $end = microtime(true);
    echo "per/sec ", $N * $M / ($end - $start), PHP_EOL;
    echo "msec per mget ", ($end - $start) / $N * 1000.0, PHP_EOL;
}

function test_simple() {
	
    global $libredis;
    global $ip;
	
    $connection = $libredis->get_connection("$ip:6379");
	$key = 'library';
	
	for($i = 0; $i < 1; $i++) {
        $batch = $libredis->create_batch("GET $key\r\n", 1);
        $executor = $libredis->create_executor();
        $executor->add($connection, $batch);
        $executor->execute(400);
        $batch->next_reply(&$reply_type, &$reply_value, &$reply_length);
		print_r($reply_value);
	}
}

function test_destroy() {
    global $libredis;
    $connection = $libredis->get_connection("$ip:6379");
    $connection = $libredis->get_connection("$ip:6379");
    $connection = $libredis->get_connection("$ip:6379");
}

function test_integer_reply()
{
    global $libredis;
    global $ip;
	$connection = $libredis->get_connection("$ip:6379");
	$batch = $libredis->create_batch("INCR incr_test\r\n", 1);
	$connection->execute($batch);
	while($level = $batch->next_reply(&$reply_type, &$reply_value, &$reply_length)) {
		echo "start", PHP_EOL;
		echo "\ttype ", $reply_type, PHP_EOL;
		echo "\tval ", $reply_value, " (", gettype($reply_value), ")", PHP_EOL;
		echo "\tlen ", $reply_length, PHP_EOL;
		echo "\tlevel ", $level, PHP_EOL;
		echo "end", PHP_EOL;
	}
}

function test_connections()
{
    global $libredis;
    for($i = 0; $i < 10; $i++) {    
        $connection1 = $libredis->get_connection("$ip:6379");
        $connection2 = $libredis->get_connection("$ip:6380");
    }
}

function test_convenience()
{
    global $libredis;
    global $ip;
    $batch = $libredis->create_batch();
    $batch->set("blaat", "aap");
    $batch->set("joop", "blaat");
    $connection = $libredis->get_connection("$ip:6379");
    $connection->execute($batch);
    while($batch->next_reply(&$reply_type, &$reply_value, &$reply_length)) {
        echo $reply_value, PHP_EOL;    
    }
    $batch = $libredis->create_batch();
    $batch->get("blaat");
    $batch->get("joop");
    $connection->execute($batch);
    while($batch->next_reply(&$reply_type, &$reply_value, &$reply_length)) {
        echo $reply_value, PHP_EOL;    
    }
}

//test_ketama();
//test_simple();
test_mget();
//test_destroy();
//test_integer_reply();
//test_connections();
//test_convenience();
//echo "done...!", PHP_EOL;

?>
