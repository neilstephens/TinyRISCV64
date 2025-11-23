# RISC-V RV64IM Self-Test Program (STP)
# Designed for raw bytecode loading or flat binary execution.
#
# REGISTERS USED:
# x1-x31 are used for testing.
# x2 (sp) is assumed to be initialized to a valid stack area.

# ==============================================================================
# MACROS
# ==============================================================================
# Simple PUSH macro for 64-bit systems
.macro PUSH reg
    addi sp, sp, -8
    sd \reg, 0(sp)
.endm

.section .text
.globl _start
_start:

    # ==========================================================================
    # 1. BASIC INTEGER ARITHMETIC (ADD, SUB, ADDI)
    # ==========================================================================

    # Setup initial values
    li x2, 0x10   # Assume valid stack, but let's use x2 as a value source for now if needed. 
                  # WAIT: x2 is SP. We must NOT overwrite x2 unless we are modifying stack.
    li x3, 10
    li x4, 5

    # TEST: Basic ADD
    # CONTEXT: x3=10, x4=5
    # EXPECTED PUSH: 0x000000000000000F
    add x5, x3, x4
    PUSH x5

    # TEST: Basic SUB
    # CONTEXT: x3=10, x4=5
    # EXPECTED PUSH: 0x0000000000000005
    sub x5, x3, x4
    PUSH x5

    # TEST: ADDI with negative immediate (Sign Extension Check)
    # CONTEXT: x0=0, imm=-1 (0xFFF in 12-bit)
    # EXPECTED PUSH: 0xFFFFFFFFFFFFFFFF
    addi x5, x0, -1
    PUSH x5

    # TEST: Overflow behavior (Ignored in RISC-V, but result must wrap)
    # CONTEXT: x6 = Max Int (0x7FFFFFFFFFFFFFFF)
    # EXPECTED PUSH: 0x8000000000000000
    li x6, 0x7FFFFFFFFFFFFFFF
    addi x6, x6, 1
    PUSH x6

    # ==========================================================================
    # 2. LOGICAL OPERATIONS (AND, OR, XOR)
    # ==========================================================================

    # TEST: AND Operation
    # CONTEXT: x3=0xF0, x4=0xCC (11001100) -> Result 0xC0 (11000000)
    # EXPECTED PUSH: 0x00000000000000C0
    li x3, 0xF0
    li x4, 0xCC
    and x5, x3, x4
    PUSH x5

    # TEST: OR Operation
    # CONTEXT: x3=0xF0, x4=0xCC -> Result 0xFC (11111100)
    # EXPECTED PUSH: 0x00000000000000FC
    or x5, x3, x4
    PUSH x5

    # TEST: XOR Operation
    # CONTEXT: x3=0xF0, x4=0xCC -> Result 0x3C (00111100)
    # EXPECTED PUSH: 0x000000000000003C
    xor x5, x3, x4
    PUSH x5

    # ==========================================================================
    # 3. SHIFT OPERATIONS (SLL, SRL, SRA) & EDGE CASES
    # ==========================================================================

    # TEST: SLLI (Shift Left Logical Immediate)
    # CONTEXT: x3=1, shift=4
    # EXPECTED PUSH: 0x0000000000000010
    li x3, 1
    slli x5, x3, 4
    PUSH x5

    # TEST: SRAI (Shift Right Arithmetic Immediate) - Sign Extension Preservation
    # CONTEXT: x3=-16 (0xFF...F0)
    # EXPECTED PUSH: 0xFFFFFFFFFFFFFFFF
    li x3, -16
    srai x5, x3, 4
    PUSH x5

    # TEST: SRLI (Shift Right Logical Immediate) - Zero Extension
    # CONTEXT: x3=-1 (0xFF...FF)
    # EXPECTED PUSH: 0x0FFFFFFFFFFFFFFF
    li x3, -1
    srli x5, x3, 4
    PUSH x5

    # TEST: Shift Amount Truncation (Edge Case)
    # CONTEXT: x3=1, x4=65. RV64 uses only lower 6 bits (65 & 0x3F = 1).
    # EXPECTED PUSH: 0x0000000000000002
    li x3, 1
    li x4, 65
    sll x5, x3, x4
    PUSH x5

    # ==========================================================================
    # 4. COMPARISONS (SLT, SLTU)
    # ==========================================================================

    # TEST: SLT Signed Comparison (Negative < Positive)
    # CONTEXT: x3=-1, x4=1. -1 < 1 is True.
    # EXPECTED PUSH: 0x0000000000000001
    li x3, -1
    li x4, 1
    slt x5, x3, x4
    PUSH x5

    # TEST: SLTU Unsigned Comparison (Large Unsigned > Small Unsigned)
    # CONTEXT: x3=-1 (Max Uint), x4=1. Max > 1. Result False (0).
    # EXPECTED PUSH: 0x0000000000000000
    sltu x5, x3, x4
    PUSH x5

    # ==========================================================================
    # 5. CONTROL FLOW (BRANCHES, JUMPS)
    # ==========================================================================

    # TEST: BEQ Taken
    # CONTEXT: x3=10, x4=10. Branch skips instruction loading 0 to x5.
    # EXPECTED PUSH: 0x0000000000000001
    li x3, 10
    li x4, 10
    li x5, 0
    beq x3, x4, 1f
    li x5, 0          # Should be skipped
    j 2f
