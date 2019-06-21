all:
	gcc -o Server Server.c -lpthread
	gcc -o Client Client.c -lpthread