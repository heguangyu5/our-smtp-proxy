<?php

function sendMail() {
    $fp = stream_socket_client("tcp://127.0.0.1:9925", $errno, $errstr, 30);
    if (!$fp) {
        echo "$errstr ($errno)\n";
        exit;
    }

    for ($i = 1; $i < 3; $i++) {
        fwrite($fp, "$i: 123456789");
        fwrite($fp, "123456789");
        fwrite($fp, "123456789");
        fwrite($fp, "123456789");
        fwrite($fp, "123456789");
        fwrite($fp, ".\r\n");
    }

    sleep(mt_rand(5, 10));
}

$children = array();

for ($i = 0; $i < 10; $i++) {
    $pid = pcntl_fork();
    if ($pid > 0) {
        $children[] = $pid;
    } else if ($pid == 0) {
        sendMail();
        exit;
    }
}

foreach ($children as $pid) {
    pcntl_waitpid($pid, $status);
    echo $pid, " end\n";
}
