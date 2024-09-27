#include <time.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>

// gcc -c mlib.c -o mlib.o

uint64_t getRand(uint64_t max) {
  srand(time(NULL));
  return rand() % max;
}

// IO

int printFmt(char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int ret = vfprintf(stdout, fmt, ap);
  va_end(ap);
  return ret;
}

int strPrintFmt(char* buffer, char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int ret = vsprintf(buffer, fmt, ap);
  va_end(ap);
  return ret;
}

char* getInput(char* buffer, int size) {
  return fgets(buffer, size, stdin);
}

void printBin(uint64_t value, int bits) {
  for (int i = bits - 1; i >= 0; i--) {
    printf("%d", ((value & (1<<i)) ? 1 : 0));
  }
  printf("\n");
}

struct file {
  int fd;
  uint64_t size;
};

/*
fn fileOpen(var name: *char, var mode: *char): *file;
fn fileRead(var fp: *file, var buf: *u8, var count: u64): int;
fn fileClose(var fp: *file): int;
*/
int __mlib_open(char* name, int mode) {
  return open(name, mode);
}

int64_t __mlib_get_size(int fd) {
  struct stat s;
  if (fstat(fd, &s) == -1) {
    int saveErrno = errno;
    return errno;
  }
  return s.st_size;
}

int64_t __mlib_read(int fd, uint8_t* buffer, uint64_t count) {
  return read(fd, buffer, count);
}

int64_t __mlib_write(int fd, uint8_t* buffer, uint64_t count) {
  return write(fd, buffer, count);
}

uint64_t __mlib_lseek(int fd, uint64_t offset, int whence) {
  return lseek(fd, offset, whence);
}

int __mlib_close(int fd) {
  return close(fd);
}

// Str

int strToInt(char* str) {
  return atoi(str);
}

int strLen(char* str) {
  return strlen(str);
}