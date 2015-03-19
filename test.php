<?php

require_once 'Zend/Loader/Autoloader.php';
Zend_Loader_Autoloader::getInstance();

class mySmtp extends Zend_Mail_Transport_Abstract
{
    protected $fp;

    public function __construct($remoteSocket)
    {
        $this->fp = stream_socket_client($remoteSocket, $errno, $errstr, 30);
        if (!$this->fp) {
            throw new Zend_Mail_Transport_Exception("cannot connect to $remoteSocket, $errstr ($errno)");
        }
    }

    protected function _sendMail()
    {
        $msg = array();
        $msg[] = $this->_mail->getReturnPath();
        foreach ($this->_mail->getRecipients() as $recipient) {
            $msg[] = $recipient;
        }
        $msg[] = 'DATA';
        $msg[] = $this->header;
        $msg[] = str_replace("\n.\r\n", "\n..\r\n", $this->body);
        $msg[] = ".\r\n";
        $msg = implode("\r\n", $msg);

        if (fwrite($this->fp, $msg) === false) {
            throw new Zend_Mail_Transport_Exception('send msg error');
        }

        stream_set_timeout($this->fp, 600);
        $response = fgets($this->fp, 1024);

        $info = stream_get_meta_data($this->fp);
        if (!empty($info['timed_out'])) {
            throw new Zend_Mail_Transport_Exception('read response timed out');
        }
        if ($response === false) {
            throw new Zend_Mail_Transport_Exception('read response error');
        }
        if (strncmp("250", $response, 3) != 0) {
            throw new Zend_Mail_Transport_Exception(substr($response, 4));
        }
    }

    // @see Zend_Mail_Transport_Smtp
    protected function _prepareHeaders($headers)
    {
        if (!$this->_mail) {
            throw new Zend_Mail_Transport_Exception('_prepareHeaders requires a registered Zend_Mail object');
        }

        unset($headers['Bcc']);
        parent::_prepareHeaders($headers);
    }
}

function sendMail($pid, $totalSend, $transport) {
    $tp = new mySmtp('tcp://127.0.0.1:9925');
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
        sendMail($pid, $sendPerChild, $transport);
        exit;
    }
}

foreach ($children as $pid) {
    pcntl_waitpid($pid, $status);
    echo $pid, " end\n";
}
