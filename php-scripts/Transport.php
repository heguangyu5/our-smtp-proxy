<?php

class mySmtpTransport extends Zend_Mail_Transport_Abstract
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
