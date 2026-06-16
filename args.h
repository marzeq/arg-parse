#ifndef ARGS_H
#define ARGS_H
/*
====================================================================================
Argument Parser
====================================================================================

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

------------------------------------------------------------------------------------
Basic Usage
------------------------------------------------------------------------------------

0. Include the header.

  #define ARGS_IMPLEMENTATION
  #include "args.h"

1. Create and configure an args instance.

  args a = {0};

  // Positional argument requirements:
  //   null = no positional arguments allowed
  //   "?"  = zero or one positional argument
  //   "+"  = one or more positional arguments
  //   "*"  = any number of positional arguments
  //   "N"  = exactly N positional arguments
  a.positional_args_req = "?";

2. Register arguments.

  bool* verbose = add_arg(&a, "v", "Enable verbose output", false);
  int* nproc = add_arg(&a, "nproc", "Number of processes", 4);
  const char** output = add_arg(&a, "output", "Output file", "out.txt");
  char* default_sources[] = {"main.c", "util.c", nullptr};
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
  for (size_t i = 0; (*sources)[i] != nullptr; i++) {
    printf("source[%zu]: %s\n", i, (*sources)[i]);
  }

6. Reset parser state and free resources.

  args_reset(&a);

There is a fixed limit of ARGS_MAX_ARGS arguments that can be registered.
You can change it by defining ARGS_MAX_ARGS before including args.h.

------------------------------------------------------------------------------------
Argument Registration
------------------------------------------------------------------------------------

All arguments are registered with the _Generic add_arg() function, which
infers the argument type and default value from the provided default.

add_arg signature:

  T* add_arg(args* a, const char* name, const char* description, T default_value);

  where T may be:
    - bool
    - int
    - (const) char*
    - (const) char** (null-terminated)

------------------------------------------------------------------------------------
Argument Semantics
------------------------------------------------------------------------------------

If an argument was provided explicitly in the command line, arg.is_set
will be true. Otherwise, it will be false and the value will be the default.

If a char** argument is provided, the parser expects a null-terminated array
of strings. Each occurrence of the argument appends a value to the array
rather than replacing it.

------------------------------------------------------------------------------------
Manually Accessing Arguments
------------------------------------------------------------------------------------

Arguments are stored in an array:

    a.args

And have an associated count:

    a.args_count

------------------------------------------------------------------------------------
Positional Arguments
------------------------------------------------------------------------------------

Positional arguments are stored in:

  a.positional_args

And have an associated count:

  a.positional_arg_count

Example:

  prog input.txt output.txt

Access:

  for (size_t i = 0; i < a.positional_arg_count; i++) {
    printf("%s\n", a.positional_args[i]);
  }

Validation modes with a.positional_args_req:

  null        no positional arguments allowed
  "?"         zero or one
  "+"         one or more
  "*"         any number
  "3"/"5"/... exactly three/five/...

Example:

  a.positional_args_req = "2";

Accepts:

  prog file1 file2

Rejects:

  prog
  prog file1
  prog file1 file2 file3

Positional args may preceed and follow flags, and be interspersed with them:

  prog file1 -v file2

------------------------------------------------------------------------------------
Help
------------------------------------------------------------------------------------

The parser automatically reserves -h and generates help text based on
registered arguments. When -h is provided, the parser prints usage
information and sets a.got_help to true.

------------------------------------------------------------------------------------
Error Handling
------------------------------------------------------------------------------------

Adding:
  add_arg() returns null and prints an error message to stderr if any of the
  following occur:

    - too many arguments are registered (exceeding ARGS_MAX_ARGS)
    - an argument is registered after parsing
    - an argument name is null or "h"
    - a description is null
    - duplicate argument names
    - memory allocation fails

  You may also check a.failed_adding after registering arguments
  to see if any add_arg() call failed, so you don't have to
  have error handling logic after every single add_arg() call.

Parsing:

  args_parse() returns false and prints an error message to
  stderr if any of the following occur:

    - an unknown argument is encountered
    - a required argument value is missing
    - an integer is invalid
    - an integer is out of range
    - positional argument requirements are violated
    - memory allocation fails

In such cases, you are to immediately exit the program:

  args_reset(&a);
  return 1;

------------------------------------------------------------------------------------
Ownership
------------------------------------------------------------------------------------

The parser takes no ownership of any data you provide to it and is only
responsible for managing memory allocated internally. All values (name/desc/default)
that are pointers (char*, char**) are copied internally. Ownership of the
original data remains with the caller, so if you passed a heap backed pointer
as a default value, you are responsible for freeing it after registering.

------------------------------------------------------------------------------------
Decisions Rationale
------------------------------------------------------------------------------------

# 1. Why a fixed number of arguments?

Very rarely does the number of arguments change at runtime, so we do not
need dynamic reallocation capabilities. Instead, we can set the maximum
number of arguments at compile time.

Even when the number of arguments is determined at runtime, it is even
rarer for it to grow beyond a reasonable upper bound. In such cases, we
can simply set the limit high enough to cover all practical use cases
without introducing the complexity of dynamic resizing.

If the number of arguments is dynamic but we end up using fewer than the
configured limit, the amount of wasted memory is negligible. Even if we
reserved space for 1000 arguments, which is itself highly unlikely, the
total memory usage would still be only a few hundred kilobytes at most,
so practically nothing on modern systems.

# 2. Why not use a hash table instead of a dynamic array for argument storage?

We intentionally return pointers to argument values instead of providing
an API that retrieves values by argument name. As a result, value access
is constant time regardless of the underlying storage mechanism.

If access to the underlying argument structure is required, we can perform
offset arithmetic on the value pointer to recover a pointer to the
containing argument structure. See the `get_arg()` macro.

When a linear search is required, the number of registered arguments is
almost always small enough that the performance benefit of a hash table
does not justify the additional implementation complexity.

# 3. Why copy all pointer values instead of storing the provided pointers directly?

This is mainly done for consistency and convenience:

1. Appendable arguments require ownership of the underlying array because
   of their semantics. To keep ownership rules consistent across all
   pointer-based argument types, we copy all pointer values, even for
   non-appendable arguments that do not strictly require it.

2. From the library user's perspective, ownership of the provided values
   is never shared with the parser. Once an argument has been registered,
   the user may immediately free or discard the original values if they
   no longer need them, without affecting the parser's internal state.


====================================================================================
*/

