#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <time.h>

#include "vm.h"

#define RAM_SIZE (4096)
#define NS 1e6

#define VEC_RESET 0
#define VEC_INV_INST 4
#define VEC_ARITH_EXC 8

char flags = -1;
char *vm_ram = NULL;
uint32_t vm_reg[LAST_REG+1] = {0};


char vm_mmio(uint16_t addr, uint32_t val, char op) {
    switch (addr) {
        case 0:
            /* Serial output */
            putchar((char)val);
            break;
        case 1:
            fflush(stdout);
            break;
        default:
            /* Not implemented */
            break;
    }
    return 0;
}

char vm_io(uint16_t addr, uint32_t val , char op) {
    switch (addr) {
        case 0 ... RAM_SIZE-1:
            /* ram */
            if (op == WRITE)
                return vm_ram[addr] = (val);
            else return (vm_ram[addr]);
        default:
            /* IO */
            return vm_mmio(addr-RAM_SIZE, val, op);
    }
}

static int (*trap_cb)() = NULL;

extern void trace_trap();

void vm_run(void) {
    static_assert(sizeof(op) == sizeof(uint32_t), "wrong size");
    op cop = {0};
    for (;;) {
        if (vm_reg[REG_PC] >= RAM_SIZE)
            __builtin_debugtrap();
        memcpy(&cop, &vm_ram[vm_reg[REG_PC]], sizeof(op)); 
        
        vm_reg[REG_PC] += ALIGN;

        switch (cop.opcode) {
            case NOP: break;
            case CALL: 
                vm_reg[REG_LR] = vm_reg[REG_PC];
                vm_reg[REG_PC] = (cop.A1 << 8) + cop.A2;
                break;
            case RET:
                vm_reg[REG_PC] = vm_reg[REG_LR];
                break;
            case PUSH:
                vm_io(vm_reg[REG_SP]--, vm_reg[cop.A1]&0xff, WRITE);
                vm_io(vm_reg[REG_SP]--, vm_reg[cop.A1]>>8, WRITE);
                break;
            case POP:
                vm_reg[cop.A1] = vm_io(++vm_reg[REG_SP], 0, READ)<<8;
                vm_reg[cop.A1] |= vm_io(++vm_reg[REG_SP], 0, READ);
                break;
            case MOVI: vm_reg[cop.A1] = (cop.A2 << 8)+cop.A3; break;
            case MOVR: vm_reg[cop.A1] = vm_reg[cop.A2]; break;
            case JMP: vm_reg[REG_PC] = (cop.A1<<8)+cop.A2; break;
            case HLT: return;
            case CMP: flags = (vm_reg[REG_A] - vm_reg[REG_X]); break;
            case JZ: if (flags == 0) vm_reg[REG_PC] = (cop.A1<<8) + cop.A2; break;
            case TRAP:
                    trace_trap();
                    break;
            case WRITE: 
                 vm_io((cop.A2 << 8) + cop.A3, vm_reg[cop.A1], cop.opcode);
                 break; 
            case READ:
                 vm_reg[cop.A1] &= 0xffffff00;
                 vm_reg[cop.A1] |= vm_io((cop.A2 << 8) + cop.A3, 0, cop.opcode);
                 break; 
            case WRITER: 
                 vm_io(vm_reg[cop.A1], vm_reg[cop.A2], cop.opcode);
                 break; 
            case READR:
                 vm_reg[cop.A1] &= 0xffffff00;
                 vm_reg[cop.A1] |= vm_io(vm_reg[cop.A2], 0, cop.opcode);
                 break; 
            case LSL:
                 vm_reg[cop.A1] <<= cop.A2;
                 break;
            case LSR:
                 vm_reg[cop.A1] >>= cop.A2;
                 break;
            case LOR:
                 vm_reg[cop.A1] |= cop.A2;
                 break;
            case LAND:
                 vm_reg[cop.A1] &= cop.A2;
                 break;
            case LXOR:
                 vm_reg[cop.A1] ^= cop.A2;
                 break;
            case ADD: vm_reg[cop.A1] = vm_reg[cop.A2] + vm_reg[cop.A3]; break;
            case SUB: vm_reg[cop.A1] = vm_reg[cop.A2] - vm_reg[cop.A3]; break;
            case MUL: vm_reg[cop.A1] = vm_reg[cop.A2] * vm_reg[cop.A3]; break;
            case DIV:
                      if (vm_reg[cop.A3] == 0) {
                          vm_reg[REG_PC] = VEC_ARITH_EXC;
                          break;
                      }
                      vm_reg[cop.A1] = vm_reg[cop.A2] / vm_reg[cop.A3];
                      break;
            case INC: vm_reg[cop.A1]++; break;
            case JNZ: if (flags != 0) vm_reg[REG_PC] = (cop.A1 << 8) + cop.A2; break;
            default:
                    vm_reg[REG_PC] = VEC_INV_INST;
                    break;
        }


        if (vm_reg[REG_PC] >= RAM_SIZE) {
            __builtin_debugtrap();
        }
    }
}

char *read_file(char *fn, int *len) {
    int fd = fn != NULL ? open(fn, O_RDONLY) : STDIN_FILENO;
    if (fd < 0)
        return NULL;
    char *data = malloc(RAM_SIZE);
    if ((*len = read(fd, data, RAM_SIZE)) < 0) {
        free(data);
        return NULL;
    }
    return data;
}

int main(int argc, char **argv) {
    char *pn = argv[0];
    argv++;
    vm_ram = malloc(RAM_SIZE);
    int len = 0;
    char *data = read_file(argv[0], &len);
    if (data == NULL) {
        fprintf(stderr, "vm: %s: %s\n", argv[0], strerror(errno));
        return 1;
    }
    memcpy(vm_ram, data, len);
    free(data);
    vm_run();

    return 0;
}
