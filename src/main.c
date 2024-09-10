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

  char* cg_name;
  char* obj_name;

#ifdef DEBUG
  cg_name = "out.ll";
  printf("AST Viewer results:\n");

  ast_view(parser->ast);
#else
  srand(time(NULL));
  cg_name = (char*)malloc(11); // 6 random digits (dot) ll
  uint32_t num = rand() % 999999;
  sprintf(cg_name, "%d.ll", num);
#endif
  backend_gen(parser->ast, cg_name);

#ifndef DEBUG
  char* command = (char*)malloc(128);
  sprintf(command, "llc -filetype=obj %s -o %s -opaque-pointers", cg_name, argv[2]);
  system(command);
  remove(cg_name);
#endif

  lexer_destroy(lexer);

  free(file);
  return 0;
}