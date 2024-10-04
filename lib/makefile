all:
	gcc -c mlib.c -o mlib.c.o
	nasm -felf64 mlib.asm -o mlib.asm.o
	mel -c io.mel io.mel.o
	ar rcs libstd.a io.mel.o mlib.asm.o mlib.c.o

	mel -c math.mel math.mel.o
	gcc -shared -o libmath.a /usr/lib/x86_64-linux-gnu/libm.a math.mel.o
	sudo mv *.a /usr/mel/lib

	rm *.o