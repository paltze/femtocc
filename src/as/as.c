#include "as.h"

#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#define lo(x) ((u8)((x) & 0xFF))
#define hi(x) ((u8)(((x) >> 8) & 0xFF))

static const Instr instrs[] = {
    {"ADD",    1, R, 0 },
    {"SUB",    1, R, 1 },
    {"AND",    1, R, 2 },
    {"OR",     1, R, 3 },
    {"XOR",    1, R, 4 },
    {"NOT",    1, R, 5 },
    {"SHL",    1, R, 6 },
    {"SHR",    1, R, 7 },
    {"MOVI",   1, I, 8 },
    {"LOAD",   1, M, 9 },
    {"STORE",  1, M, 10},
    {"JMP",    3, J, 11},
    {"JZ",     3, J, 12},
    {"JNZ",    3, J, 13},
    {"CALL",   3, J, 14},
    {"RET",    1, J, 15},
    {"PUSH",   3, P, 16},
    {"POP",    3, P, 17},
    {"PUSHI",  4, P, 18},
    {"MOV",    1, P, 19},
    {"INC",    2, P, 20},
    {"DEC",    2, P, 21},
    {"CLR",    1, P, 22},
    {"JE",     5, P, 23},
    {"JNE",    5, P, 24},
    {"JLT",    6, P, 25},
    {"JGT",    4, P, 26},
    {"LOADD",  3, P, 27},
    {"STORED", 3, P, 28}
};

static const int instr_size = sizeof(instrs) / sizeof(Instr);

/* ---------- Helpers ---------- */

static void buf_push(ByteBuf* b, u8 byte) {
    if (b->size == b->cap) {
        b->cap = b->cap ? b->cap * 2 : 256;
        b->data = realloc(b->data, b->cap);
    }
    b->data[b->size++] = byte;
}

static void strip_comment(char* line) {
    int in_str = 0;
    for (int i = 0; line[i]; i++) {
        if (line[i] == '"' && (i == 0 || line[i - 1] != '\\'))
            in_str = !in_str;
        else if (line[i] == ';' && !in_str) {
            line[i] = '\0';
            return;
        }
    }
}

static char* trim(char* s) {
    while (isspace((unsigned char)*s)) s++;
    char* end = s + strlen(s);
    while (end > s && isspace((unsigned char)end[-1])) *--end = '\0';
    return s;
}

static const Instr* find_instr(const char* mnemonic) {
    for (int i = 0; i < instr_size; i++)
        if (!strcmp(mnemonic, instrs[i].mnemonic))
            return &instrs[i];
    return NULL;
}

static int find_label(u16* target, const char* name, const AsmState* st) {
    for (int i = 0; i < st->label_count; i++) {
        if (!strcmp(name, st->labels[i].name)) {
            *target = st->labels[i].addr;
            return 1;
        }
    }
    return 0;
}

static int find_equ(int* out_val, const char* name, const AsmState* st) {
    for (int i = 0; i < st->equ_count; i++) {
        if (!strcmp(name, st->equs[i].name)) {
            *out_val = st->equs[i].value;
            return 1;
        }
    }
    return 0;
}

