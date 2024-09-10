#include "file.h"

char* open_file(char* filename) {
  FILE* fp = fopen(filename, "rb");
  if (!fp) return NULL;

  fseek(fp, 0, SEEK_END);
  long size = ftell(fp);
  fseek(fp, 0, SEEK_SET);

  char* buffer = (char*)malloc(size + 1);
  if (!buffer) {
    fprintf(stderr, "mel: Error while creating file buffer.\n");
    return NULL;
  }
  fread(buffer, 1, size, fp);

  buffer[size] = 0;
  fclose(fp);
  return buffer;
}