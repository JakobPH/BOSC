
all: bosh

OBJS = parser.o print.o
LIBS= -lreadline -ltermcap
CC = gcc

bosh: bosh.o ${OBJS}
	${CC} -o $@  bosh.o ${OBJS}  ${LIBS}

clean:
	rm -rf *o bosh
