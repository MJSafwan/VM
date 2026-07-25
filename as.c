#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <unistd.h>
#include <assert.h>
#include "vm.h"

#define TOK_END 0
#define TOK_ID 1
#define TOK_NUM 2
#define TOK_SLIT 3
#define TOK_COM 4
#define TOK_NL 5
#define TOK_COL 6
#define TOK_EQ 7
#define TOK_MATH 8
#define TOK_OP 9
#define TOK_CP 10

#define TZ_SEEK 0
#define TZ_ACC 1

#define ACC_NUM 0
#define ACC_ID 1
#define ACC_STR 2


typedef struct {
    char *buff;
    size_t cursor;
    int state;
    int accs;
    int cmt;
} tokenizer;

typedef struct {
    int kind;
    union {
        char *name;
        char *slit;
        int num;
        char math;
    };
} token;

typedef struct {
    char *ptr;
    size_t offset;
    size_t len;
} string_view;

typedef struct {
    int l, r;
} pair;

pair math_op[128] = {
    ['+'] = (pair){1, 2},
    ['-'] = (pair){1, 2},
    ['*'] = (pair){3, 4},
    ['/'] = (pair){3, 4},
};

uint16_t pc = 0;

typedef struct {
    char *key;
    uint16_t val;    
} entry;

typedef struct {
    entry *items;
    size_t count;
    size_t capacity;
} bucket;

typedef struct {
    bucket *items;
    size_t capacity;
} hashlist;

hashlist make_list(size_t buckets) {
    hashlist map = {0};
    map.items = malloc(buckets * sizeof(bucket));
    memset(map.items, 0, buckets * sizeof(bucket));
    map.capacity = buckets;
    return map;
}

size_t hash(hashlist *map, char *key) {
    size_t sum = 0;
    for (size_t i = 0; key[i] != 0; ++i) {
         sum += key[i]; 
    }
    return sum % map->capacity;
}

int map_lookup(bucket *b, char *key, uint16_t *out) {
    uint16_t out_tmp;
    if (out == NULL)
        out = &out_tmp;
    for (size_t i = 0; i < b->count; ++i) {
        if (strcmp(key, b->items[i].key) == 0) {
            *out = b->items[i].val;
            return i;
        }
    }
    return -1;
}

void map_append(bucket *b, char *key, uint16_t val) {
    int idx = map_lookup(b, key, NULL);
    if (idx >= 0) {
        b->items[idx].val = val;
        return;
    }

    if (b->count >= b->capacity) {
        b->capacity = b->capacity == 0 ? 16 : b->capacity * 2;
        b->items = realloc(b->items, b->capacity * sizeof(entry));
    }
    b->items[b->count++] = (entry){key, val};
}

int map_get(hashlist *map, char *key, uint16_t *out) {
    size_t h = hash(map, key);
    return map_lookup(&map->items[h], key, out);
}

void map_set(hashlist *map, char *key, uint16_t val) {
    size_t h = hash(map, key);
    map_append(&map->items[h], key, val);
}

char *sstrdup(string_view sv) {
    char *out = malloc(sv.len+1);
    strncpy(out, sv.ptr + sv.offset, sv.len);
    out[sv.len] = 0;
    return out;
}

int is_sp(char cc) {
    return cc == '_' || cc == '.';
}

int is_math(char cc) {
    return cc == '+' || cc == '-' || cc == '*' || cc == '/';
}

