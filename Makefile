all: our-smtp-proxy test-smtp test-pthread-dllist test-dllist

our-smtp-proxy: ini.o pthread_dllist.o dllist.o log.o rcpt.o smtp.o main.o transport.o client.o report.o
	gcc -Wall -Wl,--no-as-needed -pthread -lrt -o our-smtp-proxy ini.o pthread_dllist.o dllist.o log.o rcpt.o smtp.o main.o transport.o client.o report.o

test-smtp: testSmtp.o log.o rcpt.o smtp.o
	gcc -Wall -o test-smtp testSmtp.o log.o rcpt.o smtp.o

test-pthread-dllist: testPthreadDllist.o pthread_dllist.o
	gcc -Wall -Wl,--no-as-needed -pthread -lrt -o test-pthread-dllist testPthreadDllist.o pthread_dllist.o

test-dllist: testDllist.o dllist.o
	gcc -Wall -o test-dllist testDllist.o dllist.o

main.o: main.c
	gcc -Wall -c main.c

transport.o: transport.h transport.c
	gcc -Wall -c transport.c

ini.o: ini.h ini.c
	gcc -Wall -c ini.c

log.o: log.h log.c
	gcc -Wall -c log.c

client.o: client.h client.c
	gcc -Wall -c client.c

rcpt.o: rcpt.h rcpt.c
	gcc -Wall -c rcpt.c

smtp.o: smtp.h smtp.c
	gcc -Wall -c smtp.c

testSmtp.o: testSmtp.c
	gcc -Wall -c testSmtp.c

pthread_dllist.o: pthread_dllist.h pthread_dllist.c
	gcc -Wall -c pthread_dllist.c

testPthreadDllist.o: testPthreadDllist.c
	gcc -Wall -c testPthreadDllist.c

dllist.o: dllist.h dllist.c
	gcc -Wall -c dllist.c

testDllist.o: testDllist.c
	gcc -Wall -c testDllist.c

report.o: report.c
	gcc -Wall -c report.c

clean:
	rm *.o our-smtp-proxy test-smtp test-pthread-dllist test-dllist
