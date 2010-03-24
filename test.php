<?php

function mget($aKeys, $module, $ketama) {
    $batches = array();
    $batch_keys = array();
    foreach($aKeys as $key) {
        $ordinal = $ketama->get_server($key);
        if(!isset($batches[$ordinal])) {
	        $batch = Redis_Batch();
            $batch->add_command();
            $batch->write("MGET");
            $batches[$ordinal] = $batch;
        }
        else {
        	$batch = $batches[$ordinal];
        }
        $batch->write(" %s", $key);
        $batch_keys[$ordinal][] = $key;
    }
    //finalize batches, and start executing
    foreach($batches as $ordinal=>$batch) {
        $batch->write("\r\n");
        $addr = $ketama->get_server_addr($ordinal);
        $connection = Redis_Connection($addr);
        $connection->execute($batch, false);
    }
    //handle events until all complete
    $module->dispatch();
    //build up results
    $results = array();
    foreach($batches as $ordinal=>$batch) {
        //only expect 1 (multibulk) reply per batch
        $reply = $batch->pop_reply();
        foreach($batch_keys[$ordinal] as $key) {
            $child = $reply->pop_child();
            $results[$key] = $child->get_value();
        }
	}
    return $results;
}

$module = Redis_Module();
$ketama = Redis_Ketama();

print_r(mget(array('piet1', 'piet2'), $module, $ketama));

?>

