#ifndef AS_H
#define AS_H

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

typedef uint8_t  u8;
typedef uint16_t u16;

#define MAX_LABELS        1024
#define MAX_EQUS          512
#define MAX_INCLUDE_DEPTH 32

typedef enum { R, I, M, J, P } InstrType;

typedef struct {
    const char* mnemonic;
    int count;
    InstrType type;
    int id;
} Instr;

typedef struct {
    char name[256];
    u16 addr;
    int is_data;
} Label;

typedef struct {
    char name[256];
    int value;
} Equ;

typedef struct {
    u8* data;
    size_t size;
    size_t cap;
} ByteBuf;

typedef struct {
    Label labels[MAX_LABELS];
    int label_count;

    Equ equs[MAX_EQUS];
    int equ_count;

    int pending_labels[MAX_LABELS];
    int pending_count;

    u16 code_addr;
    u16 data_addr;

    const char* include_stack[MAX_INCLUDE_DEPTH];
    int include_depth;

    ByteBuf data_buf;
} AsmState;

int as_assemble(const char* in_path, FILE* out);

#endif
