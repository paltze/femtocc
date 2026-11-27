#ifndef AS_H
#define AS_H

#include <stdio.h>
#include <stdint.h>

#define u16 uint16_t
#define u8 uint8_t

typedef struct {
    char name[64];
    u16 addr;
} Label;

typedef enum {
    R,
    I,
    M,
    J,
    P // For pseudo
} InstrType;

typedef struct {
    char mnemonic[16];
    int count;
    InstrType type;
    u8 id;
} Instr;

void as_build_labels(FILE* fp, Label* labels, int* label_size);
void as_build_output(FILE* in, FILE* out, Label* labels, int label_count);

#endif
