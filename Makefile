all: colorpipe

colorpipe: colorpipe.c
	cc colorpipe.c -I/usr/local/include -L/usr/local/lib -lSDL2 -o colorpipe

run: colorpipe
	cat colorpipe.c | ./colorpipe
