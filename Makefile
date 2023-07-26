file=main.c
cmd=gcc

httpd: httpd.c
	gcc -W -Wall -lpthread -o httpd.out httpd.c


client: simpleclient.c
	gcc -o client.out $^


clean:
	-rm httpd.out client.out
