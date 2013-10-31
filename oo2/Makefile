all: sqrt prodcons

list: list/list.o
	cd list; make

sqrt: sqrt.o
	gcc -o $@ sqrt.o -lpthread -lm

prodcons: list prodcons.o
	gcc -o $@ list/list.o prodcons.o -lpthread -ggdb

clean:
	rm sqrt.o* 
	rm sqrt
