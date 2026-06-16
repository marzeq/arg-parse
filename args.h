#ifndef ARGS_H
#define ARGS_H
/*
==============================================================================
Argument Parser
==============================================================================

A small, self-contained command-line argument parser packaged as a
STB-style single-header library for C23 and later.

Features:
  - bool flags
  - char* arguments
  - int arguments
  - char** (appendable)
  - Default values
  - Positional argument validation
  - Automatic help generation (-h)

------------------------------------------------------------------------------
Basic Usage
------------------------------------------------------------------------------

0. Include the header.

  #define ARGS_IMPLEMENTATION
  #include "args.h"

1. Create and configure an args instance.

  args a = {};

  // Positional argument requirements:
  //   NULL = no positional arguments allowed
  //   "?"  = zero or one positional argument
  //   "+"  = one or more positional arguments
  //   "*"  = any number of positional arguments
  //   "N"  = exactly N positional arguments
  a.positional_args_req = "?";

2. Register arguments.

  bool* verbose = add_arg(&a, "v", "Enable verbose output", false);
  int* nproc = add_arg(&a, "nproc", "Number of processes", 4);
  const char** output = add_arg(&a, "output", "Output file", "out.txt");
  char* default_sources[] = {"main.c", "util.c", nil};
  const char*** sources = add_arg(&a, "source", "Source files", (const char**)default_sources);

3. Parse arguments.

  if (!args_parse(&a, argc, argv)) {
    args_reset(&a);
    return 1;
  }

4. Handle help.

  if (a.got_help) {
    args_reset(&a);
    return 0;
  }

5. Use parsed values.

  printf("verbose: %s\n", *verbose ? "true" : "false");
  printf("nproc: %d\n", *nproc);
  printf("output: %s\n", *output);
  for (usz i = 0; (*sources)[i] != nil; i++) {
    printf("source[%zu]: %s\n", i, (*sources)[i]);
  }

6. Reset parser state and free resources.

  args_reset(&a);

There is a fixed limit of ARGS_MAX_ARGS arguments that can be registered.
You can change it by defining ARGS_MAX_ARGS before including args.h.

------------------------------------------------------------------------------
Argument Registration
------------------------------------------------------------------------------

All arguments are registered with the _Generic add_arg() function, which
infers the argument type and default value from the provided default.

add_arg signature:

  T* add_arg(args* a, const char* name, const char* description, T default_value);

  where T may be:
    - bool
    - int
    - (const) char*
    - (const) char** (null-terminated)

------------------------------------------------------------------------------
Argument Semantics
------------------------------------------------------------------------------

If an argument was provided explicitly in the command line, arg.is_set
will be true. Otherwise, it will be false and the value will be the default.

If a char** argument is provided, the parser expects a null-terminated array
of strings. Each occurrence of the argument appends a value to the array
rather than replacing it.

------------------------------------------------------------------------------
Manually Accessing Arguments
------------------------------------------------------------------------------

Arguments are stored in an array:

    a.args

And have an associated count:

    a.args_count

------------------------------------------------------------------------------
Positional Arguments
------------------------------------------------------------------------------

Positional arguments are stored in:

    a.positional_args

And have an associated count:

    a.positional_arg_count

Example:

    prog input.txt output.txt

Access:

    for (usz i = 0; i < a.positional_arg_count; i++) {
      printf("%s\n", a.positional_args[i]);
    }

Validation modes with a.positional_args_req:

    NULL  no positional arguments allowed
    "?"   zero or one
    "+"   one or more
    "*"   any number
    "3"/"5"/...   exactly three/five/...

Example:

    a.positional_args_req = "2";

Accepts:

    prog file1 file2

Rejects:

    prog
    prog file1
    prog file1 file2 file3

------------------------------------------------------------------------------
Help
------------------------------------------------------------------------------

The parser automatically reserves -h and generates help text based on
registered arguments. When -h is provided, the parser prints usage
information and sets a.got_help to true.

------------------------------------------------------------------------------
Error Handling
------------------------------------------------------------------------------

args_parse() returns false and prints an error message to
stderr if any of the following occur:

  - an unknown argument is encountered
  - a required argument value is missing
  - an integer is invalid
  - an integer is out of range
  - positional argument requirements are violated
  - memory allocation fails

In such case, you are to immediately exit the program.

Example:

    if (!args_parse(&a, argc, argv)) {
      args_reset(&a);
      return 1;
    }

------------------------------------------------------------------------------
Ownership
------------------------------------------------------------------------------

The parser takes no ownership of any data you provide to it and is only
responsible for managing memory allocated internally. All default values
that are pointers (char*, char**) are copied internally. Ownership of the
original data remains with the caller, so if you passed a heap backed pointer
as a default value, you are responsible for freeing it after registering.

==============================================================================
*/

