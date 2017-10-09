#include <stdio.h>
#include "run.h"

int main(int argc, char** argv)
{
    if (argc != 2) {
        puts("Supported syntax: ./program filename");
        return 1;
    }
    const char* filename = argv[1];

    run_file(filename);

    return 0;
}