# Makefile for heat project

CFLAGS= -Wall -Wextra -pedantic

heat: heat.c
	$(CC) heat.c -o heat $(CFLAGS) -std=c99

clean:
	rm heat

