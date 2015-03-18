<?php

function sendMail($pid) {
    $fp = stream_socket_client("tcp://127.0.0.1:9925", $errno, $errstr, 30);
    if (!$fp) {
        echo "$errstr ($errno)\n";
        exit;
    }

    $i = 1;
    while ($i < 2) {
        fwrite($fp, "employee001@126.com\r\n");
        fwrite($fp, "employee003@126.com\r\n");
        fwrite($fp, "employee004@126.com\r\n");
        fwrite($fp, "DATA\r\n");
        fwrite($fp, "$pid($i): 123456789123456789123456789123456789123456789\r\n");
        fwrite($fp, ".\r\n");
        //sleep(mt_rand(1, 5));
        echo $pid . "($i): " . fgets($fp);
        $i++;
    }
}

sendMail(posix_getpid());
exit;

$children = array();

for ($i = 0; $i < 10; $i++) {
    $pid = pcntl_fork();
    if ($pid > 0) {
        $children[] = $pid;
    } else if ($pid == 0) {
        $pid = posix_getpid();
        sendMail($pid);
        exit;
    }
}

foreach ($children as $pid) {
    pcntl_waitpid($pid, $status);
    echo $pid, " end\n";
}
