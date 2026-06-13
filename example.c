#include "args.h"

int main(int argc, char** argv) {
  args a = {};
  a.positional_args_req = "?";
  int* nproc = add_arg_default(&a, "nproc", "Number of processes", 4);
  bool* verbose = add_arg(&a, "v", "Verbose", bool);

  if (!args_parse(&a, argc, argv)) {
    args_free(&a);
    return 1;
  }
  if (a.got_help) {
    args_free(&a);
    return 0;
  }

  if (*verbose) {
    for (usz i = 0; i < a.count; i++) {
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
      }
    }

    for (usz i = 0; i < a.positional_arg_count; i++) {
      printf("%s\n", a.positional_args[i]);
    }
  }

  printf("Number of processes: %d\n", *nproc);

  args_free(&a);
  return 0;
}