int next_token(tokenizer *tz, token *out) {
    token token_tmp;
    if (out == NULL)
        out = &token_tmp;
    tz->state = TZ_SEEK;
    string_view sv = {0};
    sv.ptr = tz->buff;

    for (;;) {
        char cc = tz->buff[tz->cursor];
        if (tz->state == TZ_SEEK) {
            if (cc == ';') {
                tz->cmt = 1;
                tz->cursor++;
                continue;
            }
            if (cc == '\n') {
                tz->cursor++;
                tz->cmt = 0;
                return TOK_NL;
            }
            if (tz->cmt) {
                tz->cursor++;
                continue;
            }
            if (cc == 0)
                return TOK_END;
            if (isspace(cc) == 0) {
                if (cc == ',') {tz->cursor++; return TOK_COM;}
                if (cc == ':') {tz->cursor++; return TOK_COL;}
                if (cc == '=') {tz->cursor++; return TOK_EQ;}
                if (cc == '(') {tz->cursor++; return TOK_OP;}
                if (cc == ')') {tz->cursor++; return TOK_CP;}
                if (is_math(cc)) {
                    tz->cursor++;
                    out->kind = TOK_MATH;
                    out->math = cc;
                    return TOK_MATH;
                }
                tz->state = TZ_ACC;
                if (cc == '"') {
                    tz->accs = ACC_STR;
                    tz->cursor++;
                    sv.offset = tz->cursor;
                    continue;
                }
                if (is_sp(cc) || isalnum(cc)) {
                    if (isdigit(cc))
                        tz->accs = ACC_NUM; 
                    else tz->accs = ACC_ID;
                    sv.offset = tz->cursor;
                    continue;
                }
            }
        } 

        if (tz->state == TZ_ACC) {
            if (tz->accs == ACC_ID) {
                if (isalnum(cc) == 0 && is_sp(cc) == 0) {
                    out->kind = TOK_ID;
                    out->name = sstrdup(sv);
                    return TOK_ID;
                }
            }

            if (tz->accs == ACC_NUM) {
                if (isdigit(cc) == 0) {
                    out->kind = TOK_NUM;
                    char *tmp = sstrdup(sv);
                    out->num = atoi(tmp);
                    free(tmp);
                    return TOK_NUM;
                }
            }

            if (tz->accs == ACC_STR) {
                if (cc == '"') {
                    out->kind = TOK_SLIT;
                    out->slit = sstrdup(sv);
                    tz->cursor++;
                    return TOK_SLIT;
                }
            }
            sv.len++;
        }
        tz->cursor++;
    }
}

int expect(tokenizer tz, int kind) {
    return next_token(&tz, NULL) == kind;
}

void skip(tokenizer *tz) {
    next_token(tz, NULL);
}

token peek(tokenizer tz) {
    token tmp = {0};
    tmp.kind = next_token(&tz, &tmp);
    return tmp;
}

int parse_nllist(tokenizer *tz) {
    if (!expect(*tz, TOK_NL))
        return 0;
    skip(tz);
    for (;;) {
        if (expect(*tz, TOK_NL))
            skip(tz);
        else break;
    }
    return 1;
}

int parse_linebreak(tokenizer *tz) {
    if (!expect(*tz, TOK_NL))
        return 1;
    return parse_nllist(tz);
}

int expect_arg(tokenizer tz) {
    return expect(tz, TOK_NUM) || expect(tz, TOK_ID);
}

typedef struct {
    char *key;
    char val;
} op_kv;

#define PROG_LEN 1028
#define ROM_LEN (PROG_LEN*4)

hashlist labels = {0};
hashlist regs = {0};
char rom[ROM_LEN] = {0};

#define ARG_BYTE 0
#define ARG_REL 1

typedef struct {
    union {
        char *label;
        char val;
    };
    int kind;
} op_arg;

typedef struct {
    uint16_t loc;
    char opcode;
    char set;
    op_arg argv[3];
} op_entry;

#define DARG_NUM 0
#define DARG_SLIT 1
typedef struct {
    int kind;
    union {
        int num;
        char *slit;
    };
} dir_arg;

typedef struct {
    op_entry items[PROG_LEN];
} op_list;

size_t opc = 0;

op_list prog = {0};

hashlist opnames = {0};


void emit_op(char *opname) {
    uint16_t op = 0;
    if (map_get(&opnames, opname, &op) != -1) {
        prog.items[opc].opcode = (char)op;
        prog.items[opc].loc = pc;
        prog.items[opc].set = 1;
        return;
    }
    __builtin_debugtrap();
}

void emit_num(char num, int idx) {
    prog.items[opc].argv[idx].val = num;
    prog.items[opc].argv[idx].kind = ARG_BYTE;
}

