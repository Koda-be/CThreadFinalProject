.SILENT:

GRILLESDL=GrilleSDL
RESSOURCES=Ressources

CC = gcc -I$(GRILLESDL) -I$(RESSOURCES)
OBJS = $(GRILLESDL)/GrilleSDL.o $(RESSOURCES)/Ressources.o
PROGRAMS = Blockudoku

ALL: $(PROGRAMS)

Blockudoku:	Blockudoku.c $(OBJS)
	echo Creation de Blockudoku...
	$(CC) Blockudoku.c -o Blockudoku $(OBJS) -Wall -lrt -lpthread -lSDL

$(GRILLESDL)/GrilleSDL.o:	$(GRILLESDL)/GrilleSDL.c $(GRILLESDL)/GrilleSDL.h
		echo Creation de GrilleSDL.o ...
		$(CC) -c $(GRILLESDL)/GrilleSDL.c
		mv GrilleSDL.o $(GRILLESDL)

$(RESSOURCES)/Ressources.o:	$(RESSOURCES)/Ressources.c $(RESSOURCES)/Ressources.h
		echo Creation de Ressources.o ...
		$(CC) -c $(RESSOURCES)/Ressources.c
		mv Ressources.o $(RESSOURCES)

clean:
	@rm -f $(OBJS) core

clobber:	clean
	@rm -f tags $(PROGRAMS)
