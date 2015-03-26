<?php

$fp = stream_socket_client("tcp://127.0.0.1:9926");
echo fread($fp, 4096);
fclose($fp);
