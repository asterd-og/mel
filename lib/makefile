all:
	gcc -c mlib.c -o mlib.c.o
	nasm -felf64 mlib.asm -o mlib.asm.o
	ar rcs mlib.a mlib.asm.o mlib.c.o