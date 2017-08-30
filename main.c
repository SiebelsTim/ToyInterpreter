#include <stdio.h>
#include "run.h"

int main(int argc, char** argv)
{
    if (argc != 2) {
        puts("Supported syntax: ./program filename");
        return 1;
    }
    const char* filename = argv[1];
    FILE* handle = fopen(filename, "r");

    run_file(handle);

    return 0;
}