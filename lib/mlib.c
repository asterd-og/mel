#include <time.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

// gcc -c mlib.c -o mlib.o

uint64_t getRand(uint64_t max) {
  srand(time(NULL));
  return rand() % max;
}

int printFmt(char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int ret = vfprintf(stdout, fmt, ap);
  va_end(ap);
  return ret;
}

int StrPrintFmt(char* buffer, char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int ret = vsprintf(buffer, fmt, ap);
  va_end(ap);
  return ret;
}

int StrToInt(char* str) {
  return atoi(str);
}

int ChrIsNum(char chr) {
  return (chr >= '0' && chr <= '9');
}

int strLen(char* str) {
  return strlen(str);
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