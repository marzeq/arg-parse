#define ARGS_IMPLEMENTATION
#define ARGS_MAX_ARGS 4
#include "args.h"

#include <stdio.h>

int main(int argc, char** argv) {
  args a = {};
  a.positional_args_req = "?";
  int* nproc = add_arg(&a, "nproc", "Number of processes", 4);
  const char** output = add_arg(&a, "output", "Output file", (const char*)nullptr);
  bool* verbose = add_arg(&a, "v", "Verbose", false);
  char* default_test[] = {"default1", "default2", nullptr};
  const char*** test = add_arg(&a, "t", "Test string array", (const char**)default_test);

  if (a.failed_adding) {
    args_reset(&a);
    return 1;
  }

  if (!args_parse(&a, argc, argv)) {
    args_reset(&a);
    return 1;
  }
  if (a.got_help) {
    print_help(&a, argv[0]);
    args_reset(&a);
    return 0;
  }

  if (*verbose) {
    for (size_t i = 0; i < a.args_count; i++) {
      arg* argument = &a.args[i];
      printf("-%s ", argument->name);
      switch (argument->type) {
        case BOOL:
          printf("%s\n", argument->value.bool_value ? "true" : "false");
          break;
        case STRING:
          printf("%s\n", argument->value.string_value);
          break;
        case NUMBER:
          printf("%d\n", argument->value.number_value);
          break;
        case STRINGV: {
          printf("[");
          for (size_t j = 0; argument->value.stringv_value[j] != nullptr; j++) {
            printf("%s", argument->value.stringv_value[j]);
            if (argument->value.stringv_value[j + 1] != nullptr) {
              printf(", ");
            }
          }
          printf("]\n");
          break;
        }
      }
    }

    printf("# Positional Arguments:\n");
    for (size_t i = 0; i < a.positional_arg_count; i++) {
      printf("%s\n", a.positional_args[i]);
    }
  }

  printf("Number of processes: %d\n", *nproc);
  if (a.positional_arg_count > 0) {
    printf("First positional argument: %s\n", a.positional_args[0]);
  }
  printf("Verbose argument is set: %s\n", arg_is_set(verbose) ? "true" : "false");
  for (size_t i = 0; (*test)[i] != nullptr; i++) {
    printf("Test string array element %zu: %s\n", i, (*test)[i]);
  }

  args_reset(&a);
  return 0;
}
