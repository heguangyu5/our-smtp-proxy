<?php

class mySmtpTransport extends Zend_Mail_Transport_Abstract
{
    public $EOL = "\n";
    protected $fp;

    public function __construct($remoteSocket)
    {
        $this->fp = stream_socket_client($remoteSocket, $errno, $errstr, 30);
        if (!$this->fp) {
            throw new Zend_Mail_Transport_Exception("cannot connect to $remoteSocket, $errstr ($errno)");
        }
    }

    public function sendRawMail($from, $to, $rawHeader, $rawBody)
    {
        // 将header和body里的\n换成\r\n,一些smtp服务器,比如qmail,不接受\n换行的邮件
        // @see http://cr.yp.to/docs/smtplf.html
        $rawHeader = trim($rawHeader);
        $rawHeader = str_replace("\r\n", "\n", $rawHeader);
        $rawHeader = str_replace("\n", "\r\n", $rawHeader);

        $rawBody = trim($rawBody);
        $rawBody = str_replace("\r\n", "\n", $rawBody);
        $rawBody = str_replace("\n.", "\n..", $rawBody); // escape line begin with .
        $rawBody = str_replace("\n", "\r\n", $rawBody);

        $msg = array($from);
        if (is_array($to)) {
            foreach ($to as $recipient) {
                $msg[] = $recipient;
            }
        } else {
            $msg[] = $to;
        }
        $msg[] = 'DATA';
        $msg[] = $rawHeader . "\r\n";
        $msg[] = $rawBody . "\r\n.\r\n";
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

    protected function _sendMail()
    {
        $this->sendRawMail(
            $this->_mail->getReturnPath(),
            $this->_mail->getRecipients(),
            $this->header,
            $this->body
        );
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