#if __STDC_VERSION__ < 202311L
#error "args.h requires C23 or later"
#endif

#include <stdbool.h>
#include <stddef.h>

typedef size_t usz;
#define nil NULL

typedef enum arg_type {
  BOOL,
  STRING,
  NUMBER,
  STRINGV,
} arg_type;

typedef struct {
  const char* name;
  const char* desc;
  arg_type type;
  union {
    bool bool_value;
    const char* string_value;
    int number_value;
    const char** stringv_value;
  } value;
  bool is_set;
  usz _stringv_capacity;
} arg;

#define get_arg(pvalue) ((arg*)((char*)(pvalue) - offsetof(arg, value)))
#define arg_is_set(pvalue) (get_arg(pvalue)->is_set)
#define arg_name(pvalue) (get_arg(pvalue)->name)
#define arg_desc(pvalue) (get_arg(pvalue)->desc)

#ifndef ARGS_MAX_ARGS
#define ARGS_MAX_ARGS 64
#endif

typedef struct {
  const char* positional_args_req;

  arg args[ARGS_MAX_ARGS];
  usz args_count;

  char** positional_args;
  usz positional_arg_count;
  usz _positional_arg_capacity;

  bool got_help;

  bool _parsed;
} args;

bool args_parse(args* a, int argc, char** argv);
void args_reset(args* a);

const char** _add_arg_string(
  args* a,
  const char* name,
  const char* description,
  const char* def
);

int* _add_arg_int(
  args* a,
  const char* name,
  const char* description,
  int def
);

bool* _add_arg_bool(
  args* a,
  const char* name,
  const char* description,
  bool def
);

const char*** _add_arg_stringv(
  args* a,
  const char* name,
  const char* description,
  const char** def
);

#define add_arg(a, name, description, def) \
  _Generic((def),                          \
    char*: _add_arg_string,                \
    const char*: _add_arg_string,          \
    int: _add_arg_int,                     \
    bool: _add_arg_bool,                   \
    char**: _add_arg_stringv,              \
    const char**: _add_arg_stringv         \
  )(a, name, description, def)

#ifdef ARGS_IMPLEMENTATION

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <stddef.h>

static char* _args_copy_string(args* a, const char* str) {
  size_t len = strlen(str);
  char* copy = malloc(len + 1);
  if (!copy) {
    return nil;
  }
  memcpy(copy, str, len + 1);
  return copy;
}

static void* _add_arg(args* ar, const char* name, const char* description, arg_type type) {
  if (ar->args_count >= ARGS_MAX_ARGS) {
    fprintf(
      stderr,
      "Maximum number of arguments exceeded (%d). "
      "#define ARGS_MAX_ARGS before including args.h to increase this limit.\n",
      ARGS_MAX_ARGS
    );
    return nil;
  }

  if (ar->_parsed) {
    fprintf(stderr, "Cannot add arguments after parsing\n");
    return nil;
  }

  if (name == nil || description == nil) {
    return nil;
  }

  if (strcmp(name, "h") == 0) {
    fprintf(stderr, "'-h' is reserved for help\n");
    return nil;
  }

  for (usz i = 0; i < ar->args_count; i++) {
    if (strcmp(ar->args[i].name, name) == 0) {
      fprintf(stderr, "Duplicate argument name: %s\n", name);
      return nil;
    }
  }

  ar->args[ar->args_count].name = _args_copy_string(ar, name);
  if (!ar->args[ar->args_count].name) {
    return nil;
  }
  ar->args[ar->args_count].type = type;
  ar->args[ar->args_count].is_set = false;
  ar->args[ar->args_count].desc = _args_copy_string(ar, description);
  if (!ar->args[ar->args_count].desc) {
    return nil;
  }
  ar->args_count += 1;
  return &ar->args[ar->args_count - 1].value;
}

