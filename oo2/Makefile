all: sqrt prodcons

list: list/list.o
	cd list; make

sqrt: sqrt.o
	gcc -o $@ sqrt.o -lpthread -lm

prodcons: list prodcons.o
	gcc -o $@ list/list.o prodcons.o -lpthread -ggdb

report:
	pdflatex report.tex

clean:
	rm -f sqrt.o* 
	rm -f sqrt
	rm -f report.aux
	rm -f report.out
	rm -f report.log
	rm -f report.pdf
