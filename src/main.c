#include "parser.h"
#include "optimizer.h"
#include "bytecode.h"

#include <stdio.h>
#include <stdlib.h>

// TODO: more optimizations
// TODO: compiler

static void usage(int argc, char *argv[]) {
    printf("Usage: %s [parameter-names...] code\n", argc > 0 ? argv[0] : "parser_example");
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        usage(argc, argv);
        return 1;
    }

    const char *code = argv[argc - 1];
    long *args = NULL;
    struct Parser parser = parse_string(code, &argv[1], argc - 2);
    int status = 0;

    if (parser.error != ERROR_NONE) {
        parser_print_error(&parser, stderr);
        goto error;
    }

    args = calloc(argc - 2, sizeof(long));
    if (args == NULL) {
        perror("allocating arguments");
        return 1;
    }

    for (int argind = 1; argind < argc - 1; ++ argind) {
        const char *name = argv[argind];
        const char *strvalue = getenv(name);
        if (strvalue == NULL) {
            fprintf(stderr, "Error: Environment variable not set: %s\n", name);
            usage(argc, argv);
            goto error;
        }

        char *endptr = NULL;
        const long value = strtol(strvalue, &endptr, 10);

        if (!*strvalue || *endptr) {
            fprintf(stderr, "Error: Environment variable is not a long integer: %s=%s\n", name, strvalue);
            usage(argc, argv);
            goto error;
        }

        args[argind - 1] = value;
    }

    putchar('(');
    for (int argind = 1; argind < argc - 1;) {
        printf("%s", argv[argind]);
        ++ argind;
        if (argind < argc - 1) {
            printf(", ");
        }
    }
    printf(") -> %s\n\n", code);

    long value;
    printf("AST\n");
    printf("---\n");

    printf("Parsed AST: ");
    ast_print(&parser.ast, &argv[1], stdout);
    value = ast_eval(&parser.ast, args);
    printf(" = %ld\n", value);

    printf("Optimized AST: ");
    optimize(&parser.ast);
    ast_print(&parser.ast, &argv[1], stdout);
    value = ast_eval(&parser.ast, args);
    printf(" = %ld\n\n", value);

    printf("Byte Code\n");
    printf("---------\n");
    struct Bytecode bytecode = bytecode_compile(&parser.ast);
    if (bytecode.stack_size == 0) {
        fprintf(stderr, "Error\n"); // TODO: better error messages
    } else {
        bytecode_print(bytecode.bytes.data, &argv[1], stdout);
        value = bytecode_eval(bytecode.bytes.data, args);
        printf("\nresult = %ld\n", value);
    }

    bytecode_destroy(&bytecode);

    goto cleanup;

error:
    status = 1;

cleanup:
    parser_destroy(&parser);
    free(args);

    return status;
}
