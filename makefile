all: build run

build:
	-mkdir out
	clang -g $(shell find . -name "*.c") -DDEBUG -o out/mel

run:
	./out/mel tests/test.mel test

comp:
	nasm -felf64 out.asm -o out.o
	gcc out.o lib/mlib.a -o test
	./test