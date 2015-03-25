# our-smtp-proxy

网易企业邮箱在使用smtp发邮件时,对单IP的并发连接数有限制,不能超过20.

可能很多其它邮箱也都有同样的情况.

our-smtp-proxy要解决的就是这个问题.

# 解决办法

our-smtp-proxy根据transport.ini的配置,维护限定数量的smtp并发连接.

应用程序到our-smtp-proxy的并发连接数可根据情况自由掌控.

our-smtp-proxy接收到完整的发邮件请求后,选择一个smtp连接将邮件发送出去.

smtp服务器如果在发邮件过程中报错,比如发现收件人不存在,则our-smtp-proxy将报错信息返回给应用程序.

这里有一点很重要,**our-smtp-proxy是同步发送邮件的,而不是异步的,这样邮件到底没有没发送成功,应用程序立即就会得到结果.**

# MAX_CLIENTS

MAX_CLIENTS 的数量主要受限于进程的 max open files (ulimit -n) 和内存

由于proxy会接收下完整的邮件后再选择一个smtp连接将邮件发送出去,所以如果一封邮件大小5M,并发100个连接的话,至少就需要500M的内存.

proxy启动后, stdin, stdout, stderr, logFile, proxyfd, monitorfd 这6个fd已经使用了.

当使用monitor功能时,还需要占用1个fd.

每个tp的最大连接数(transport.ini maxConn)是可以确定的.

这样最终 **MAX_CLIENTS = (ulimit -n) - 6 - 1 - tp1maxConn - tp2maxConn - ...**

另外:
如果可用的smtp并发连接太少,而client很多,显然在getAnIdleConn时必然有一些会超时,这时就会返回 **500 proxy too busy, try later**,所以终极解决办法还是加大smtp的并发连接数.


# Transport.php stream_set_timeout

一个连接最长的超时时间这样确定:

    proxyfd accept MAX_WAIT     10s
    tp getAnIdleConn TIMEOUT    90s
    smtp initial 220 Message    300s
    smtp EHLO                   300s
    smtp AUTH LOGIN             60s
    smtp username               60s
    smtp password               60s
    smtp MAIL FROM              300s
    smtp RCPT TO                300s
    smtp DATA                   120s
    smtp mail header + body     600s
    smtp RSET                   60s
    total:                      2260s

实际情况的话, 如果10分钟一封邮件还没有发出去,那就有问题了. **所以Transport.php里我们设600s timeout.**
