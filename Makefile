OBJFILES = serport.o tty.o
CC = gcc

serport: $(OBJFILES)
	$(CC) -pthread -o $@ $^

/usr/local/bin/serport: serport
	sudo cp serport /usr/local/bin

.PHONY : install
install: /usr/local/bin/serport

.PHONY : clean
clean :
	rm serport $(OBJFILES)
