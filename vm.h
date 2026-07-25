#ifndef VM_H_
#define VM_H_

#define ALIGN (4)

#define OPCODES \
X(NOP, 0x0) \
X(MOVI, 0x1) \
X(JMP, 0x2) \
X(CMP, 0x3) \
X(JZ, 0x4) \
X(ADD, 0x5) \
X(SUB, 0x6) \
X(MUL, 0x7) \
X(DIV, 0x8) \
X(WRITE, 0x9) \
X(HLT, 0xa) \
X(INC, 0xb) \
X(JNZ, 0xc) \
X(READ, 0xd) \
X(MOVR, 0xe) \
X(CALL, 0xf) \
X(RET, 0x10) \
X(PUSH, 0x11) \
X(POP, 0x12) \
X(READR, 0x13) \
X(WRITER, 0x14) \
X(LSL, 0x15) \
X(LSR, 0x16) \
X(LOR, 0x17) \
X(LAND, 0x18) \
X(LXOR, 0x19)  

#define X(a, b) a,
enum {
OPCODES
};
#undef X

#define REGS \
X(A, 0x0) \
X(X, 0x1) \
X(SP, 0x2) \
X(PC, 0x3) \
X(LR, 0x4) \
X(FP, 0x5)

#define X(a, b) REG_##a,
enum {
    REGS
};
#undef X

#define LAST_REG REG_FP

typedef struct {
    unsigned char opcode;
    unsigned char A1;
    unsigned char A2;
    unsigned char A3;
} op;

#endif
