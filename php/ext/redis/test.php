<?php

echo hello_world();

function test_ketama() {
	$ketama = new Ketama();
    $ketama->add_server("10.0.1.1", 11211, 600);
    $ketama->add_server("10.0.1.2", 11211, 300);
    $ketama->add_server("10.0.1.3", 11211, 200);
    $ketama->add_server("10.0.1.4", 11211, 350);
    $ketama->add_server("10.0.1.5", 11211, 1000);
    $ketama->add_server("10.0.1.6", 11211, 800);
    $ketama->add_server("10.0.1.7", 11211, 950);
    $ketama->add_server("10.0.1.8", 11211, 100);
}

test_ketama();

?>