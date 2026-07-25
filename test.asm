.ORG 0
VECTOR_TABLE:
    JMP _ENTRY     ; addr 0, reset
    JMP PANIC      ; addr 4, invalid operation
    JMP PANIC      ; addr 8, arithmatic exception
    JMP PANIC      ; addr 12, reserved

SERIAL_OUT = 4096
SERIAL_FLUSH = SERIAL_OUT+1

.ORG 16 ; Code starts at address 16
_ENTRY:
    MOVI SP, 1, 0
    MOVI X, SERIAL_OUT
    MOVI A, MSG
    CALL PRINT_SERIAL
    HLT

PRINT_SERIAL:
    PUSH LR
PRINT_SERIAL.LOOP:
    READR X, A
    WRITE X, SERIAL_OUT
    INC A
    PUSH A
    MOVI A, 0
    CMP X, A
    POP A
    JZ PRINT_SERIAL.END
    JMP PRINT_SERIAL.LOOP
PRINT_SERIAL.END:
    WRITE X, SERIAL_FLUSH
    POP LR
    RET

PANIC:
    MOVI A, PANIC_MSG
    CALL PRINT_SERIAL
    HLT

MSG:
    .DATA "Hello, World!", 10, 0

PANIC_MSG: 
    .DATA "Panic!", 0

