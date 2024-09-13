#pragma once

#include <stdint.h>
#include <stddef.h>
#include "../list.h"

void backend_gen(list_t* ast, bool print_ir, char* out);