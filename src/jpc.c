#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
    int result = system("gcc hello.c -o hello");

    if (result != 0) {
        fprintf(stderr, "Compilation failed with code %d\n", result);
        return result;
    }

    return 0;
}