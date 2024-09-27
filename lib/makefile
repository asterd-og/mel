all:
	gcc -c mlib.c -o mlib.c.o
	nasm -felf64 mlib.asm -o mlib.asm.o
	ar rcs lib.a mlib.asm.o mlib.c.o
	rm mlib.asm.o mlib.c.o