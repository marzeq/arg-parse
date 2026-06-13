#ifndef ARGS_H
#define ARGS_H
/*
==============================================================================
Argument Parser
==============================================================================

A small, self-contained command-line argument parser.

Features:
  - Boolean flags
  - String arguments
  - Integer arguments
  - Default values
  - Positional argument validation
  - Automatic help generation (-h)

------------------------------------------------------------------------------
Basic Usage
------------------------------------------------------------------------------

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

    bool* verbose = add_arg(&a, "v", "Enable verbose output", bool);

    int* nproc = add_arg_default(&a, "nproc", "Number of processes", 4);

    const char** output = add_arg_default(&a, "output", "Output file", "out.txt");

3. Parse arguments.

    if (!args_parse(&a, argc, argv)) {
      args_free(&a);
      return 1;
    }

4. Handle help.

    if (a.got_help) {
      args_free(&a);
      return 0;
    }

5. Use parsed values.

    printf("verbose: %s\n", *verbose ? "true" : "false");
    printf("nproc: %d\n", *nproc);
    printf("output: %s\n", *output);

6. Free parser resources.

    args_free(&a);

------------------------------------------------------------------------------
Argument Types
------------------------------------------------------------------------------

Boolean flag:

    bool* verbose = add_arg(&a, "v", "Verbose output", bool);

Usage:

    prog -v

String argument:

    const char** output = add_arg(&a, "output", "Output file", const char*);

Usage:

    prog -output result.txt

Integer argument:

    int* nproc = add_arg(&a, "nproc", "Process count", int);

Usage:

    prog -nproc 8

------------------------------------------------------------------------------
Default Values
------------------------------------------------------------------------------

Arguments may be registered with defaults.

    int* nproc = add_arg_default(&a, "nproc", "Number of processes", 4);

If the user does not provide the argument:

    *nproc == 4

If the user provides:

    prog -nproc 16

Then:

    *nproc == 16

------------------------------------------------------------------------------
Argument State
------------------------------------------------------------------------------

Each registered argument contains two state flags:

    is_present
    is_set

is_present:
    The argument currently has a value.

    True when:
      - a default value exists
      - the user supplied a value

is_set:
    The user explicitly supplied the argument.

Example:

    add_arg_default(..., 4)

    prog

      is_present == true
      is_set     == false

    prog -nproc 8

      is_present == true
      is_set     == true

------------------------------------------------------------------------------
Positional Arguments
------------------------------------------------------------------------------

Positional arguments are stored in:

    a.positional_args
    a.positional_arg_count

Example:

    prog input.txt output.txt

Access:

    for (usz i = 0; i < a.positional_arg_count; i++) {
      printf("%s\n", a.positional_args[i]);
    }

Validation modes:

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

The parser automatically reserves:

    -h

Example:

    prog -h

Output:

    Usage: prog [options] [arg]
    Options:
      -v Verbose output
      -nproc Number of processes

When help is requested:

    a.got_help == true

------------------------------------------------------------------------------
Error Handling
------------------------------------------------------------------------------

args_parse() returns false when:

  - an unknown argument is encountered
  - a required argument value is missing
  - an integer is invalid
  - an integer is out of range
  - positional argument requirements are violated
  - memory allocation fails

Example:

    if (!args_parse(&a, argc, argv)) {
      args_free(&a);
      return 1;
    }

==============================================================================
*/

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>

typedef size_t usz;
#define nil NULL

#define _int_by_1_5(val) ((val) + (val) / 2)

typedef struct {
  void* ctx;

  void* (*alloc)(
    void* ctx,
    usz size
  );

  void* (*realloc)(
    void* ctx,
    void* ptr,
    usz new_size
  );

  void (*free)(
    void* ctx,
    void* ptr
  );
} _allocator;

typedef struct {
  void** allocations;
  usz count;
  usz capacity;
} _alloc_tracker;

bool _tracker_resize(_alloc_tracker* tracker) {
  usz new_capacity = _int_by_1_5(tracker->capacity);

  void** new_allocations = realloc(tracker->allocations, sizeof(void*) * new_capacity);

  if (new_allocations == nil) {
    return false;
  }

  tracker->allocations = new_allocations;
  tracker->capacity = new_capacity;

  return true;
}

bool _track_ptr(_alloc_tracker* tracker, void* ptr) {
  if (ptr == nil) {
    return false;
  }

  if (tracker->allocations == nil) {
    tracker->capacity = 16;

    tracker->allocations = malloc(sizeof(void*) * tracker->capacity);

    if (tracker->allocations == nil) {
      tracker->capacity = 0;
      return false;
    }
  }

  if (tracker->count >= tracker->capacity) {
    if (!_tracker_resize(tracker)) {
      return false;
    }
  }

  tracker->allocations[tracker->count++] = ptr;

  return true;
}