static int parse_value(const char* raw, const AsmState* st, int allow_labels) {
    char buf[256];
    strncpy(buf, raw, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    char* s = trim(buf);

    int val;
    if (find_equ(&val, s, st))
        return val;

    if (allow_labels) {
        u16 addr;
        if (find_label(&addr, s, st))
            return (int)addr;
    }

    if (!strncasecmp(s, "0b", 2))
        return (int)strtol(s + 2, NULL, 2);

    return (int)strtol(s, NULL, 0);
}

static int parse_reg(const char* raw) {
    char buf[16];
    strncpy(buf, raw, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    char* s = trim(buf);

    if (!strcasecmp(s, "SP")) return 14;
    if (!strcasecmp(s, "FP")) return 11;
    if (!strcasecmp(s, "SR")) return 13;
    if (toupper((unsigned char)s[0]) == 'R')
        return atoi(s + 1);

    return -1;
}

/* Extracts a quoted string, handling basic escapes.
   If out_buf is non-NULL, appends bytes to it. Returns byte count or -1 on error. */
static int parse_quoted_string(const char* args, ByteBuf* out_buf) {
    const char* p = strchr(args, '"');
    if (!p) return -1;
    p++;

    int len = 0;
    while (*p && *p != '"') {
        u8 ch;
        if (*p == '\\' && p[1]) {
            p++;
            switch (*p) {
                case 'n':  ch = '\n'; break;
                case 'r':  ch = '\r'; break;
                case 't':  ch = '\t'; break;
                case '0':  ch = '\0'; break;
                case '\\': ch = '\\'; break;
                case '"':  ch = '"';  break;
                default:   ch = (u8)*p; break;
            }
        } else {
            ch = (u8)*p;
        }
        if (out_buf) buf_push(out_buf, ch);
        len++;
        p++;
    }

    if (*p != '"') return -1;
    return len;
}

static int extract_filename(const char* args, char* out, size_t out_size) {
    const char* start = strchr(args, '"');
    if (!start) return 0;
    start++;
    const char* end = strchr(start, '"');
    if (!end || (size_t)(end - start) >= out_size) return 0;

    memcpy(out, start, (size_t)(end - start));
    out[end - start] = '\0';
    return 1;
}

static int count_csv_items(const char* args) {
    int count = 0;
    const char* p = args;
    while (*p) {
        while (isspace((unsigned char)*p)) p++;
        if (!*p) break;
        count++;
        while (*p && *p != ',') p++;
        if (*p == ',') p++;
    }
    return count;
}

static void bind_pending_labels(AsmState* st, int is_data) {
    u16 target = is_data ? st->data_addr : st->code_addr;
    for (int i = 0; i < st->pending_count; i++) {
        int idx = st->pending_labels[i];
        st->labels[idx].addr = target;
        st->labels[idx].is_data = is_data;
    }
    st->pending_count = 0;
}

static int push_include(AsmState* st, const char* path) {
    for (int i = 0; i < st->include_depth; i++) {
        if (!strcmp(st->include_stack[i], path)) {
            printf("Error: circular include detected for '%s'\n", path);
            return -1;
        }
    }
    if (st->include_depth >= MAX_INCLUDE_DEPTH) {
        printf("Error: max include depth exceeded at '%s'\n", path);
        return -1;
    }
    st->include_stack[st->include_depth++] = path;
    return 0;
}

/* ---------- Pass 1: Labels, .equ, Sizes ---------- */

static int pass1_file(const char* path, AsmState* st) {
    if (push_include(st, path) < 0)
        return -1;

    FILE* fp = fopen(path, "r");
    if (!fp) {
        printf("Error: cannot open file '%s'\n", path);
        st->include_depth--;
        return -1;
    }

    char line[256];
    char token[256];

    while (fgets(line, sizeof(line), fp)) {
        strip_comment(line);
        char* p = trim(line);
        if (!*p) continue;

        if (sscanf(p, "%255s", token) <= 0)
            continue;

        size_t tlen = strlen(token);
        char* args = trim(p + tlen);

        /* Label */
        if (token[tlen - 1] == ':') {
            token[tlen - 1] = '\0';
            int idx = st->label_count++;
            strcpy(st->labels[idx].name, token);
            st->pending_labels[st->pending_count++] = idx;
            continue;
        }

        /* Directives */
        if (token[0] == '.') {
            if (!strcasecmp(token, ".include")) {
                char inc_path[256];
                if (!extract_filename(args, inc_path, sizeof(inc_path))) {
                    printf("Error: invalid .include syntax in '%s'\n", path);
                    fclose(fp);
                    st->include_depth--;
                    return -1;
                }
                if (pass1_file(inc_path, st) < 0) {
                    fclose(fp);
                    st->include_depth--;
                    return -1;
                }
            }
            else if (!strcasecmp(token, ".equ")) {
                char name[256], val_s[256];
                if (sscanf(args, "%255[^,], %255s", name, val_s) != 2) {
                    printf("Error: invalid .equ syntax: %s\n", args);
                    fclose(fp);
                    st->include_depth--;
                    return -1;
                }
                strcpy(st->equs[st->equ_count].name, trim(name));
                st->equs[st->equ_count].value = parse_value(val_s, st, 0);
                st->equ_count++;
            }
            else if (!strcasecmp(token, ".byte")) {
                bind_pending_labels(st, 1);
                st->data_addr += count_csv_items(args);
            }
            else if (!strcasecmp(token, ".word")) {
                bind_pending_labels(st, 1);
                st->data_addr += 2 * count_csv_items(args);
            }
            else if (!strcasecmp(token, ".zero")) {
                bind_pending_labels(st, 1);
                st->data_addr += parse_value(args, st, 0);
            }
            else if (!strcasecmp(token, ".fill")) {
                char count_s[128], val_s[128];
                if (sscanf(args, "%127[^,], %127s", count_s, val_s) != 2) {
                    printf("Error: invalid .fill syntax: %s\n", args);
                    fclose(fp);
                    st->include_depth--;
                    return -1;
                }
                bind_pending_labels(st, 1);
                st->data_addr += parse_value(count_s, st, 0);
            }
            else if (!strcasecmp(token, ".ascii") || !strcasecmp(token, ".asciiz")) {
                bind_pending_labels(st, 1);
                int len = parse_quoted_string(args, NULL);
                if (len < 0) {
                    printf("Error: malformed string in %s\n", token);
                    fclose(fp);
                    st->include_depth--;
                    return -1;
                }
                st->data_addr += len + (!strcasecmp(token, ".asciiz") ? 1 : 0);
            }
            else if (!strcasecmp(token, ".incbin")) {
                char bin_path[256];
                if (!extract_filename(args, bin_path, sizeof(bin_path))) {
                    printf("Error: invalid .incbin syntax\n");
                    fclose(fp);
                    st->include_depth--;
                    return -1;
                }
                FILE* bf = fopen(bin_path, "rb");
                if (!bf) {
                    printf("Error: cannot open binary '%s'\n", bin_path);
                    fclose(fp);
                    st->include_depth--;
                    return -1;
                }
                fseek(bf, 0, SEEK_END);
                long fsize = ftell(bf);
                fclose(bf);

                bind_pending_labels(st, 1);
                st->data_addr += (u16)fsize;
            }
            else {
                printf("Error: unknown directive '%s'\n", token);
                fclose(fp);
                st->include_depth--;
                return -1;
            }
            continue;
        }

        /* Instruction */
        const Instr* instr = find_instr(token);
        if (!instr) {
            printf("Error: unknown instruction '%s'\n", token);
            fclose(fp);
            st->include_depth--;
            return -1;
        }

        bind_pending_labels(st, 0);
        st->code_addr += 2 * instr->count;
    }

    fclose(fp);
    st->include_depth--;
    return 0;
}

/* ---------- Pass 2: Code Emission & Data Collection ---------- */

static int pass2_file(const char* path, FILE* out, AsmState* st) {
    if (push_include(st, path) < 0)
        return -1;

    FILE* fp = fopen(path, "r");
    if (!fp) {
        st->include_depth--;
        return -1;
    }

    char line[256];
    char token[256];
    char r1[16], r2[16], r3[16], imm_s[64], label[256];

    while (fgets(line, sizeof(line), fp)) {
        strip_comment(line);
        char* p = trim(line);
        if (!*p) continue;

        if (sscanf(p, "%255s", token) <= 0)
            continue;

        size_t tlen = strlen(token);
        if (token[tlen - 1] == ':')
            continue;

        char* args = trim(p + tlen);

        /* Directives */
        if (token[0] == '.') {
            if (!strcasecmp(token, ".include")) {
                char inc_path[256];
                extract_filename(args, inc_path, sizeof(inc_path));
                if (pass2_file(inc_path, out, st) < 0) {
                    fclose(fp);
                    st->include_depth--;
                    return -1;
                }
            }
            else if (!strcasecmp(token, ".equ")) {
                continue;
            }
            else if (!strcasecmp(token, ".byte") || !strcasecmp(token, ".word")) {
                int is_word = !strcasecmp(token, ".word");
                char copy[256];
                strncpy(copy, args, sizeof(copy) - 1);
                copy[sizeof(copy) - 1] = '\0';

                char* item = strtok(copy, ",");
                while (item) {
                    int val = parse_value(item, st, 1);
                    if (is_word) {
                        buf_push(&st->data_buf, lo(val));
                        buf_push(&st->data_buf, hi(val));
                    } else {
                        buf_push(&st->data_buf, lo(val));
                    }
                    item = strtok(NULL, ",");
                }
            }
            else if (!strcasecmp(token, ".zero")) {
                int count = parse_value(args, st, 0);
                for (int i = 0; i < count; i++)
                    buf_push(&st->data_buf, 0);
            }
            else if (!strcasecmp(token, ".fill")) {
                char count_s[128], val_s[128];
                sscanf(args, "%127[^,], %127s", count_s, val_s);
                int count = parse_value(count_s, st, 0);
                u8 val = lo(parse_value(val_s, st, 1));
                for (int i = 0; i < count; i++)
                    buf_push(&st->data_buf, val);
            }
            else if (!strcasecmp(token, ".ascii") || !strcasecmp(token, ".asciiz")) {
                parse_quoted_string(args, &st->data_buf);
                if (!strcasecmp(token, ".asciiz"))
                    buf_push(&st->data_buf, 0);
            }
            else if (!strcasecmp(token, ".incbin")) {
                char bin_path[256];
                extract_filename(args, bin_path, sizeof(bin_path));
                FILE* bf = fopen(bin_path, "rb");
                int c;
                while ((c = fgetc(bf)) != EOF)
                    buf_push(&st->data_buf, (u8)c);
                fclose(bf);
            }
            continue;
        }

        /* Instructions */
        const Instr* instr = find_instr(token);
        if (!instr) {
            printf("Unknown instruction: %s\n", token);
            fclose(fp);
            st->include_depth--;
            return -1;
        }

        if (instr->type == R) {
            sscanf(args, "%15[^,], %15[^,], %15s", r1, r2, r3);
            int d = parse_reg(r1), s = parse_reg(r2), t = parse_reg(r3);
            fprintf(out, "%1hX%1hX%1hX%1hX\n", instr->id, d, s, t);
        }
        else if (instr->type == I) {
            sscanf(args, "%15[^,], %63s", r1, imm_s);
            int d = parse_reg(r1), imm = parse_value(imm_s, st, 0);
            fprintf(out, "%1hX%1hX%02hX\n", instr->id, d, lo(imm));
        }
        else if (instr->type == M) {
            sscanf(args, "%15[^,], %15[^,], %15s", r1, r2, r3);
            int d = parse_reg(r1), rhi = parse_reg(r2), rlo = parse_reg(r3);
            fprintf(out, "%1hX%1hX%1hX%1hX\n", instr->id, d, rhi, rlo);
        }
        else if (instr->type == J) {
            if (instr->id == 15) {
                fprintf(out, "F000\n");
                continue;
            }

            int rs = 0;
            if (instr->id == 12 || instr->id == 13) {
                sscanf(args, "%255[^,], %15s", label, r1);
                rs = parse_reg(r1);
            } else {
                sscanf(args, "%255s", label);
            }

            u16 addr;
            if (!find_label(&addr, trim(label), st)) {
                printf("Unknown label: %s\n", label);
                fclose(fp);
                st->include_depth--;
                return -1;
            }

            fprintf(out, "8C%02hX\n", lo(addr));
            fprintf(out, "8F%02hX\n", hi(addr));
            fprintf(out, "%1hXFC%1hX\n", instr->id, rs);
        }
        /* Pseudo instructions */
        else if (instr->id == 16) { /* PUSH */
            sscanf(args, "%15s", r1);
            fprintf(out, "8F01\n1EEF\nA%1hXEE\n", parse_reg(r1));
        }
        else if (instr->id == 17) { /* POP */
            sscanf(args, "%15s", r1);
            fprintf(out, "9%1hXEE\n8F01\n0EEF\n", parse_reg(r1));
        }
        else if (instr->id == 18) { /* PUSHI */
            sscanf(args, "%63s", imm_s);
            u8 imm = lo(parse_value(imm_s, st, 0));
            fprintf(out, "8C%02hX\n8F01\n1EEF\nACEE\n", imm);
        }
        else if (instr->id == 19) { /* MOV */
            sscanf(args, "%15[^,], %15s", r1, r2);
            int rd = parse_reg(r1), rs = parse_reg(r2);
            fprintf(out, "2%1hX%1hX%1hX\n", rd, rs, rs);
        }
        else if (instr->id == 20) { /* INC */
            sscanf(args, "%15s", r1);
            int rs = parse_reg(r1);
            fprintf(out, "8F01\n0%1hX%1hXF\n", rs, rs);
        }
        else if (instr->id == 21) { /* DEC */
            sscanf(args, "%15s", r1);
            int rs = parse_reg(r1);
            fprintf(out, "8F01\n1%1hX%1hXF\n", rs, rs);
        }
        else if (instr->id == 22) { /* CLR */
            sscanf(args, "%15s", r1);
            fprintf(out, "8%1hX00\n", parse_reg(r1));
        }
        else if (instr->id == 23 || instr->id == 24) { /* JE / JNE */
            sscanf(args, "%255[^,], %15[^,], %15s", label, r1, r2);
            int rs = parse_reg(r1), rt = parse_reg(r2);
            u16 addr;
            if (!find_label(&addr, trim(label), st)) {
                printf("Unknown label: %s\n", label);
                fclose(fp);
                st->include_depth--;
                return -1;
            }
            fprintf(out, "1F%1hX%1hX\n2DFF\n", rs, rt);
            fprintf(out, "8C%02hX\n8F%02hX\n", lo(addr), hi(addr));
            fprintf(out, instr->id == 23 ? "CFCD\n" : "DFCD\n");
        }
        else if (instr->id == 25) { /* JLT */
            sscanf(args, "%255[^,], %15[^,], %15s", label, r1, r2);
            int rs = parse_reg(r1), rt = parse_reg(r2);
            u16 addr;
            if (!find_label(&addr, trim(label), st)) {
                printf("Unknown label: %s\n", label);
                fclose(fp);
                st->include_depth--;
                return -1;
            }
            fprintf(out, "1F%1hX%1hX\n8C01\n7DDC\n", rs, rt);
            fprintf(out, "8C%02hX\n8F%02hX\nDFCD\n", lo(addr), hi(addr));
        }
        else if (instr->id == 26) { /* JGT */
            sscanf(args, "%255[^,], %15[^,], %15s", label, r1, r2);
            int rs = parse_reg(r1), rt = parse_reg(r2);
            u16 addr;
            if (!find_label(&addr, trim(label), st)) {
                printf("Unknown label: %s\n", label);
                fclose(fp);
                st->include_depth--;
                return -1;
            }
            fprintf(out, "1F%1hX%1hX\n", rs, rt);
            fprintf(out, "8C%02hX\n8F%02hX\nCFCD\n", lo(addr), hi(addr));
        }
        else if (instr->id == 27 || instr->id == 28) { /* LOADD / STORED */
            sscanf(args, "%15[^,], %255s", r1, label);
            int r = parse_reg(r1);
            u16 addr;
            if (!find_label(&addr, trim(label), st)) {
                printf("Unknown label: %s\n", label);
                fclose(fp);
                st->include_depth--;
                return -1;
            }
            fprintf(out, "8C%02hX\n", lo(addr));
            fprintf(out, "8F%02hX\n", hi(addr));
            fprintf(out, "%1hX%1hXFC\n", instr->id == 27 ? 9 : 10, r);
        }
    }

    fclose(fp);
    st->include_depth--;
    return 0;
}

/* ---------- Entry Point ---------- */

int as_assemble(const char* in_path, FILE* out) {
    AsmState st = {0};

    if (pass1_file(in_path, &st) < 0)
        return -1;

    /* Any trailing labels at EOF belong to the code section */
    bind_pending_labels(&st, 0);

    /* Relocate all data labels past code + the 6-byte JMP */
    if (st.data_addr > 0) {
        u16 data_base = st.code_addr + 6;
        for (int i = 0; i < st.label_count; i++) {
            if (st.labels[i].is_data)
                st.labels[i].addr += data_base;
        }
    }

    if (pass2_file(in_path, out, &st) < 0) {
        free(st.data_buf.data);
        return -1;
    }

    /* Emit single JMP + padded data pool */
    if (st.data_buf.size > 0) {
        if (st.data_buf.size % 2 != 0)
            buf_push(&st.data_buf, 0x00);

        u16 end_addr = st.code_addr + 6 + (u16)st.data_buf.size;
        fprintf(out, "8C%02hX\n", lo(end_addr));
        fprintf(out, "8F%02hX\n", hi(end_addr));
        fprintf(out, "BFC0\n");

        for (size_t i = 0; i < st.data_buf.size; i += 2)
            fprintf(out, "%02X%02X\n", st.data_buf.data[i + 1], st.data_buf.data[i]);
    }

    free(st.data_buf.data);
    return 0;
}
