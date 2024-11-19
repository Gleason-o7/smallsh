all:
	gcc -std=c99 -g -o smallsh main.c
val:
	gcc -std=c99 -o smallsh main.c
no: 
	gcc -std=c99 -o smallsh a.c
	./smallsh
test:
	chmod +x ./p3testscript
	./p3testscript 2>&1
