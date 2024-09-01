#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include "list.h"
#include "hashmap.h"
#include "frontend/lexer.h"
#include "frontend/parser.h"
#include "frontend/ast_viewer.h"
#include "backend/cg.h"

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

int main(int argc, char** argv) {
  if (argc < 2) {
    printf("mel: Error: No input files.\n");
    return 1;
  }
  if (argc < 3) {
    printf("mel: Error: No output file.\n");
    return 1;
  }
  char* file = open_file(argv[1]);
  if (!file) {
    printf("mel: Error: Error while opening file '%s'.\n", argv[1]);
    return 1;
  }

  lexer_t* lexer = lexer_create(file, argv[1]);
  lexer_lex(lexer);

  /*for (list_item_t* item = lexer->tok_list->head->next; item != lexer->tok_list->head; item = item->next) {
    token_t* tok = (token_t*)item->data;
    printf("%s - %.*s\n", type_to_str[tok->type], tok->text_len, tok->text);
  }*/

  parser_t* parser = parser_create(lexer);
  parser_parse(parser);

  /*printf("AST Viewer results:\n");

  ast_view(parser->ast);*/

  srand(time(NULL));
  char* asm_name = (char*)malloc(11); // 6 random digits (dot) asm
  char* obj_name = (char*)malloc(11); // 6 random digits (dot) asm
  uint32_t num = rand() % 999999;
  sprintf(asm_name, "%06d.asm", num);
  sprintf(obj_name, "%06d.o", num);
  cg_t* cg = cg_create(parser->ast, asm_name);
  cg_gen(cg);

  char* cmd = (char*)malloc(50);
  char* cmd2 = (char*)malloc(50);
  sprintf(cmd, "nasm -felf64 %s -o %s", asm_name, obj_name);
  sprintf(cmd2, "gcc %s lib/mlib.a -o %s", obj_name, argv[2]);
  system(cmd);
  system(cmd2);
  remove(asm_name);
  remove(obj_name);

  lexer_destroy(lexer);

  free(file);
  return 0;
}