const char** _add_arg_string(args* a, const char* name, const char* description, const char* def) {
  void* got = _add_arg(a, name, description, STRING);
  if (!got) {
    return nil;
  }
  a->args[a->args_count - 1].value.string_value = _args_copy_string(a, def);
  if (!a->args[a->args_count - 1].value.string_value) {
    return nil;
  }
  return (const char**)got;
}

int* _add_arg_int(args* a, const char* name, const char* description, int def) {
  void* got = _add_arg(a, name, description, NUMBER);
  if (!got) {
    return nil;
  }
  a->args[a->args_count - 1].value.number_value = def;
  return (int*)got;
}

bool* _add_arg_bool(args* a, const char* name, const char* description, bool def) {
  void* got = _add_arg(a, name, description, BOOL);
  if (!got) {
    return nil;
  }
  a->args[a->args_count - 1].value.bool_value = def;
  return (bool*)got;
}

static usz _null_term_array_len(const void** arr) {
  usz len = 0;
  while (arr[len] != nil) {
    len += 1;
  }
  return len;
}

const char*** _add_arg_stringv(args* a, const char* name, const char* description, const char** def) {
  void* got = _add_arg(a, name, description, STRINGV);
  if (!got) {
    return nil;
  }

  // special case - we have to copy the array of strings and not just store the pointer,
  // because when parsing instead of replacing the pointer to the array we append
  // to it, and since the default might not be backed by a simple malloc, we
  // need full ownership of the array to be able to realloc it
  usz def_len = _null_term_array_len((const void**)def);

  // set capacity to the smallest power of 2 that can hold the default array
  usz capacity = 1;
  while (capacity < def_len + 1) {
    capacity = capacity << 1;
  }

  char** copy = malloc(capacity * sizeof(char*));
  if (!copy) {
    return nil;
  }
  for (usz i = 0; i < def_len; i++) {
    copy[i] = _args_copy_string(a, def[i]);
  }
  copy[def_len] = nil; // null-terminate the array
  a->args[a->args_count - 1]._stringv_capacity = capacity;
  a->args[a->args_count - 1].value.stringv_value = (const char**)copy;
  return (const char***)got;
}

void args_reset(args* a) {
  // free allocated data that we commited ownership to
  for (usz i = 0; i < a->args_count; i++) {
    free((char*)a->args[i].name);
    free((char*)a->args[i].desc);
    if (a->args[i].type == STRING) {
      free((char*)a->args[i].value.string_value);
    } else if (a->args[i].type == STRINGV) {
      char** arr = (char**)a->args[i].value.stringv_value;
      for (usz j = 0; arr[j] != nil; j++) {
        free(arr[j]);
      }
      free(arr);
    }
  }

  for (usz i = 0; i < a->positional_arg_count; i++) {
    free(a->positional_args[i]);
  }

  free(a->positional_args);

  // reset state
  a->args_count = 0;

  a->positional_args = nil;
  a->positional_arg_count = 0;
  a->_positional_arg_capacity = 0;

  a->got_help = false;

  a->_parsed = false;
}

static bool _is_flag(const char* arg) {
  return arg[0] == '-' && arg[1] != '\0';
}

static bool _str_startswith(const char* str, const char* prefix) {
  size_t str_len = strlen(str);
  size_t prefix_len = strlen(prefix);
  return str_len >= prefix_len && strncmp(str, prefix, prefix_len) == 0;
}

static bool _add_positional_arg(args* a, const char* arg) {
  if (!a->positional_args) {
    a->positional_args = malloc(8* sizeof(char*));
    if (!a->positional_args) {
      return false;
    }
    a->_positional_arg_capacity = 8;
    a->positional_arg_count = 0;
  } else if (a->positional_arg_count >= a->_positional_arg_capacity) {
    usz new_cap = a->_positional_arg_capacity*2;

    char** new_positional_args = realloc(a->positional_args, new_cap * sizeof(char*));

    if (!new_positional_args) {
      return false;
    }

    a->positional_args = new_positional_args;
    a->_positional_arg_capacity = new_cap;
  }

  a->positional_args[a->positional_arg_count] = _args_copy_string(a, arg);

  if (!a->positional_args[a->positional_arg_count]) {
    return false;
  }

  a->positional_arg_count += 1;
  return true;
}