#if !defined(__STDC_VERSION__) || __STDC_VERSION__ < 202311L
#error "args.h requires C23 or later"
#endif

#include <stdbool.h>
#include <stddef.h>

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
  size_t _stringv_capacity;
} arg;

#define get_arg(pvalue) ((arg*)((char*)(pvalue) - offsetof(arg, value)))
#define arg_is_set(pvalue) (get_arg(pvalue)->is_set)
#define arg_name(pvalue) (get_arg(pvalue)->name)
#define arg_desc(pvalue) (get_arg(pvalue)->desc)

#ifndef ARGS_MAX_ARGS
#define ARGS_MAX_ARGS 64
#endif

static_assert(ARGS_MAX_ARGS > 0, "ARGS_MAX_ARGS must be greater than 0");

typedef struct {
  const char* positional_args_req;

  arg args[ARGS_MAX_ARGS];
  size_t args_count;

  char** positional_args;
  size_t positional_arg_count;
  size_t _positional_arg_capacity;

  bool got_help;

  bool _parsed;
  bool failed_adding;
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

static void _xfree(void* ptr) {
  if (ptr) {
    free(ptr);
  }
}

static void* _add_arg(args* ar, const char* name, const char* description, arg_type type) {
  if (ar->args_count >= ARGS_MAX_ARGS) {
    fprintf(
      stderr,
      "Maximum number of arguments exceeded (%d). "
      "#define ARGS_MAX_ARGS before including args.h to increase this limit.\n",
      ARGS_MAX_ARGS
    );
    ar->failed_adding = true;
    return nullptr;
  }

  if (ar->_parsed) {
    fprintf(stderr, "Cannot add arguments after parsing\n");
    ar->failed_adding = true;
    return nullptr;
  }

  if (name == nullptr || description == nullptr) {
    fprintf(stderr, "Argument name and/or description cannot be null\n");
    ar->failed_adding = true;
    return nullptr;
  }

  for (const char* p = name; *p != '\0'; p++) {
    if (*p == '=') {
      fprintf(stderr, "Argument name cannot contain '=': %s\n", name);
      ar->failed_adding = true;
      return nullptr;
    }
  }

  if (strcmp(name, "h") == 0) {
    fprintf(stderr, "'-h' is reserved for help\n");
    ar->failed_adding = true;
    return nullptr;
  }

  for (size_t i = 0; i < ar->args_count; i++) {
    if (strcmp(ar->args[i].name, name) == 0) {
      fprintf(stderr, "Duplicate argument name: %s\n", name);
      ar->failed_adding = true;
      return nullptr;
    }
  }

  char* name_copy = strdup(name);
  if (!name_copy) {
    fprintf(stderr, "Memory allocation failed for argument name: %s\n", name);
    ar->failed_adding = true;
    return nullptr;
  }

  char* desc_copy = strdup(description);
  if (!desc_copy) {
    fprintf(stderr, "Memory allocation failed for argument description: %s\n", name);
    free(name_copy);
    ar->failed_adding = true;
    return nullptr;
  }

  ar->args[ar->args_count].name = name_copy;
  ar->args[ar->args_count].desc = desc_copy;
  ar->args[ar->args_count].type = type;
  ar->args[ar->args_count].is_set = false;
  ar->args_count += 1;
  return &ar->args[ar->args_count - 1].value;
}

const char** _add_arg_string(args* a, const char* name, const char* description, const char* def) {
  if (def == nullptr) {
    def = "";
  }
  void* got = _add_arg(a, name, description, STRING);
  if (!got) {
    return nullptr;
  }
  a->args[a->args_count - 1].value.string_value = strdup(def);
  if (!a->args[a->args_count - 1].value.string_value) {
    fprintf(stderr, "Memory allocation failed for default value of argument '%s'\n", name);
    a->failed_adding = true;
    return nullptr;
  }
  return (const char**)got;
}

int* _add_arg_int(args* a, const char* name, const char* description, int def) {
  void* got = _add_arg(a, name, description, NUMBER);
  if (!got) {
    return nullptr;
  }
  a->args[a->args_count - 1].value.number_value = def;
  return (int*)got;
}

bool* _add_arg_bool(args* a, const char* name, const char* description, bool def) {
  void* got = _add_arg(a, name, description, BOOL);
  if (!got) {
    return nullptr;
  }
  a->args[a->args_count - 1].value.bool_value = def;
  return (bool*)got;
}

static size_t _null_term_array_len(const void** arr) {
  size_t len = 0;
  while (arr[len] != nullptr) {
    len += 1;
  }
  return len;
}

const char*** _add_arg_stringv(args* a, const char* name, const char* description, const char** def) {
  if (def == nullptr) {
    // treat null default as empty array
    static const char* empty[] = {nullptr};
    def = empty;
  }
  void* got = _add_arg(a, name, description, STRINGV);
  if (!got) {
    return nullptr;
  }

  // special case - we have to copy the array of strings and not just store the pointer,
  // because when parsing instead of replacing the pointer to the array we append
  // to it, and since the default might not be backed by a simple malloc, we
  // need full ownership of the array to be able to realloc it
  size_t def_len = _null_term_array_len((const void**)def);

  // set capacity to the smallest power of 2 that can hold the default array
  size_t capacity = 1;
  while (capacity < def_len + 1) {
    capacity = capacity << 1;
  }

  char** copy = malloc(capacity * sizeof(char*));
  if (!copy) {
    fprintf(stderr, "Memory allocation failed for default value array of argument '%s'\n", name);
    a->failed_adding = true;
    return nullptr;
  }
  for (size_t i = 0; i < def_len; i++) {
    copy[i] = strdup(def[i]);
    if (!copy[i]) {
      for (size_t j = 0; j < i; j++) {
        _xfree(copy[j]);
      }
      _xfree(copy);
      fprintf(stderr, "Memory allocation failed for default value of argument '%s'\n", name);
      a->failed_adding = true;
      return nullptr;
    }
  }
  copy[def_len] = nullptr; // null-terminate the array
  a->args[a->args_count - 1]._stringv_capacity = capacity;
  a->args[a->args_count - 1].value.stringv_value = (const char**)copy;
  return (const char***)got;
}

void args_reset(args* a) {
  // free allocated data that we commited ownership to
  for (size_t i = 0; i < a->args_count; i++) {
    _xfree((char*)a->args[i].name);
    _xfree((char*)a->args[i].desc);
    if (a->args[i].type == STRING) {
      _xfree((char*)a->args[i].value.string_value);
    } else if (a->args[i].type == STRINGV) {
      char** arr = (char**)a->args[i].value.stringv_value;
      for (size_t j = 0; arr[j] != nullptr; j++) {
        _xfree(arr[j]);
      }
      _xfree(arr);
    }
  }

  for (size_t i = 0; i < a->positional_arg_count; i++) {
    _xfree(a->positional_args[i]);
  }

  _xfree(a->positional_args);

  // reset state
  a->args_count = 0;

  a->positional_args = nullptr;
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
    size_t new_cap = a->_positional_arg_capacity*2;

    char** new_positional_args = realloc(a->positional_args, new_cap * sizeof(char*));

    if (!new_positional_args) {
      return false;
    }

    a->positional_args = new_positional_args;
    a->_positional_arg_capacity = new_cap;
  }

  a->positional_args[a->positional_arg_count] = strdup(arg);

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
      if (value_str != nullptr) {
        fprintf(stderr, "Boolean argument '%s' does not take a value\n", arg->name);
        return false;
      }

      arg->value.bool_value = true;
      break;
    }
    case STRING: {
      _xfree((char*)arg->value.string_value);
      arg->value.string_value = strdup(value_str);
      if (arg->value.string_value == nullptr) {
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
      size_t len = _null_term_array_len((const void**)arr);

      if (len + 1 >= arg->_stringv_capacity) {
        size_t new_cap = arg->_stringv_capacity * 2;
        char** new_arr = realloc(arr, new_cap * sizeof(char*));
        if (!new_arr) {
          fprintf(stderr, "Memory allocation failed for argument '%s'\n", arg->name);
          return false;
        }
        arr = new_arr;
        arg->value.stringv_value = (const char**)new_arr;
        arg->_stringv_capacity = new_cap;
      }

      char* copy = strdup(value_str);
      if (!copy) {
        fprintf(stderr, "Memory allocation failed for argument '%s'\n", arg->name);
        return false;
      }
      arr[len] = copy;
      arr[len + 1] = nullptr; // maintain null-termination
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

        size_t max_name_len = 0;
        for (size_t j = 0; j < a->args_count; j++) {
          size_t len = strlen(a->args[j].name);
          if (len > max_name_len) {
            max_name_len = len;
          }
        }

        for (size_t j = 0; j < a->args_count; j++) {
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
              if (a->args[j].value.stringv_value[0] != nullptr) {
                printf(" (appends to: [");
                for (size_t k = 0; a->args[j].value.stringv_value[k] != nullptr; k++) {
                  printf("%s", a->args[j].value.stringv_value[k]);
                  if (a->args[j].value.stringv_value[k + 1] != nullptr) {
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
    for (size_t j = 0; j < a->args_count; j++) {
      // -flag value syntax
      if (strcmp(a->args[j].name, arg) == 0) {
        found = true;
        char* value_str = nullptr;
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

      size_t candidate_len = strlen(a->args[j].name); 
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
      fprintf(stderr, "Expected no positional arguments, got %zu\n", a->positional_arg_count);
      return false;
    }
  } else if (strcmp(a->positional_args_req, "+") == 0) {
    if (a->positional_arg_count == 0) {
      fprintf(stderr, "Expected at least one positional argument\n");
      return false;
    }
  } else if (strcmp(a->positional_args_req, "?") == 0) {
    if (a->positional_arg_count > 1) {
      fprintf(stderr, "Expected at most one positional argument\n");
      return false;
    }
  } else if (strcmp(a->positional_args_req, "*") == 0) {
    // any number of positional arguments is allowed
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

    if (a->positional_arg_count != (size_t)expected) {
      fprintf(stderr, "Expected %ld positional arguments, got %zu\n", expected, a->positional_arg_count);
      return false;
    }
  }

  a->_parsed = true;
  return true;
}

#endif // ARGS_IMPLEMENTATION

#endif // ARGS_H
