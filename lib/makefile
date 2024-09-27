all:
	gcc -c mlib.c -o mlib.c.o
	nasm -felf64 mlib.asm -o mlib.asm.o
	mel -c io.mel io.mel.o
	ar rcs lib.a io.mel.o mlib.asm.o mlib.c.o
	rm mlib.asm.o mlib.c.o io.mel.o