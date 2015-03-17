main: main.o transport.o ini.o message.o client.o rcpt.o smtp.o
	gcc -Wall -pthread -g -o our-smtp-proxy main.o transport.o ini.o message.o client.o rcpt.o smtp.o

main.o: main.c
	gcc -Wall -c main.c

transport.o: transport.h transport.c
	gcc -Wall -c transport.c

ini.o: ini.h ini.c
	gcc -Wall -c ini.c

message.o: message.h message.c
	gcc -Wall -c message.c

client.o: client.h client.c
	gcc -Wall -c client.c

rcpt.o: rcpt.h rcpt.c
	gcc -Wall -c rcpt.c

smtp.o: smtp.h smtp.c
	gcc -Wall -c smtp.c

clean:
	rm *.o our-smtp-proxy