void emit_id(char *name, int idx) {
    if (strcmp(name, ".") == 0) {
        emit_num(pc<<8, idx);
        emit_num(pc&0xff, idx+1);
        return;
    }
    uint16_t regval = 0;
    if (map_get(&regs, name, &regval) != -1) {
        prog.items[opc].argv[idx].val = (char)regval;
        prog.items[opc].argv[idx].kind = ARG_BYTE;
        return;
    }

    prog.items[opc].argv[idx].label = name;
    prog.items[opc].argv[idx].kind = ARG_REL;
}

int parse_arg(tokenizer *tz, int idx) {
    token t = peek(*tz);

    if (t.kind == TOK_ID) {
        skip(tz);
        emit_id(t.name, idx);
        return 1;
    } else if (t.kind == TOK_NUM) {
        skip(tz);
        emit_num(t.num, idx);
        return 1;
    } else {
        return 0;
    }
}

int parse_argv(tokenizer *tz) {
    if (parse_arg(tz, 0) == 0)
        return 1;

    int argc = 1;
    for (;;) {
        if (argc >= 3)
            break;
        if (!expect(*tz, TOK_COM))
            break;
        skip(tz);
        if (parse_arg(tz, argc) == 0)
            break;
        argc++;
    }
    return 1;
}

dir_arg *parse_dir_arg(tokenizer *tz) {
    token t = peek(*tz);
    dir_arg *a = malloc(sizeof(dir_arg));
    if (t.kind == TOK_NUM) {
        skip(tz);
        a->num = t.num;
        a->kind = DARG_NUM;
        return a;
    } else if (t.kind == TOK_SLIT) {
        skip(tz);
        a->slit = t.slit;
        a->kind = DARG_SLIT;
        return a;
    } else {
        return NULL;
    }
}

#define MAX_DARG 256
dir_arg **parse_dir_argv(tokenizer *tz) {
    dir_arg **argv = malloc(MAX_DARG * sizeof(dir_arg*));
    int argc = 0;
    dir_arg *a = parse_dir_arg(tz);
    if (a == NULL)
        return NULL;
    argv[argc++] = a;
    for (;;) {
        if (argc >= MAX_DARG-1)
            break;
        if (!expect(*tz, TOK_COM))
            break;
        skip(tz);
        dir_arg *a = parse_dir_arg(tz);
        if (a == NULL)
            break;
        argv[argc++] = a;
    }
    argv[argc] = NULL;
    return argv;
}

int parse_dir(tokenizer *tz, char *dname) {
    dir_arg **argv = parse_dir_argv(tz);
    if (argv == NULL)
        goto FAIL;
    if (strcmp(dname, ".ORG") == 0) {
        if (argv[0] == NULL)
            goto FAIL;
        int target = argv[0]->num;
        pc = target;
        goto SUCCESS;
    } 

    if (strcmp(dname, ".DATA") == 0) {
        for (int i = 0; argv[i] != NULL; ++i) {
            dir_arg *a = argv[i];
            if (a->kind == DARG_SLIT) {
                strcpy(rom+pc, a->slit);
                pc += strlen(a->slit);
            } else if (a->kind == DARG_NUM) {
                (rom+pc)[0] = a->num;
                pc++;
            }
        }
        goto SUCCESS;
    }
SUCCESS:
    free(argv);
    return 1;
FAIL:
    free(argv);
    return 0;
}

uint16_t eval_math(uint16_t num1, uint16_t num2, char op) {
    switch (op) {
        case '+': return num1 + num2;
        case '-': return num1 - num2;
        case '*': return num1 * num2;
        case '/': if (num2 != 0) return num1 / num2; else return 0;
        default: return 0;
    }
}

int parse_math(tokenizer *tz, size_t min, uint16_t *ret);

