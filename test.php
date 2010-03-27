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

function test_connection() {
	
	$connection = new Redis_Connection("127.0.0.1:6379");
	
	$batch = new Redis_Batch();
	$key = "piet";
    $batch->write("GET $key\r\n");	
	$batch->add_command();
	
	$connection->execute($batch);
	
	Redis_dispatch();
	
	$res = $batch->next_reply(&$reply_type, &$reply_value, &$reply_length);
	
	echo $reply_type, PHP_EOL;
	echo $reply_value, PHP_EOL;
	echo $reply_length, PHP_EOL;
	echo $res, PHP_EOL;
}

//test_ketama();
test_connection();

echo "done...!", PHP_EOL;
?>