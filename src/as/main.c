#include "as.h"

int main(int argc, char* argv[]) {
    if (argc < 3) {
        printf("Please pass a asm file to assemble\n");
        return 0;
    }

    FILE* in = fopen(argv[1], "r");
    FILE* out = fopen(argv[2], "w");

    Label labels[256];
    int label_count = 0;

    as_build_labels(in, labels, &label_count);
    as_build_output(in, out, labels, label_count);

    fclose(in);
    fclose(out);

    return 0;
}
