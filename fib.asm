; THIS WAS AI GENERATED.

.ORG 0
VECTOR_TABLE:
    JMP _ENTRY     ; addr 0: System Boot / Reset
    JMP PANIC      ; addr 4: Invalid Instruction Exception
    JMP PANIC      ; addr 8: Arithmetic Exception (e.g., Div by Zero)
    JMP PANIC      ; addr 12: Reserved

; --- Hardware Peripherals ---
SERIAL_OUT = 4096
SERIAL_FLUSH = SERIAL_OUT+1

; --- Scratchpad RAM Variables (Byte-Addressable) ---
CONST_10    = 500  ; Holds the number 10 for base-10 division
NUM_VAL     = 501  ; Temporary storage for the number being printed
DIGIT_COUNT = 502  ; Tracks how many digits are pushed to the stack
TEMP_Q      = 503  ; Holds quotient (N / 10)
TEMP_P      = 504  ; Holds product (Q * 10)

FIB_N       = 510  ; Loop counter (how many numbers left to generate)
FIB_A       = 511  ; f(n-2)
FIB_B       = 512  ; f(n-1)
FIB_C       = 513  ; f(n)

.ORG 16 ; Main firmware execution block
_ENTRY:
    MOVI SP, 15, 254
    TRAP
    
    ; Cache our base-10 divisor in RAM
    MOVI A, 0, 10
    WRITE A, CONST_10
    
    ; Initialize Fibonacci state: f(0)=0, f(1)=1
    MOVI A, 0, 0          
    WRITE A, FIB_A
    MOVI A, 0, 1          
    WRITE A, FIB_B
    
    ; Set up loop to generate the first 10 Fibonacci numbers
    MOVI A, 0, 10
    WRITE A, FIB_N

FIB_LOOP:
    ; Check if we have processed all 10 numbers (FIB_N == 0)
    READ A, FIB_N
    MOVI X, 0, 0
    CMP A, X              ; flags = A - X
    JZ FIB_END            ; Exit loop if counter hits zero
    
    ; --- Step 1: Print current Fibonacci Number ---
    READ A, FIB_A         ; Load current number into Accumulator
    CALL PRINT_NUM        ; Execute string conversion and print
    
    ; Print a space delimiter (ASCII 32) between numbers
    MOVI X, 0, 32
    WRITE X, SERIAL_OUT
    
    ; --- Step 2: Calculate Next Fibonacci Number ---
    READ A, FIB_A
    READ X, FIB_B
    ADD A, A, X           ; A = f(n-2) + f(n-1)
    WRITE A, FIB_C        ; Store next term
    
    ; --- Step 3: Shift Variables Down the Sequence ---
    READ A, FIB_B
    WRITE A, FIB_A        ; f(n-2) = f(n-1)
    
    READ A, FIB_C
    WRITE A, FIB_B        ; f(n-1) = f(n)
    
    ; --- Step 4: Decrement Loop Counter ---
    READ A, FIB_N
    MOVI X, 0, 1
    SUB A, A, X           ; Counter--
    WRITE A, FIB_N
    
    JMP FIB_LOOP          ; Repeat sequence

FIB_END:
    MOVI X, 0, 10         ; Print trailing Newline (ASCII 10)
    WRITE X, SERIAL_OUT
    WRITE X, SERIAL_FLUSH ; Flush the serial buffer to output text
    HLT                   ; Halt Virtual Machine safely


; =========================================================================
; SUBROUTINE: PRINT_NUM
; Converts an integer in Register A into characters and prints them.
; Uses a stack-reversal trick to print digits left-to-right.
; =========================================================================
PRINT_NUM:
    PUSH LR               ; Save return link register
    WRITE A, NUM_VAL      ; Store value to process
    
    MOVI A, 0, 0          ; Clear digit counter
    WRITE A, DIGIT_COUNT
    
    ; Special Case check: Is the number exactly 0?
    READ A, NUM_VAL
    MOVI X, 0, 0
    CMP A, X
    JNZ PRINT_NUM.DECOMPOSE
    
    MOVI X, 0, 48         ; If 0, print ASCII '0' directly
    WRITE X, SERIAL_OUT
    JMP PRINT_NUM.DONE

PRINT_NUM.DECOMPOSE:
    ; Loop while NUM_VAL > 0
    READ A, NUM_VAL
    MOVI X, 0, 0
    CMP A, X
    JZ PRINT_NUM.DISPLAY  ; No digits left, transition to printing stage
    
    ; Q = NUM_VAL / 10
    READ A, NUM_VAL
    READ X, CONST_10
    DIV A, A, X           
    WRITE A, TEMP_Q
    
    ; P = Q * 10
    READ X, CONST_10
    MUL A, A, X           
    WRITE A, TEMP_P
    
    ; Remainder (Digit) = NUM_VAL - P
    READ A, NUM_VAL
    READ X, TEMP_P
    SUB A, A, X           
    
    ; Convert raw digit to ASCII (Digit + 48)
    MOVI X, 0, 48
    ADD A, A, X           
    PUSH A                ; Push ASCII character onto the stack
    
    ; Track how many digits are stored on stack
    READ A, DIGIT_COUNT
    INC A
    WRITE A, DIGIT_COUNT
    
    ; Set NUM_VAL = Q for the next round
    READ A, TEMP_Q
    WRITE A, NUM_VAL
    JMP PRINT_NUM.DECOMPOSE

PRINT_NUM.DISPLAY:
    ; Pull characters off the stack in reverse order
    READ A, DIGIT_COUNT
    MOVI X, 0, 0
    CMP A, X
    JZ PRINT_NUM.DONE     ; Out of digits, finish routine
    
    POP X                 ; Pop the highest order ASCII character
    WRITE X, SERIAL_OUT   ; Send to terminal
    
    ; Decrement digit tracker
    READ A, DIGIT_COUNT
    MOVI X, 0, 1
    SUB A, A, X
    WRITE A, DIGIT_COUNT
    JMP PRINT_NUM.DISPLAY

PRINT_NUM.DONE:
    POP LR                ; Restore original return path
    RET

PANIC_MSG: 
    .DATA "P"

PANIC:
    READ A, PANIC_MSG
    WRITE A, SERIAL_OUT
    HLT                   ; Hardware exception safety trap