static bool _set_arg_value(args* a, arg* arg, const char* value_str) {
  if (arg->is_set && arg->type != STRINGV) {
    fprintf(stderr, "Argument '%s' specified multiple times\n", arg->name);
    return false;
  }

  switch (arg->type) {
    case BOOL: {
      if (value_str != NULL) {
        fprintf(stderr, "Boolean argument '%s' does not take a value\n", arg->name);
        return false;
      }

      arg->value.bool_value = true;
      break;
    }
    case STRING: {
      arg->value.string_value = _args_copy_string(a, value_str);
      if (arg->value.string_value == NULL) {
        fprintf(stderr, "Memory allocation failed for argument '%s'\n", arg->name);
        return false;
      }
      break;
    }
    case NUMBER: {
      char* end;
      errno = 0;

      long value = strtol(value_str, &end, 10);

      if (end == value_str || *end != '\0') {
        fprintf(stderr, "Invalid integer for argument '%s': %s\n", arg->name, value_str);
        return false;
      }

      if (errno == ERANGE || value < INT_MIN || value > INT_MAX) {
        fprintf(stderr, "Integer out of range for argument '%s': %s\n", arg->name, value_str);
        return false;
      }

      arg->value.number_value = (int)value;
      break;
    }
    case STRINGV: {
      char** arr = (char**)arg->value.stringv_value;
      usz len = _null_term_array_len((const void**)arr);

      if (len + 1 >= arg->_stringv_capacity) {
        usz new_cap = arg->_stringv_capacity * 2;
        char** new_arr = realloc(arr, new_cap * sizeof(char*));
        if (!new_arr) {
          fprintf(stderr, "Memory allocation failed for argument '%s'\n", arg->name);
          return false;
        }
        arr = new_arr;
        arg->value.stringv_value = (const char**)new_arr;
        arg->_stringv_capacity = new_cap;
      }

      char* copy = _args_copy_string(a, value_str);
      if (!copy) {
        fprintf(stderr, "Memory allocation failed for argument '%s'\n", arg->name);
        return false;
      }
      arr[len] = copy;
      arr[len + 1] = nil; // maintain null-termination
      break;
    }
    default: {
      fprintf(stderr, "Unknown argument type for '%s'\n", arg->name);
      return false;
    }
  }

  arg->is_set = true;

  return true;
}

