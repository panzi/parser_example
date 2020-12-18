#include "parser.h"
#include "bytecode.h"

#include <stdio.h>

// TODO: more optimizations
// TODO: compiler

int main(int argc, char *argv[]) {
    for (int index = 1; index < argc; ++ index) {
        int linelen = printf("Argument %d: %s\n", index, argv[index]) - 1;
        while (linelen -- > 0) {
            putchar('-');
        }
        putchar('\n');

        struct Parser parser = parser_create_from_string(argv[index]);

        if (parser.error != ERROR_NONE) {
            parser_print_error(&parser, stderr);
        } else {
            printf("# AST: ");
            ast_print(&parser.ast, stdout);
            long value = ast_eval(&parser.ast);
            printf(" = %ld\n\n", value);

            printf("# Byte Code:\n\n");
            struct Bytecode bytecode = bytecode_compile(&parser.ast);
            if (bytecode.stack_size == 0) {
                fprintf(stderr, "Error\n"); // TODO: better error messages
            } else {
                bytecode_print(bytecode.bytes.data, stdout);
                value = bytecode_eval(bytecode.bytes.data);
                printf("-> %ld\n", value);
            }

            bytecode_destroy(&bytecode);
        }
        putchar('\n');

        parser_destroy(&parser);
    }
    return 0;
}
