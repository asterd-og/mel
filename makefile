all: build run

build:
	clang -g $(shell find . -name "*.c") -o out/mel

run:
	./out/mel tests/helloworld.mel test

comp:
	nasm -felf64 out.asm -o out.o
	nasm -felf64 mel_lib.asm -o mlib.o
	gcc out.o mlib.o lib/mlib.o -o test
	./test