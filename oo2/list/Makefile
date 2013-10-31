all: fifo

OBJS = list.o main.o 
LIBS= -lpthread

fifo: main.o ${OBJS}
	gcc -o $@ ${OBJS} ${LIBS} -ggdb

clean:
	rm -rf *o fifo