int parse_pmath(tokenizer *tz, uint16_t *ret) {
    token t = peek(*tz);
    if (t.kind == TOK_NUM) {
        skip(tz);
        *ret = t.num;
        return 1;
    } else if (t.kind == TOK_OP) {
        skip(tz);
        if (!parse_math(tz, 0, ret))
            return 0;
        if (!expect(*tz, TOK_CP))
            return 0;
        skip(tz);
        return 1;
    } else if (t.kind == TOK_ID) {
        skip(tz);
        if (strcmp(t.name, ".") == 0) {
            *ret = pc;
            return 1;
        }
        if (map_get(&labels, t.name, ret) == -1)
            return 0;
        return 1;
    }
    return 0;
}

int parse_math(tokenizer *tz, size_t min, uint16_t *ret) {
    uint16_t num1 = 0;
    if (!parse_pmath(tz, &num1))
        return 0;

    for (;;) {
        token t = peek(*tz);
        if (t.kind != TOK_MATH)
            break;
        if (math_op[t.math].l < min)
            break;
        uint16_t num2 = 0;
        skip(tz);
        if (!parse_math(tz,math_op[t.math].r, &num2))
            return 0;
        num1 = eval_math(num1, num2, t.math);
    }
    *ret = num1;
    return 1;
}

int parse_instruction(tokenizer *tz) {
    pc += pc % ALIGN == 0 ? 0 : ALIGN - (pc % ALIGN);
    char *opname;
    if (!expect(*tz, TOK_ID))
        return 0;
    token t = {0};
    next_token(tz, &t);
    opname = t.name;
    token tn = peek(*tz);
    if (tn.kind == TOK_COL) {
        skip(tz); 
        map_set(&labels, opname, pc);
        return parse_instruction(tz);
    } else if (tn.kind == TOK_EQ) {
        skip(tz);
        uint16_t val = 0;
        if (!parse_math(tz, 0, &val))
            return 0;
        map_set(&labels, opname, val);
        return 1;
    }

    if (opname[0] == '.') {
        return parse_dir(tz, opname);
    }

    emit_op(opname);
    int res = parse_argv(tz); 
    pc += ALIGN;
    opc++;
    return res;
}

int parse_program(tokenizer *tz) {
    parse_linebreak(tz);
    for (;;) {
        if (expect(*tz, TOK_END))
            break;
        parse_instruction(tz);
        if (!parse_nllist(tz))
            break;
    }
    return 1;
}

void link_program() {
    for (size_t i = 0; i < PROG_LEN; ++i) {
       op_entry entry = prog.items[i]; 
       op co = {0};
       if (entry.set == 0)
           continue;
       co.opcode = entry.opcode;
       int argc = 1;
       for (size_t j = 0; j < 3; ++j) {
           op_arg arg = entry.argv[j];
           if (arg.kind == ARG_BYTE) {
               ((char *)(&co))[argc] = arg.val;
               argc++;
           } else {
                uint16_t val = 0;
                if (map_get(&labels, arg.label, &val) == -1)
                    __builtin_debugtrap();
                ((char *)(&co))[argc] = val>>8;
                ((char *)(&co))[argc+1] = val&0xff;
                argc += 2;
           }
           if (argc > 3)
               break;
       }
       memcpy(rom+entry.loc, &co, sizeof(co));
    }
}

void init_tables(void) {
    labels = make_list(512);
    opnames = make_list(256);
#define X(a, b) map_set(&opnames, #a, b);
    OPCODES;
#undef X
    regs = make_list(LAST_REG);
#define X(a, b) map_set(&regs, #a, b);
    REGS;
#undef X
}

#define BUFF_CAP (512)

int main(void) {
    init_tables();
    char *buff = malloc(BUFF_CAP+1);
    int total = 0;
    int br = 0;
    int cap = BUFF_CAP;
    while ((br = read(0, buff+total, BUFF_CAP)) > 0) {
        total += br;
        if (total >= cap) {
            cap *= 2;
            buff = realloc(buff, cap+1); 
        }
    }
    buff[total] = 0;

    tokenizer tz = {0};
    tz.buff = buff;
    parse_program(&tz);
    link_program();
    write(STDOUT_FILENO, rom, ROM_LEN);
    return 1;
}
