LIBS := -lrt -lpthread

all: printer client

printer: printer.c
	gcc -g $^ -o $@ $(LIBS)

client: client.c
	gcc -g $^ -o $@ $(LIBS)

clean:
	-rm printer client
