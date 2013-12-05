all: listmachine listmachine6

listmachine: listmachine.c
	gcc -m32 -Wall -ggdb listmachine.c -o listmachine

listmachine6: listmachine6.c
	gcc -m32 -Wall -ggdb listmachine6.c -o listmachine6

report: 
	pdflatex report.tex

clean:
	rm -f listmachine
	rm -f listmachine6
	rm -f report.pdf