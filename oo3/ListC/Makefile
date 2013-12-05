all: listmachine

listmachine: listmachine.o
	gcc -m32 -Wall -ggdb listmachine.c -o listmachine

report: 
	pdflatex report.tex

clean:
	@touch listmachine