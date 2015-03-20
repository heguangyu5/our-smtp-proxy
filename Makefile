all: main test-smtp test-dllist

main: ini.o dllist.o log.o rcpt.o smtp.o main.o transport.o client.o
	gcc -Wall -Wl,--no-as-needed -pthread -lrt -o our-smtp-proxy ini.o dllist.o log.o rcpt.o smtp.o main.o transport.o client.o

test-smtp: testSmtp.o log.o rcpt.o smtp.o
	gcc -Wall -o test-smtp testSmtp.o log.o rcpt.o smtp.o

test-dllist: testDllist.o dllist.o
	gcc -Wall -Wl,--no-as-needed -pthread -lrt -o test-dllist testDllist.o dllist.o

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
