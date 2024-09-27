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

char* compile(char* filename) {
  char* file = open_file(filename);
  if (!file) {
    printf("mel: Error: Error while opening file '%s'.\n", filename);
    return 1;
  }

  lexer_t* lexer = lexer_create(file, filename);
  lexer_lex(lexer);

  parser_t* parser = parser_create(lexer);
  parser_parse(parser);

  char* cg_name;

  // ast_view(parser->ast);
  srand(time(NULL));
  cg_name = (char*)malloc(11); // 6 random digits (dot) ll
  uint32_t num = rand() % 999999;
  sprintf(cg_name, "%d.ll", num);

  backend_gen(parser->ast, false, cg_name);

  lexer_destroy(lexer);

  free(file);

  return cg_name;
}

char* do_llvm(char* filename) {
  srand(time(NULL));
  char* obj_name = (char*)malloc(11); // 6 random digits (dot) ll
  uint32_t num = rand() % 999999;
  sprintf(obj_name, "%d.o", num);
  char* command = (char*)malloc(256);
  sprintf(command, "llc --code-model=medium -filetype=obj %s -o %s -opaque-pointers", filename, obj_name);
  int status = system(command);
  if (status != 0) {
    return status;
  }
  return obj_name;
}

char* do_link(char* out_fname, char** in_fname) {
  char cmd[4096];
  sprintf(cmd, "clang");
  while (*in_fname) {
    sprintf(cmd, "%s %s", cmd, *in_fname);
    in_fname++;
  }
  sprintf(cmd, "%s -o %s", cmd, out_fname);
  int status = system(cmd);
  if (status != 0) {
    return NULL;
  }
  return out_fname;
}

void usage(char* name) {
  fprintf(stderr, "Usage: %s [options] file...\n", name);
  fprintf(stderr, "Options:\n");
  fprintf(stderr, "  -c generate object files but don't link them\n");
  fprintf(stderr, "  -f generate free standing object file\n");
  fprintf(stderr, "  -o outfile, produce the output file\n");
  exit(1);
}

int main(int argc, char** argv) {
  if (argc < 2) {
    printf("mel: Error: No input files.\n");
    usage(argv[0]);
  }
  char* out_fname = NULL;
  bool link = true;
  bool freestanding = false;
  int i = 1;
  if (argv[1][0] == '-') {
    for (i = 1; i < argc; i++) {
      // No leading '-', stop scanning for options
      if (*argv[i] != '-')
        break;
  
      // For each option in this argument
      for (int j = 1; (*argv[i] == '-') && argv[i][j]; j++) {
        switch (argv[i][j]) {
	        case 'o':
	          out_fname = argv[++i];	// Save & skip to next argument
	          break;
	        case 'c':
	          link = false;
	          break;
	        case 'f':
            freestanding = true;
	          break;
	        default:
	          usage(argv[0]);
            break;
        }
      }
    }
  }
  if (!out_fname) {
    out_fname = argv[argc - 1];
    argc--;
  }
  if (argc - i - 1 > 2) {
    fprintf(stderr, "Mel: Error: Trying to generate object file from multiple sources.\n");
    return 1;
  }
  char* obj;
  char* llc;
  char* in_fnames[100];
  int obj_cnt = 0;
  while (i < argc) {
    llc = compile(argv[i]);
    obj = do_llvm(llc);
    in_fnames[obj_cnt++] = obj;
    in_fnames[obj_cnt] = NULL;
    remove(llc);
    i++;
  }
  if (!freestanding) {
    char* lib = (char*)malloc(64);
    strcpy(lib, "/usr/mel/lib/lib.a");
    in_fnames[obj_cnt] = lib;
    in_fnames[obj_cnt + 1] = NULL;
  }
  if (link) {
    do_link(out_fname, in_fnames);
    for (i = 0; i < obj_cnt; i++) {
      remove(in_fnames[i]);
    }
  } else {
    rename(in_fnames[0], out_fname);
  }
  return 0;
}