void _untrack_ptr(_alloc_tracker* tracker, void* ptr) {
  for (usz i = 0; i < tracker->count; i++) {
    if (tracker->allocations[i] == ptr) {
      tracker->allocations[i] = tracker->allocations[tracker->count - 1];

      tracker->count -= 1;
      return;
    }
  }
}

void* _tracked_alloc(void* ctx, usz size) {
  _alloc_tracker* tracker = ctx;

  void* ptr = malloc(size);

  if (ptr == nil) {
    return nil;
  }

  if (!_track_ptr(tracker, ptr)) {
    free(ptr);
    return nil;
  }

  return ptr;
}

void* _tracked_realloc(void* ctx, void* ptr, usz new_size) {
  _alloc_tracker* tracker = ctx;

  if (ptr == nil) {
    void* new_ptr = malloc(new_size);

    if (new_ptr == nil) {
      return nil;
    }

    if (!_track_ptr(tracker, new_ptr)) {
      free(new_ptr);
      return nil;
    }

    return new_ptr;
  }

  for (usz i = 0; i < tracker->count; i++) {
    if (tracker->allocations[i] == ptr) {
      void* new_ptr = realloc(ptr, new_size);

      if (new_ptr == nil) {
        return nil;
      }

      tracker->allocations[i] = new_ptr;

      return new_ptr;
    }
  }

  return nil;
}

void _tracked_free(void* ctx, void* ptr) {
  _alloc_tracker* tracker = ctx;

  _untrack_ptr(tracker, ptr);

  free(ptr);
}

_allocator _tracked_allocator(_alloc_tracker* tracker) {
  return (_allocator) {
    .ctx = tracker,
    .alloc = _tracked_alloc,
    .realloc = _tracked_realloc,
    .free = _tracked_free,
  };
}

void _tracker_free_all(_alloc_tracker* tracker) {
  for (usz i = 0; i < tracker->count; i++) {
    free(tracker->allocations[i]);
  }

  free(tracker->allocations);

  tracker->allocations = nil;
  tracker->count = 0;
  tracker->capacity = 0;
}

typedef enum arg_type {
  BOOL,
  STRING,
  NUMBER,
} arg_type;

typedef struct {
  const char* name;
  const char* desc;
  arg_type type;
  union {
    bool bool_value;
    const char* string_value;
    int number_value;
  } value;
  bool is_set;
  bool is_present;
} arg;

const usz init_arg_cap = 4;
const usz arg_cap_increment = 4;

typedef struct {
  arg* args;
  usz count;
  usz capacity;
  char** positional_args;
  usz positional_arg_count;
  usz positional_arg_capacity;
  const char* positional_args_req;
  _allocator self_alloc;
  _alloc_tracker self_tracker;
  bool alloc_initialised;
  bool got_help;
} args;

char* a_strdup(_allocator a, const char* str) {
  size_t len = strlen(str);
  char* copy = a.alloc(a.ctx, len + 1);
  if (copy) {
    memcpy(copy, str, len + 1);
  }
  return copy;
}

#define add_arg(a, name, description, ctype) \
  _Generic(*(ctype*)0, \
    bool: add_arg_bool, \
    char*: add_arg_string, \
    const char*: add_arg_string, \
    int: add_arg_number \
  )(a, name, description)

void* _add_arg(args* ar, const char* name, const char* description, arg_type type) {
  if (name == nil || description == nil) {
    return nil;
  }

  if (name[0] == '-' && strcmp(name+1, "h") == 0) {
    fprintf(stderr, "'-h' is reserved for help\n");
    return nil;
  }

  if (!ar->alloc_initialised) {
    ar->self_tracker = (_alloc_tracker){};
    ar->self_alloc = _tracked_allocator(&ar->self_tracker);
    ar->alloc_initialised = true;
  }

  _allocator a = ar->self_alloc;

  if (!ar->args) {
    ar->args = a.alloc(a.ctx, init_arg_cap * sizeof(arg));
    if (!ar->args) {
      return nil;
    }
    ar->capacity = init_arg_cap;
    ar->count = 0;
  } else if (ar->count >= ar->capacity) {
    usz new_cap = ar->capacity + arg_cap_increment;
    arg* new_args = a.realloc(a.ctx, ar->args, new_cap * sizeof(arg));
    if (!new_args) {
      return nil;
    }
    ar->args = new_args;
    ar->capacity = new_cap;
  }

  for (usz i = 0; i < ar->count; i++) {
    if (strcmp(ar->args[i].name, name) == 0) {
      fprintf(stderr, "Duplicate argument name: %s\n", name);
      return nil;
    }
  }

  ar->args[ar->count].name = a_strdup(ar->self_alloc, name);
  if (!ar->args[ar->count].name) {
    return nil;
  }
  ar->args[ar->count].type = type;
  ar->args[ar->count].is_set = false;
  ar->args[ar->count].is_present = false;
  ar->args[ar->count].desc = a_strdup(ar->self_alloc, description);
  if (!ar->args[ar->count].desc) {
    return nil;
  }
  return &ar->args[ar->count++].value;
}