bool args_parse(args* a, int argc, char** argv) {
  for (int i = 1; i < argc; i++) {
    char* arg = argv[i];
    if (!_is_flag(arg)) {
      if (!_add_positional_arg(a, arg)) {
        return false;
      }
      continue;
    }

    arg += 1; // skip the leading '-'
    
    if (strcmp(arg, "h") == 0) {
      printf("Usage: %s [options]", argv[0]);
      if (a->positional_args_req) {
        if (strcmp(a->positional_args_req, "+") == 0) {
          printf(" <arg1> [arg2] ...");
        } else if (strcmp(a->positional_args_req, "?") == 0) {
          printf(" [arg]");
        } else if (strcmp(a->positional_args_req, "*") == 0) {
          printf(" [arg1] [arg2] ...");
        } else {
          printf(" ");
          char* end;
          errno = 0;

          long expected = strtol(a->positional_args_req, &end, 10);
          if (*end != '\0' || end == a->positional_args_req || errno == ERANGE || expected < 0) {
            fprintf(stderr, "Invalid positional_args_req value: %s\n", a->positional_args_req);
            return false;
          }

          for (long j = 0; j < expected; j++) {
            printf("<arg%ld> ", j + 1);
          }
        }
      }

      printf("\n");

      if (a->args_count > 0) {
        printf("\nOptions:\n");

        usz max_name_len = 0;
        for (usz j = 0; j < a->args_count; j++) {
          usz len = strlen(a->args[j].name);
          if (len > max_name_len) {
            max_name_len = len;
          }
        }

        for (usz j = 0; j < a->args_count; j++) {
          printf("  -%-*s  %s", (int)max_name_len, a->args[j].name, a->args[j].desc);
          switch (a->args[j].type) {
            case STRING:
              if (a->args[j].value.string_value) {
                printf(" (default: %s)", a->args[j].value.string_value);
              }
              break;
            case NUMBER:
              printf(" (default: %d)", a->args[j].value.number_value);
              break;
            case STRINGV: {
              if (a->args[j].value.stringv_value[0] != nil) {
                printf(" (appends to: [");
                for (usz k = 0; a->args[j].value.stringv_value[k] != nil; k++) {
                  printf("%s", a->args[j].value.stringv_value[k]);
                  if (a->args[j].value.stringv_value[k + 1] != nil) {
                    printf(", ");
                  }
                }
                printf("])");
              }
            }
            default:
              break;
          }
          printf("\n");
        }
      }

      a->got_help = true;
      return true;
    }

    bool found = false;
    for (usz j = 0; j < a->args_count; j++) {
      // -flag value syntax
      if (strcmp(a->args[j].name, arg) == 0) {
        found = true;
        char* value_str = nil;
        if (a->args[j].type != BOOL) {
          if (i + 1 >= argc) {
            fprintf(stderr, "Argument '%s' requires a value\n", arg);
            return false;
          }
          value_str = argv[i + 1];
          i += 1;
        }
        if (!_set_arg_value(a, &a->args[j], value_str)) {
          return false;
        }
        break;
      }

      usz candidate_len = strlen(a->args[j].name); 
      if (!_str_startswith(arg, a->args[j].name)) {
        continue;
      }

      char* suffix = arg + candidate_len;
      if (*suffix == '\0' || *suffix != '=') {
        continue;
      }


      // -flag=value syntax
      found = true;
      char* value_str = suffix + 1;
      if (!_set_arg_value(a, &a->args[j], value_str)) {
        return false;
      }
      break;
    }

    if (!found) {
      char* equal_sign = strchr(arg, '=');
      if (equal_sign) {
        size_t len = equal_sign - arg;
        char* arg_name = malloc(len + 1);
        if (!arg_name) {
          return false;
        }
        strncpy(arg_name, arg, len);
        arg_name[len] = '\0';
        fprintf(stderr, "Unknown argument: %s\n", arg_name);
        free(arg_name);
      } else {
        fprintf(stderr, "Unknown argument: %s\n", arg);
      }
      return false;
    }
  }

  if (!a->positional_args_req) {
    // unspecified, assume 0
    if (a->positional_arg_count > 0) {
      fprintf(stderr, "Expected no free arguments, got %zu\n", a->positional_arg_count);
      return false;
    }
  } else if (strcmp(a->positional_args_req, "+") == 0) {
    if (a->positional_arg_count == 0) {
      fprintf(stderr, "Expected at least one free argument\n");
      return false;
    }
  } else if (strcmp(a->positional_args_req, "?") == 0) {
    if (a->positional_arg_count > 1) {
      fprintf(stderr, "Expected at most one free argument\n");
      return false;
    }
  } else if (strcmp(a->positional_args_req, "*") == 0) {
    // any number of free arguments is allowed
  } else {
    // expected to be a number
    char *end;
    errno = 0;

    long expected = strtol(a->positional_args_req, &end, 10);
    if (end == a->positional_args_req) {
      fprintf(stderr, "Invalid positional_args_req value: %s\n", a->positional_args_req);
      return false;
    }

    if (*end != '\0') {
      fprintf(stderr, "Invalid positional_args_req value: %s\n", a->positional_args_req);
      return false;
    }

    if (errno == ERANGE || expected < 0) {
      fprintf(stderr, "Invalid positional_args_req value: %s\n", a->positional_args_req);
      return false;
    }

    if (a->positional_arg_count != (usz)expected) {
      fprintf(stderr, "Expected %ld free arguments, got %zu\n", expected, a->positional_arg_count);
      return false;
    }
  }

  a->_parsed = true;
  return true;
}

#endif // ARGS_IMPLEMENTATION

#endif // ARGS_H
