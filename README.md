# our-smtp-proxy

网易企业邮箱在使用smtp发邮件时,对单IP的并发连接数有限制,不能超过20.

可能很多其它邮箱也都有同样的情况.

our-smtp-proxy要解决的就是这个问题.

# 解决办法

our-smtp-proxy根据transport.ini的配置,维护限定数量的smtp并发连接.

应用程序到our-smtp-proxy的并发连接数可根据情况自由掌控.

our-smtp-proxy接收到完整的发邮件请求后,选择一个smtp连接将邮件发送出去.

smtp服务器如果在发邮件过程中报错,比如发现收件人不存在,则our-smtp-proxy将报错信息返回给应用程序.

这里有一点很重要,our-smtp-proxy是同步发送邮件的,而不是异步的,这样邮件到底没有没发送成功,应用程序立即就会得到结果.
