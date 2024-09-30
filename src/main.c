#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <unistd.h>
#include "list.h"
#include "hashmap.h"
#include "file.h"
#include "frontend/lexer.h"
#include "frontend/parser.h"
#include "frontend/ast_viewer.h"
#include "backend/backend.h"

uint32_t last_random = 0;
bool verbose = false;

char* compile(char* filename) {
  char* file = open_file(filename);
  if (!file) {
    printf("mel: Error: Error while opening file '%s'.\n", filename);
    if (access(filename, F_OK) != 0) printf("Mel: File doesn't exist.\n");
    return NULL;
  }

  lexer_t* lexer = lexer_create(file, filename);
  if (verbose) fprintf(stderr, "Lexer: Created.\n");
  lexer_lex(lexer);
  if (verbose) fprintf(stderr, "Lexer: Lexed.\n");

  parser_t* parser = parser_create(lexer);
  if (verbose) fprintf(stderr, "Parser: Created.\n");
  parser_parse(parser);
  if (verbose) fprintf(stderr, "Parser: Parsed.\n");

  char* cg_name;

  // ast_view(parser->ast);
  srand(time(NULL));
  cg_name = (char*)malloc(32); // 6 random digits (dot) ll
  if (last_random == 0) last_random = rand() % 999999;
  else last_random++;
  sprintf(cg_name, "%d.ll", last_random);

  if (verbose) fprintf(stderr, "Generating llvm code to %s.\n", cg_name);
  backend_gen(parser->ast, false, cg_name);
  if (verbose) fprintf(stderr, "Code generated.\n");

  lexer_destroy(lexer);
  parser_destroy(parser);

  free(file);

  return cg_name;
}

char* do_llvm(char* filename) {
  srand(time(NULL));
  char* obj_name = (char*)malloc(32);
  sprintf(obj_name, "%s.o", filename);
  char* command = (char*)malloc(256);
  sprintf(command, "llc --code-model=medium -filetype=obj %s -o %s -opaque-pointers", filename, obj_name);
  if (verbose) fprintf(stderr, "Compiling llvm IR code.\n");
  int status = system(command);
  if (verbose) fprintf(stderr, "LLC Returned %d.\n", status);
  if (status != 0) {
    return NULL;
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
  if (verbose) fprintf(stderr, "Linking using %s.\n", cmd);
  int status = system(cmd);
  if (verbose) fprintf(stderr, "Linker returned %d.\n", status);
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
  fprintf(stderr, "  -l links the object file with a library\n");
  fprintf(stderr, "  -v enables verbose mode\n");
  exit(1);
}

char* find_lib(char* name) {
  char* path = (char*)malloc(256);
  sprintf(path, "/usr/mel/lib/lib%s.a", name);
  if (access(path, F_OK) == 0) return path;
  fprintf(stderr, "Mel: Error: No lib named '%s' found.\n", name);
  exit(1);
  return NULL;
}

int main(int argc, char** argv) {
  if (argc < 2) {
    printf("mel: Error: No input files.\n");
    usage(argv[0]);
  }
  char* out_fname = NULL;
  bool link = true;
  bool freestanding = false;
  char* lib_names[100];
  int lib_idx = 0;
  bool link_lib = false;
  int i = 1;
  if (argv[1][0] == '-') {
    for (i = 1; i < argc; i++) {
      // No leading '-', stop scanning for options
      if (*argv[i] != '-')
        break;
  
      // For each option in this argument
      for (int j = 1; (*argv[i] == '-') && argv[i][j]; j++) {
        switch (argv[i][j]) {
          case 'v':
            verbose = true;
            break;
          case 'l':
            link_lib = true;
            lib_names[lib_idx++] = find_lib(argv[++i]);
            break;
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
    for (int k = i; k < argc - 1; k++) {
      if (!strcmp(argv[k], out_fname)) {
        fprintf(stderr, "Mel: Error: Trying to output into an input file.\n");
        exit(1);
      }
    }
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
    if (verbose) fprintf(stderr, "Compiled %s to %s.\n", argv[i], llc);
    if (!llc) {
      for (i = 0; i < obj_cnt; i++) {
        remove(in_fnames[i]);
      }
      exit(1);
    }
    obj = do_llvm(llc);
    if (verbose) fprintf(stderr, "LLVM Compiled %s to %s.\n", llc, obj);
    if (!obj) {
      for (i = 0; i < obj_cnt; i++) {
        remove(in_fnames[i]);
      }
      exit(1);
    }
    in_fnames[obj_cnt++] = obj;
    in_fnames[obj_cnt] = NULL;
    remove(llc);
    i++;
  }
  if (!freestanding) {
    char* lib = (char*)malloc(64);
    strcpy(lib, "/usr/mel/lib/libstd.a");
    in_fnames[obj_cnt] = lib;
    in_fnames[obj_cnt + 1] = NULL;
  }
  if (link) {
    if (link_lib) {
      if (!freestanding) obj_cnt++;
      for (i = 0; i < lib_idx; i++) {
        in_fnames[obj_cnt + i] = lib_names[i];
        in_fnames[obj_cnt + i + 1] = NULL;
      }
      if (!freestanding) obj_cnt--;
      do_link(out_fname, in_fnames);
      if (verbose) fprintf(stderr, "Linked to %s.\n", out_fname);
    } else {
      do_link(out_fname, in_fnames);
      if (verbose) fprintf(stderr, "Linked to %s.\n", out_fname);
    }
    for (i = 0; i < obj_cnt; i++) {
      remove(in_fnames[i]);
    }
  } else {
    if (link_lib) {
      lib_names[lib_idx] = in_fnames[0];
      lib_names[lib_idx + 1] = NULL;
      do_link(out_fname, lib_names);
      if (verbose) fprintf(stderr, "Linked to %s.\n", out_fname);
      remove(in_fnames[0]);
    } else {
      rename(in_fnames[0], out_fname);
      if (verbose) fprintf(stderr, "Renamed to %s.\n", out_fname);
    }
  }
  return 0;
}