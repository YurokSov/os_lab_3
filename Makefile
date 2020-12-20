all:
	gcc -Werror -pedantic main.c -lpthread -o main
clean:
	rm main