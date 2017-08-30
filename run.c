#include "run.h"
#include "parse.h"

void run_file(FILE* file) {
    AST* ast = parse(file);
    print_ast(ast, 0);
    destroy_ast(ast);
}
