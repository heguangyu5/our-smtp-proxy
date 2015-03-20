all: main test-smtp test-dllist

main: main.o transport.o ini.o log.o client.o rcpt.o smtp.o
	gcc -Wall -pthread -o our-smtp-proxy main.o transport.o ini.o log.o client.o rcpt.o smtp.o

test-smtp: testSmtp.o log.o rcpt.o smtp.o
	gcc -Wall -o test-smtp testSmtp.o log.o rcpt.o smtp.o

test-dllist: testDllist.o dllist.o
	gcc -Wall -pthread -o test-dllist testDllist.o dllist.o

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

dllist.o: dllist.h dllist.c
	gcc -Wall -c dllist.c

testDllist.o: testDllist.c
	gcc -Wall -c testDllist.c

clean:
	rm *.o our-smtp-proxy test-smtp test-dllist
