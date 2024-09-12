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

  char* cg_name = "out.ll";
  char* obj_name;

#ifdef DEBUG
  cg_name = "out.ll";
  printf("AST Viewer results:\n");

  ast_view(parser->ast);
#else
  srand(time(NULL));
  cg_name = (char*)malloc(11); // 6 random digits (dot) ll
  obj_name = (char*)malloc(11); // 6 random digits (dot) o
  uint32_t num = rand() % 999999;
  sprintf(cg_name, "%d.ll", num);
  sprintf(obj_name, "%d.o", num);
#endif
  backend_gen(parser->ast, cg_name);

#ifndef DEBUG
  char* command = (char*)malloc(256);
  sprintf(command, "llc -filetype=obj %s -o %s -opaque-pointers", cg_name, obj_name);
  int status = system(command);
  remove(cg_name);
  if (status != 0) {
    return status;
  }
  sprintf(command, "ld.lld %s /usr/mel/lib/lib.o -r -o %s", obj_name, argv[2]);
  status = system(command);
  remove(obj_name);
  if (status != 0) {
    return status;
  }
#endif

  lexer_destroy(lexer);

  free(file);
  return 0;
}