bool* add_arg_bool(args* a, const char* name, const char* description) {
  return (bool*)_add_arg(a, name, description, BOOL);
}

const char** add_arg_string(args* a, const char* name, const char* description) {
  return (const char**)_add_arg(a, name, description, STRING);
}

int* add_arg_number(args* a, const char* name, const char* description) {
  return (int*)_add_arg(a, name, description, NUMBER);
}

#define add_arg_default(a, name, description, def) \
  _Generic((def), \
    char*: _add_arg_default_string, \
    const char*: _add_arg_default_string, \
    int: _add_arg_default_int \
  )(a, name, description, def)

const char** _add_arg_default_string(args* a, const char* name, const char* description, const char* def) {
  void* got = _add_arg(a, name, description, STRING);
  if (!got) {
    return nil;
  }
  a->args[a->count - 1].value.string_value = a_strdup(a->self_alloc, def);
  if (!a->args[a->count - 1].value.string_value) {
    return nil;
  }
  a->args[a->count - 1].is_present = true;
  return (const char**)got;
}

int* _add_arg_default_int(args* a, const char* name, const char* description, int def) {
  void* got = _add_arg(a, name, description, NUMBER);
  if (!got) {
    return nil;
  }
  a->args[a->count - 1].value.number_value = def;
  a->args[a->count - 1].is_present = true;
  return (int*)got;
}

void args_free(args* a) {
  if (a->alloc_initialised) {
    _tracker_free_all(&a->self_tracker);
  }
}

bool is_flag(const char* arg) {
  return arg[0] == '-' && arg[1] != '\0';
}

bool add_positional_arg(args* a, const char* arg) {
  if (!a->positional_args) {
    a->positional_args = malloc(init_arg_cap * sizeof(char*));
    if (!a->positional_args) {
      return false;
    }
    a->positional_arg_capacity = init_arg_cap;
    a->positional_arg_count = 0;
  } else if (a->positional_arg_count >= a->positional_arg_capacity) {
    usz new_cap = a->positional_arg_capacity + arg_cap_increment;

    char** new_positional_args = realloc(a->positional_args, new_cap * sizeof(char*));

    if (!new_positional_args) {
      return false;
    }

    a->positional_args = new_positional_args;
    a->positional_arg_capacity = new_cap;
  }

  a->positional_args[a->positional_arg_count] = a_strdup(a->self_alloc, arg);

  if (!a->positional_args[a->positional_arg_count]) {
    return false;
  }

  a->positional_arg_count++;
  return true;
}

bool args_parse(args* a, int argc, char** argv) {
  for (int i = 1; i < argc; i++) {
    char* arg = argv[i];
    if (!is_flag(arg)) {
      if (!add_positional_arg(a, arg)) {
        return false;
      }
      continue;
    }

    arg++; // skip the leading '-'
    
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
      printf("Options:\n");
      for (usz j = 0; j < a->count; j++) {
        printf("  -%s %s\n", a->args[j].name, a->args[j].desc);
      }
      a->got_help = true;
      return true;
    }

    bool found = false;
    for (usz j = 0; j < a->count; j++) {
      if (strcmp(a->args[j].name, arg) == 0) {
        found = true;
        switch (a->args[j].type) {
          case BOOL: {
            a->args[j].value.bool_value = true;
            break;
          }
          case STRING: {
            if (i + 1 >= argc) {
              fprintf(stderr, "Expected value for argument: %s\n", arg);
              return false;
            }
            a->args[j].value.string_value = a_strdup(a->self_alloc, argv[++i]);
            if (!a->args[j].value.string_value) {
              return false;
            }
            break;
          }
          case NUMBER: {
            if (i + 1 >= argc) {
              fprintf(stderr, "Expected value for argument: %s\n", arg);
              return false;
            }

            char *end;
            errno = 0;

            long value = strtol(argv[++i], &end, 10);

            if (end == argv[i]) {
              fprintf(stderr, "Invalid integer for argument '%s': %s\n", arg, argv[i]);
              return false;
            }

            if (*end != '\0') {
              fprintf(stderr, "Invalid integer for argument '%s': %s\n", arg, argv[i]);
              return false;
            }

            if (errno == ERANGE || value < INT_MIN || value > INT_MAX) {
              fprintf(stderr, "Integer out of range for argument '%s': %s\n", arg, argv[i]);
              return false;
            }

            a->args[j].value.number_value = (int)value;
            break;
          }
        }
        a->args[j].is_set = true;
        a->args[j].is_present = true;
        break;
      }
    }

    if (!found) {
      fprintf(stderr, "Unknown argument: %s\n", arg);
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

  return true;
}
#endif // ARGS_H
