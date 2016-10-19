all: textedit.c 
 	  gcc -g -Wall -o textedit textedit.c

  clean: 
	  $(RM) textedit
