#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include "list.h"
#include "hashmap.h"
#include "file.h"
#include "frontend/lexer.h"
#include "frontend/parser.h"
#include "frontend/ast_viewer.h"
#include "backend/backend.h"

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

  parser_t* parser = parser_create(lexer);
  parser_parse(parser);

  char* asm_name;
  char* obj_name;

#ifdef DEBUG
  asm_name = "out.asm";
  printf("AST Viewer results:\n");

  ast_view(parser->ast);
#else
  srand(time(NULL));
  char* asm_name = (char*)malloc(11); // 6 random digits (dot) asm
  char* obj_name = (char*)malloc(11); // 6 random digits (dot) asm
  uint32_t num = rand() % 999999;
#endif
  backend_gen(parser->ast, obj_name);

#ifndef DEBUG
#endif

  lexer_destroy(lexer);

  free(file);
  return 0;
}