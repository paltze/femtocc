#include "as.h"

int main(int argc, char* argv[]) {
    if (argc < 3) {
        printf("Please pass a asm file to assemble\n");
        return 0;
    }

    FILE* out = fopen(argv[2], "w");
    as_assemble(argv[1], out);

    fclose(out);

    return 0;
}
