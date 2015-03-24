<?php

require_once 'Zend/Loader/Autoloader.php';
Zend_Loader_Autoloader::getInstance();

include __DIR__ . '/Transport.php';

function sendMail($pid, $totalSend, $transport) {
    $tp = new mySmtpTransport('tcp://127.0.0.1:9925');
    $to = array(
        'employee004@126.com',
        'heguangyu5@sina.com',
        'employee001@sohu.com'
    );
    $count = count($to);

    while ($totalSend) {
        $subject = "测试our-smtp-proxy($pid, $totalSend)";
        echo $subject, "\n";

        $mail = new Zend_Mail('UTF-8');
        $mail->setFrom($transport);
        $c = mt_rand(1, $count);
        for ($i = 0; $i < $c; $i++) {
            $mail->addTo($to[$i]);
        }
        $mail->setSubject($subject);
        $subject .= "\n";
        $mail->setBodyText(str_repeat($subject, $totalSend));
        $tp->send($mail);

        $totalSend--;

        // 每发一封信,停0-5秒,模拟实际情况
        sleep(mt_rand(0, 5));
    }
}

if ($argc != 4) {
    echo "Usage: php test.php transport num-children num-send-per-child\n";
    exit(1);
}

if (!is_numeric($argv[2]) || $argv[2] < 1) {
    exit(1);
}
if (!is_numeric($argv[3]) || $argv[3] < 1) {
    exit(1);
}

$transport     = $argv[1];
$totalChildren = $argv[2];
$sendPerChild  = $argv[3];

$children = array();
for ($i = 0; $i < $totalChildren; $i++) {
    $pid = pcntl_fork();
    if ($pid > 0) {
        $children[] = $pid;
    } else if ($pid == 0) {
        $pid = posix_getpid();
        echo "$pid started\n";
        sleep(10); // wait for all children started
        try {
            sendMail($pid, $sendPerChild, $transport);
        } catch (Exception $e) {
            echo $pid, ': ', $e->getMessage(), "\n";
        }
        exit;
    }
}

foreach ($children as $pid) {
    pcntl_waitpid($pid, $status);
    echo $pid, " end\n";
}
