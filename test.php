<?php

function test_ketama() {
	$ketama = new Redis_Ketama();
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
}


function get_connection($addr)
{
	if(!isset($connections[$addr])) {
		$connection[$addr] = new Redis_Connection($addr);
	} 
	return $connection[$addr];	
}

function mget($keys, $ketama) {
	$connections = array();
	$batches = array();
	$batch_keys = array();
    //add all keys to batches
    foreach($keys as $key) {
        $server_ordinal = $ketama->get_server($key);
        if(!isset($batches[$server_ordinal])) {
            $batch = new Redis_Batch();
            $batch->write("MGET");
            $batches[$server_ordinal] = $batch;
    	}
    	else {
    		$batch = $batches[$server_ordinal];
		}
        $batch->write(" $key");
        $batch_keys[$server_ordinal][] = $key;
    }
    //finalize batches, and start executing
    foreach($batches as $server_ordinal=>$batch) {
        $batch->write("\r\n");
        $batch->finalize(1);
        $server_addr = $ketama->get_server_addr($server_ordinal);
		if(!isset($connections[$server_addr])) {
			$connections[$server_addr] = new Redis_Connection($server_addr);
		} 
		$connection = $connections[$server_addr];	
        $connection->execute($batch);
    }
    //handle events until all complete
	Redis_dispatch();
    //build up results
    $results = array();
    foreach($batches as $server_ordinal=>$batch) {
    	//read multibulk reply
	    $batch->next_reply(&$mb_reply_type, &$mb_reply_value, &$mb_reply_length);
	    foreach($batch_keys[$server_ordinal] as $key) {
    	    $batch->next_reply(&$reply_type, &$reply_value, &$reply_length);
    	    $results[$key] = $reply_value;
		}
	}
	return $results;
}

function test_mget()
{
    $ketama = new Redis_Ketama();
    $ketama->add_server("127.0.0.1", 6379, 300);
    $ketama->add_server("127.0.0.1", 6380, 300);
    $ketama->create_continuum();

	print_r(mget(array("piet1", "piet2", "piet3", "piet4", "xx3"), $ketama));
}

function _test_simple($connection)
{
	$batch = new Redis_Batch();
	$key = "piet";
    $batch->write("GET $key\r\n");	
    $batch->write("MGET $key $key\r\n");
    $batch->write("GET $key\r\n");	
	$batch->finalize(3);
	
	$connection->execute($batch);
	
	Redis_dispatch();
	
	while($level = $batch->next_reply(&$reply_type, &$reply_value, &$reply_length)) {
		echo "start", PHP_EOL;
		echo "\ttype ", $reply_type, PHP_EOL;
		echo "\tval ", $reply_value, PHP_EOL;
		echo "\tlen ", $reply_length, PHP_EOL;
		echo "\tlevel ", $level, PHP_EOL;
		echo "end", PHP_EOL;
	}
}

function test_simple() {
	
	$connection = new Redis_Connection("127.0.0.1:6379");
	
	for($i = 0; $i < 1; $i++) {
		_test_simple($connection);
	}
}

//test_ketama();
//test_simple();
test_mget();

echo "done...!", PHP_EOL;
?>