1:  li x5, 1          # Target
2:
    PUSH x5

    # TEST: BNE Not Taken
    # CONTEXT: x3=10, x4=10. Branch fallthrough.
    # EXPECTED PUSH: 0x0000000000000002
    li x5, 2
    bne x3, x4, 3f    # Should not branch
    j 4f
3:  li x5, 0          # Fail case
4:
    PUSH x5

    # TEST: JAL (Jump and Link)
    # CONTEXT: Tests if return address is stored in x1 (ra)
    # EXPECTED PUSH: 0x0000000000000001
    li x5, 0
    jal x1, 5f
    li x5, 0          # Skipped
    j 6f
5:  li x5, 1
6:
    PUSH x5

    # ==========================================================================
    # 6. MEMORY OPERATIONS (LOAD/STORE)
    # ==========================================================================
    # We will use the stack area itself for scratch memory to avoid .data sections
    # x2 is SP. We will use offset 16(sp) and 24(sp) for scratch.

    # TEST: Store Word (SW) and Load Word (LW)
    # CONTEXT: Store 0xDEADBEEF at 16(sp), read it back.
    # EXPECTED PUSH: 0xFFFFFFFFDEADBEEF
    li x3, 0xDEADBEEF
    sw x3, 16(sp)
    lw x5, 16(sp)     # LW sign extends 32-bit to 64-bit! 0xDEADBEEF -> Negative
    PUSH x5

    # TEST: Load Word Unsigned (LWU)
    # CONTEXT: Read 0xDEADBEEF from 16(sp), zero extended.
    # EXPECTED PUSH: 0x00000000DEADBEEF
    lwu x5, 16(sp)
    PUSH x5

    # TEST: Load Byte (LB) Sign Extension
    # CONTEXT: Store 0xFF at 24(sp). Load back.
    # EXPECTED PUSH: 0xFFFFFFFFFFFFFFFF
    li x3, -1
    sb x3, 24(sp)
    lb x5, 24(sp)
    PUSH x5

    # TEST: Load Byte Unsigned (LBU)
    # CONTEXT: Store 0xFF at 24(sp). Load back zero extended.
    # EXPECTED PUSH: 0x00000000000000FF
    lbu x5, 24(sp)
    PUSH x5

    # ==========================================================================
    # 7. RV64M EXTENSION (MULTIPLY / DIVIDE)
    # ==========================================================================

    # TEST: MUL (Lower 64 bits)
    # CONTEXT: 10 * -10 = -100
    # EXPECTED PUSH: 0xFFFFFFFFFFFFFF9C
    li x3, 10
    li x4, -10
    mul x5, x3, x4
    PUSH x5

    # TEST: MULH (Signed x Signed High Part)
    # CONTEXT: MaxInt * 2. 0x7FFF... * 2 = 0xFF...FE (Low) and 0 (High).
    # Let's try: -1 * -1 = 1. High part 0.
    # Let's try: Large Negative * Large Positive to get interesting bits.
    # Use -1 * 1 = -1. 128-bit result is 0xFF...FFFF...FF. High part -1.
    # EXPECTED PUSH: 0xFFFFFFFFFFFFFFFF
    li x3, -1
    li x4, 1
    mulh x5, x3, x4
    PUSH x5

    # TEST: MULHU (Unsigned x Unsigned High Part)
    # CONTEXT: -1 (MaxUint) * 1. Result (2^64 - 1). High part 0.
    # EXPECTED PUSH: 0x0000000000000000
    li x3, -1
    li x4, 1
    mulhu x5, x3, x4
    PUSH x5

    # TEST: MULHSU (Signed x Unsigned High Part)
    # CONTEXT: rs1=-1 (Signed), rs2=1 (Unsigned).
    # -1 * 1 = -1. 128-bit: 0xFF...FF. High part: -1.
    # EXPECTED PUSH: 0xFFFFFFFFFFFFFFFF
    li x3, -1
    li x4, 1
    mulhsu x5, x3, x4
    PUSH x5

    # TEST: DIV (Signed Division)
    # CONTEXT: -100 / 10 = -10
    # EXPECTED PUSH: 0xFFFFFFFFFFFFFFF6
    li x3, -100
    li x4, 10
    div x5, x3, x4
    PUSH x5

    # TEST: DIVU (Unsigned Division)
    # CONTEXT: -100 (Large Positive) / 10.
    # 0xFFFFFFFFFFFFFF9C / 10 = Large result.
    # 18446744073709551516 / 10 = 1844674407370955151
    # Hex: 0x1999999999999999
    # EXPECTED PUSH: 0x1999999999999999
    li x3, -100
    li x4, 10
    divu x5, x3, x4
    PUSH x5

    # TEST: DIV By Zero Edge Case
    # CONTEXT: x / 0. Spec says return -1.
    # EXPECTED PUSH: 0xFFFFFFFFFFFFFFFF
    li x3, 12345
    li x4, 0
    div x5, x3, x4
    PUSH x5

    # TEST: REM By Zero Edge Case
    # CONTEXT: x % 0. Spec says return Dividend (x).
    # EXPECTED PUSH: 0x0000000000003039
    li x3, 12345     # 0x3039
    li x4, 0
    rem x5, x3, x4
    PUSH x5

    # TEST: Overflow Division Edge Case
    # CONTEXT: Most Negative (-2^63) / -1. Should Overflow and return Dividend.
    # EXPECTED PUSH: 0x8000000000000000
    li x3, 0x8000000000000000
    li x4, -1
    div x5, x3, x4
    PUSH x5

    # ==========================================================================
    # END OF TEST
    # ==========================================================================
    # Use EBREAK to signal VM to stop
    ebreak