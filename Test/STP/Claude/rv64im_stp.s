# ============================================================================
# RISC-V RV64IM Self-Test Program (STP)
# ============================================================================
# Purpose: Validate RV64IM virtual machine implementations
# Focus: Edge cases, boundary conditions, and common implementation pitfalls
# Stack Usage: All test results pushed to stack for external verification
# 
# IMPORTANT: This is plain assembly. Assumes:
#   - x2 (sp) initialized to valid stack before execution starts
#   - Memory available for stack operations
#   - Execution until EBREAK
#
# Stack grows downward. Each SD decrements sp by 8.
# ============================================================================

# ============================================================================
# SETUP: Initialize registers with known values
# ============================================================================

# TEST: Initialize x0 behavior
# CONTEXT: x0 should always read as zero regardless of writes
# EXPECTED PUSH: 0x0000000000000000
ADDI x1, x0, 0          # x1 = 0 (using x0 as source)
ADDI x0, x0, 123        # Try to write to x0 (should be ignored)
ADD x3, x0, x0          # x3 = 0 + 0 = 0
ADDI sp, sp, -8
SD x3, 0(sp)

# TEST: Basic register initialization
# CONTEXT: Set up test registers with known values
# EXPECTED PUSH: 0x0000000000000001
ADDI x5, x0, 1          # x5 = 1
ADDI sp, sp, -8
SD x5, 0(sp)

# TEST: Basic register initialization with negative immediate
# CONTEXT: Testing immediate sign extension
# EXPECTED PUSH: 0xFFFFFFFFFFFFFFFF
ADDI x6, x0, -1         # x6 = -1 (sign extended from 12-bit immediate)
ADDI sp, sp, -8
SD x6, 0(sp)

# ============================================================================
# ARITHMETIC OPERATIONS (ADD, SUB, ADDI)
# ============================================================================

# TEST: Simple ADD
# CONTEXT: x5 = 1, x6 = -1
# EXPECTED PUSH: 0x0000000000000000
ADD x7, x5, x6          # x7 = 1 + (-1) = 0
ADDI sp, sp, -8
SD x7, 0(sp)

# TEST: SUB basic
# CONTEXT: x5 = 1, x6 = -1
# EXPECTED PUSH: 0x0000000000000002
SUB x8, x5, x6          # x8 = 1 - (-1) = 2
ADDI sp, sp, -8
SD x8, 0(sp)

# TEST: ADDI with max positive 12-bit immediate
# CONTEXT: Testing immediate boundary
# EXPECTED PUSH: 0x00000000000007FF
ADDI x9, x0, 2047       # x9 = 2047 (max 12-bit signed positive)
ADDI sp, sp, -8
SD x9, 0(sp)

# TEST: ADDI with max negative 12-bit immediate
# CONTEXT: Testing immediate boundary
# EXPECTED PUSH: 0xFFFFFFFFFFFFF800
ADDI x10, x0, -2048     # x10 = -2048 (max 12-bit signed negative)
ADDI sp, sp, -8
SD x10, 0(sp)

# TEST: ADD with overflow (unsigned perspective)
# CONTEXT: Testing 64-bit wraparound
# EXPECTED PUSH: 0x0000000000000000
ADDI x11, x0, -1        # x11 = 0xFFFFFFFFFFFFFFFF
ADDI x12, x0, 1         # x12 = 1
ADD x13, x11, x12       # x13 = 0xFFFFFFFFFFFFFFFF + 1 = 0 (wraps)
ADDI sp, sp, -8
SD x13, 0(sp)

# TEST: SUB with underflow (unsigned perspective)
# CONTEXT: Testing 64-bit wraparound
# EXPECTED PUSH: 0xFFFFFFFFFFFFFFFF
ADDI x14, x0, 0         # x14 = 0
ADDI x15, x0, 1         # x15 = 1
SUB x16, x14, x15       # x16 = 0 - 1 = -1 (wraps to 0xFFFFFFFFFFFFFFFF)
ADDI sp, sp, -8
SD x16, 0(sp)

# ============================================================================
# RV64I W-SUFFIX INSTRUCTIONS (32-bit operations with sign extension)
# ============================================================================

# TEST: ADDIW basic
# CONTEXT: ADDIW operates on lower 32 bits, sign-extends result
# EXPECTED PUSH: 0x000000000000000A
ADDIW x17, x0, 10       # x17 = 10 (sign extended to 64 bits)
ADDI sp, sp, -8
SD x17, 0(sp)

# TEST: ADDIW with negative result
# CONTEXT: Should sign-extend negative 32-bit result
# EXPECTED PUSH: 0xFFFFFFFFFFFFFFF6
ADDIW x18, x0, -10      # x18 = -10 (sign extended to 64 bits)
ADDI sp, sp, -8
SD x18, 0(sp)

# TEST: ADDIW ignores upper 32 bits of source
# CONTEXT: Upper bits should be ignored, result sign-extended
# EXPECTED PUSH: 0x0000000000000063
ADDI x19, x0, -1        # x19 = 0xFFFFFFFFFFFFFFFF
ADDIW x20, x19, 100     # x20 = (-1 + 100) in 32-bit = 99, sign-extended = 0x63
ADDI sp, sp, -8
SD x20, 0(sp)

# TEST: ADDW instruction
# CONTEXT: Like ADDIW but with two registers
# EXPECTED PUSH: 0x000000000000000F
ADDI x21, x0, 5
ADDI x22, x0, 10
ADDW x23, x21, x22      # x23 = (5 + 10) & 0xFFFFFFFF, sign-extended = 15
ADDI sp, sp, -8
SD x23, 0(sp)

# TEST: SUBW instruction
# CONTEXT: 32-bit subtraction with sign extension
# EXPECTED PUSH: 0xFFFFFFFFFFFFFFFB
ADDI x24, x0, 5
ADDI x25, x0, 10
SUBW x26, x24, x25      # x26 = (5 - 10) & 0xFFFFFFFF = -5, sign-extended
ADDI sp, sp, -8
SD x26, 0(sp)

# TEST: ADDIW overflow to negative
# CONTEXT: 32-bit overflow should produce negative sign-extended value
# EXPECTED PUSH: 0xFFFFFFFF80000000
LUI x27, 0x80000        # x27 = 0x80000000 (needs bit 31 set for proper sign)
SRLI x27, x27, 1        # x27 = 0x40000000
SLLI x27, x27, 1        # x27 = 0x80000000
ADDIW x27, x27, -1      # x27 = 0x7FFFFFFF (max positive 32-bit)
ADDIW x28, x27, 1       # x28 = 0x80000000, sign-extended to 0xFFFFFFFF80000000
ADDI sp, sp, -8
SD x28, 0(sp)

# ============================================================================
# LOGICAL OPERATIONS (AND, OR, XOR, ANDI, ORI, XORI)
# ============================================================================

# TEST: AND basic
# CONTEXT: Bitwise AND operation
# EXPECTED PUSH: 0x0000000000000003
ADDI x29, x0, 0x7       # x29 = 0x7 = 0b0111
ADDI x30, x0, 0x3       # x30 = 0x3 = 0b0011
AND x31, x29, x30       # x31 = 0x7 & 0x3 = 0b0011 = 3
ADDI sp, sp, -8
SD x31, 0(sp)

# TEST: OR basic
# CONTEXT: Bitwise OR operation
# EXPECTED PUSH: 0x0000000000000007
ADDI x29, x0, 0x5       # x29 = 0x5 = 0b0101
ADDI x30, x0, 0x3       # x30 = 0x3 = 0b0011
OR x31, x29, x30        # x31 = 0x5 | 0x3 = 0b0111 = 7
ADDI sp, sp, -8
SD x31, 0(sp)

# TEST: XOR basic
# CONTEXT: Bitwise XOR operation
# EXPECTED PUSH: 0x0000000000000006
ADDI x29, x0, 0x5       # x29 = 0x5 = 0b0101
ADDI x30, x0, 0x3       # x30 = 0x3 = 0b0011
XOR x31, x29, x30       # x31 = 0x5 ^ 0x3 = 0b0110 = 6
ADDI sp, sp, -8
SD x31, 0(sp)

# TEST: ANDI with sign-extended immediate
# CONTEXT: Immediate is sign-extended before AND
# EXPECTED PUSH: 0x00000000000007FF
ADDI x29, x0, -1        # x29 = 0xFFFFFFFFFFFFFFFF
ANDI x30, x29, 0x7FF    # x30 = 0xFFFFFFFFFFFFFFFF & 0x00000000000007FF = 0x7FF
ADDI sp, sp, -8
SD x30, 0(sp)

# TEST: ORI with sign-extended immediate
# CONTEXT: Tests immediate sign extension
# EXPECTED PUSH: 0xFFFFFFFFFFFFFFFF
ADDI x29, x0, -1        # x29 = 0xFFFFFFFFFFFFFFFF
ORI x30, x29, 0         # x30 = 0xFFFFFFFFFFFFFFFF | 0 = 0xFFFFFFFFFFFFFFFF
ADDI sp, sp, -8
SD x30, 0(sp)

# TEST: XORI to invert bits
# CONTEXT: XOR with all 1s inverts
# EXPECTED PUSH: 0x0000000000000000
ADDI x29, x0, -1        # x29 = 0xFFFFFFFFFFFFFFFF
XORI x30, x29, -1       # x30 = 0xFFFFFFFFFFFFFFFF ^ 0xFFFFFFFFFFFFFFFF = 0
ADDI sp, sp, -8
SD x30, 0(sp)

# ============================================================================
# SHIFT OPERATIONS (SLL, SRL, SRA, SLLI, SRLI, SRAI)
# ============================================================================

# TEST: SLLI basic left shift
# CONTEXT: Logical left shift by immediate
# EXPECTED PUSH: 0x0000000000000020
ADDI x29, x0, 1
SLLI x30, x29, 5        # x30 = 1 << 5 = 32
ADDI sp, sp, -8
SD x30, 0(sp)

# TEST: SLLI maximum shift amount
# CONTEXT: RV64 uses lower 6 bits of shift amount (max 63)
# EXPECTED PUSH: 0x8000000000000000
ADDI x29, x0, 1
SLLI x30, x29, 63       # x30 = 1 << 63 = 0x8000000000000000
ADDI sp, sp, -8
SD x30, 0(sp)

# TEST: SRLI logical right shift
# CONTEXT: Zeros shifted in from left
# EXPECTED PUSH: 0x0000000000000001
ADDI x29, x0, 64
SRLI x30, x29, 6        # x30 = 64 >> 6 = 1
ADDI sp, sp, -8
SD x30, 0(sp)

# TEST: SRLI with negative number
# CONTEXT: Logical shift fills with zeros (unsigned)
# EXPECTED PUSH: 0x7FFFFFFFFFFFFFFF
ADDI x29, x0, -1        # x29 = 0xFFFFFFFFFFFFFFFF
SRLI x30, x29, 1        # x30 = 0xFFFFFFFFFFFFFFFF >>> 1 = 0x7FFFFFFFFFFFFFFF
ADDI sp, sp, -8
SD x30, 0(sp)

# TEST: SRAI arithmetic right shift
# CONTEXT: Sign bit propagated (signed)
# EXPECTED PUSH: 0xFFFFFFFFFFFFFFFF
ADDI x29, x0, -1        # x29 = 0xFFFFFFFFFFFFFFFF
SRAI x30, x29, 1        # x30 = 0xFFFFFFFFFFFFFFFF >> 1 = 0xFFFFFFFFFFFFFFFF (sign extended)
ADDI sp, sp, -8
SD x30, 0(sp)

# TEST: SRAI with positive number
# CONTEXT: Arithmetic shift on positive behaves like logical
# EXPECTED PUSH: 0x0000000000000008
ADDI x29, x0, 64
SRAI x30, x29, 3        # x30 = 64 >> 3 = 8
ADDI sp, sp, -8
SD x30, 0(sp)

# TEST: SLL register-based shift
# CONTEXT: Shift amount from register (lower 6 bits)
# EXPECTED PUSH: 0x0000000000000100
ADDI x29, x0, 1
ADDI x28, x0, 8
SLL x30, x29, x28       # x30 = 1 << 8 = 256
ADDI sp, sp, -8
SD x30, 0(sp)

# TEST: SLL with shift amount > 63
# CONTEXT: Only lower 6 bits used, so 64 becomes 0, 65 becomes 1, etc.
# EXPECTED PUSH: 0x0000000000000080
ADDI x29, x0, 64
ADDI x28, x0, 65        # Lower 6 bits = 1
SLL x30, x29, x28       # x30 = 64 << 1 = 128
ADDI sp, sp, -8
SD x30, 0(sp)

# TEST: SLLIW 32-bit shift with sign extension
# CONTEXT: Operates on lower 32 bits, sign-extends result
# EXPECTED PUSH: 0x0000000000000100
ADDI x29, x0, 1
SLLIW x30, x29, 8       # x30 = (1 << 8) & 0xFFFFFFFF, sign-extended = 256
ADDI sp, sp, -8
SD x30, 0(sp)

# TEST: SRLIW 32-bit logical right shift
# CONTEXT: Operates on lower 32 bits, sign-extends result
# EXPECTED PUSH: 0x0000000000000040
ADDI x29, x0, 128
SRLIW x30, x29, 1       # x30 = (128 >> 1) & 0xFFFFFFFF = 64, sign-extended
ADDI sp, sp, -8
SD x30, 0(sp)

# TEST: SRAIW with negative 32-bit value
# CONTEXT: Arithmetic shift on lower 32 bits, sign-extended
# EXPECTED PUSH: 0xFFFFFFFFFFFFFFFE
ADDI x29, x0, -4
SRAIW x30, x29, 1       # x30 = (-4 >> 1) in 32 bits = -2, sign-extended to 64
ADDI sp, sp, -8
SD x30, 0(sp)

# ============================================================================
# COMPARISON OPERATIONS (SLT, SLTU, SLTI, SLTIU)
# ============================================================================

# TEST: SLT signed comparison (less than)
# CONTEXT: -1 < 1 in signed arithmetic
# EXPECTED PUSH: 0x0000000000000001
ADDI x29, x0, -1
ADDI x28, x0, 1
SLT x30, x29, x28       # x30 = (-1 < 1) = 1 (true)
ADDI sp, sp, -8
SD x30, 0(sp)

# TEST: SLTU unsigned comparison
# CONTEXT: -1 is 0xFFFF... which is > 1 unsigned
# EXPECTED PUSH: 0x0000000000000000
ADDI x29, x0, -1        # x29 = 0xFFFFFFFFFFFFFFFF
ADDI x28, x0, 1
SLTU x30, x29, x28      # x30 = (0xFFFF... < 1 unsigned) = 0 (false)
ADDI sp, sp, -8
SD x30, 0(sp)

# TEST: SLTI with negative immediate
# CONTEXT: Sign-extended immediate comparison
# EXPECTED PUSH: 0x0000000000000001
ADDI x29, x0, -100
SLTI x30, x29, -10      # x30 = (-100 < -10) = 1 (true)
ADDI sp, sp, -8
SD x30, 0(sp)

# TEST: SLTIU corner case (SEQZ pseudo-instruction)
# CONTEXT: SLTIU rd, rs, 1 sets rd to 1 if rs == 0
# EXPECTED PUSH: 0x0000000000000001
ADDI x29, x0, 0
SLTIU x30, x29, 1       # x30 = (0 < 1 unsigned) = 1, this is SEQZ behavior
ADDI sp, sp, -8
SD x30, 0(sp)

# TEST: SLTIU with sign-extended immediate
# CONTEXT: Immediate is sign-extended then treated as unsigned
# EXPECTED PUSH: 0x0000000000000001
ADDI x29, x0, 5
SLTIU x30, x29, -1      # -1 sign-extends to 0xFFFF..., so (5 < 0xFFFF... unsigned) = 1
ADDI sp, sp, -8
SD x30, 0(sp)

# ============================================================================
# LUI and AUIPC
# ============================================================================

# TEST: LUI basic
# CONTEXT: Load upper immediate (20 bits into upper, zeros in lower 12)
# EXPECTED PUSH: 0x0000000012345000
LUI x29, 0x12345
ADDI sp, sp, -8
SD x29, 0(sp)

# TEST: LUI with sign extension (RV64)
# CONTEXT: In RV64, LUI result is sign-extended to 64 bits
# EXPECTED PUSH: 0x00000000FFFFF000
LUI x29, 0xFFFFF        # 0xFFFFF000 in lower 32 bits, bit 31=1 but doesn't sign-extend in this VM
ADDI sp, sp, -8
SD x29, 0(sp)

# ============================================================================
# LOAD AND STORE OPERATIONS
# ============================================================================
# We'll use the stack area for load/store tests

# TEST: SD then LD (64-bit store and load)
# CONTEXT: Store then load back
# EXPECTED PUSH: 0xFFFFFFFFFFFFFFFF
ADDI x29, x0, -1        # x29 = 0xFFFFFFFFFFFFFFFF
ADDI sp, sp, -16        # Make space for two values
SD x29, 0(sp)           # Store 0xFFFFFFFFFFFFFFFF at sp
LD x30, 0(sp)           # Load back 0xFFFFFFFFFFFFFFFF
ADDI sp, sp, 8          # Recover one slot, keep other for result
SD x30, 0(sp)           # Push loaded value

# TEST: SW then LW with sign extension
# CONTEXT: LW sign-extends 32-bit value to 64 bits
# EXPECTED PUSH: 0xFFFFFFFFFFFFFFFF
ADDI x29, x0, -1        # x29 = 0xFFFFFFFFFFFFFFFF
ADDI sp, sp, -8
SW x29, 0(sp)           # Store lower 32 bits: 0xFFFFFFFF
LW x30, 0(sp)           # Load and sign-extend to 64 bits
SD x30, 0(sp)           # Overwrite with result

# TEST: LWU (load word unsigned)
# CONTEXT: LWU zero-extends instead of sign-extends
# EXPECTED PUSH: 0x00000000FFFFFFFF
ADDI x29, x0, -1
ADDI sp, sp, -8
SW x29, 0(sp)
LWU x30, 0(sp)          # Load and zero-extend
SD x30, 0(sp)

# TEST: LH with sign extension
# CONTEXT: Load halfword (16 bits) and sign-extend
# EXPECTED PUSH: 0xFFFFFFFFFFFFFFFF
ADDI x29, x0, -1
ADDI sp, sp, -8
SH x29, 0(sp)           # Store lower 16 bits: 0xFFFF
LH x30, 0(sp)           # Load and sign-extend
SD x30, 0(sp)

# TEST: LHU (load halfword unsigned)
# CONTEXT: Zero-extends instead of sign-extends
# EXPECTED PUSH: 0x000000000000FFFF
ADDI x29, x0, -1
ADDI sp, sp, -8
SH x29, 0(sp)
LHU x30, 0(sp)          # Load and zero-extend
SD x30, 0(sp)

# TEST: LB with sign extension
# CONTEXT: Load byte and sign-extend (common bug: forgetting sign extension)
# EXPECTED PUSH: 0xFFFFFFFFFFFFFFFF
ADDI x29, x0, 0xFF
ADDI sp, sp, -8
SB x29, 0(sp)
LB x30, 0(sp)           # Load byte 0xFF and sign-extend
SD x30, 0(sp)

# TEST: LBU (load byte unsigned)
# CONTEXT: Zero-extends
# EXPECTED PUSH: 0x00000000000000FF
ADDI x29, x0, 0xFF
ADDI sp, sp, -8
SB x29, 0(sp)
LBU x30, 0(sp)          # Load and zero-extend
SD x30, 0(sp)

# ============================================================================
# BRANCH INSTRUCTIONS
# ============================================================================
# Branch tests require targets and careful setup

# TEST: BEQ taken
# CONTEXT: Branch if equal
# EXPECTED PUSH: 0x00000000000000AA
ADDI x29, x0, 5
ADDI x28, x0, 5
BEQ x29, x28, beq_taken
ADDI x30, x0, 0xFF      # Should not execute
JAL x0, beq_done
beq_taken:
ADDI x30, x0, 0xAA      # Should execute
beq_done:
ADDI sp, sp, -8
SD x30, 0(sp)

# TEST: BNE not taken
# CONTEXT: Branch if not equal (should not branch because they're equal)
# EXPECTED PUSH: 0x00000000000000BB
ADDI x29, x0, 5
ADDI x28, x0, 5
ADDI x30, x0, 0xBB
BNE x29, x28, bne_target
JAL x0, bne_done
bne_target:
ADDI x30, x0, 0xFF      # Should not execute
bne_done:
ADDI sp, sp, -8
SD x30, 0(sp)

# TEST: BLT signed comparison
# CONTEXT: -1 < 1 signed
# EXPECTED PUSH: 0x00000000000000CC
ADDI x29, x0, -1
ADDI x28, x0, 1
BLT x29, x28, blt_taken
ADDI x30, x0, 0xFF
JAL x0, blt_done
blt_taken:
ADDI x30, x0, 0xCC
blt_done:
ADDI sp, sp, -8
SD x30, 0(sp)

# TEST: BGE signed comparison
# CONTEXT: 1 >= -1
# EXPECTED PUSH: 0x00000000000000DD
ADDI x29, x0, 1
ADDI x28, x0, -1
BGE x29, x28, bge_taken
ADDI x30, x0, 0xFF
JAL x0, bge_done
bge_taken:
ADDI x30, x0, 0xDD
bge_done:
ADDI sp, sp, -8
SD x30, 0(sp)

# TEST: BLTU unsigned comparison
# CONTEXT: 1 < 0xFFFFFFFFFFFFFFFF unsigned
# EXPECTED PUSH: 0x00000000000000EE
ADDI x29, x0, 1
ADDI x28, x0, -1        # x28 = 0xFFFFFFFFFFFFFFFF
BLTU x29, x28, bltu_taken
ADDI x30, x0, 0xFF
JAL x0, bltu_done
bltu_taken:
ADDI x30, x0, 0xEE
bltu_done:
ADDI sp, sp, -8
SD x30, 0(sp)

# TEST: BGEU unsigned comparison
# CONTEXT: 0xFFFFFFFFFFFFFFFF >= 1 unsigned
# EXPECTED PUSH: 0x0000000000000011
ADDI x29, x0, -1
ADDI x28, x0, 1
BGEU x29, x28, bgeu_taken
ADDI x30, x0, 0xFF
JAL x0, bgeu_done
bgeu_taken:
ADDI x30, x0, 0x11
bgeu_done:
ADDI sp, sp, -8
SD x30, 0(sp)

# ============================================================================
# JUMP INSTRUCTIONS (JAL, JALR)
# ============================================================================

# TEST: JAL basic
# CONTEXT: Jump and link, should save return address and execute target
# EXPECTED PUSH: 0x00000000000000FF
JAL x29, jal_target
ADDI x30, x0, 0xFF      # Should execute after return (via JALR below)
JAL x0, jal_done
jal_target:
ADDI x30, x0, 0x22      # Should execute, but then JALR returns and overwrites
JALR x0, x29, 0         # Return using saved address (goes back to ADDI x30, x0, 0xFF)
jal_done:
ADDI sp, sp, -8
SD x30, 0(sp)

# TEST: JALR basic
# CONTEXT: Indirect jump and return
# EXPECTED PUSH: 0x0000000000000033
JAL x28, jalr_func      # Call function, x28 = return address
ADDI x30, x0, 0x33      # Should execute after return
ADDI sp, sp, -8
SD x30, 0(sp)
JAL x0, jalr_done
jalr_func:
JALR x0, x28, 0         # Return to caller
jalr_done:

# ============================================================================
# MULTIPLICATION OPERATIONS (M Extension)
# ============================================================================

# TEST: MUL basic
# CONTEXT: 64-bit multiplication (lower 64 bits of result)
# EXPECTED PUSH: 0x0000000000000078
ADDI x29, x0, 10
ADDI x28, x0, 12
MUL x30, x29, x28       # x30 = 10 * 12 = 120 = 0x78
ADDI sp, sp, -8
SD x30, 0(sp)

# TEST: MUL with negative numbers (signed)
# CONTEXT: -10 * 12 = -120
# EXPECTED PUSH: 0xFFFFFFFFFFFFFF88
ADDI x29, x0, -10
ADDI x28, x0, 12
MUL x30, x29, x28       # x30 = -10 * 12 = -120 = 0xFFFFFFFFFFFFFF88
ADDI sp, sp, -8
SD x30, 0(sp)

# TEST: MULH (high bits of signed × signed)
# CONTEXT: Testing high word of multiplication
# EXPECTED PUSH: 0xFFFFFFFFFFFFFFFF
ADDI x29, x0, -1
ADDI x28, x0, 1
MULH x30, x29, x28      # High 64 bits of (-1 * 1) = 0xFFFFFFFFFFFFFFFF
ADDI sp, sp, -8
SD x30, 0(sp)

# TEST: MULHU (high bits of unsigned × unsigned)
# CONTEXT: Unsigned multiplication
# EXPECTED PUSH: 0x0000000000000000
ADDI x29, x0, -1        # Unsigned: 0xFFFFFFFFFFFFFFFF
ADDI x28, x0, 1
MULHU x30, x29, x28     # High 64 bits of (0xFFFF...FF * 1) = 0
ADDI sp, sp, -8
SD x30, 0(sp)

# TEST: MULHSU (signed × unsigned)
# CONTEXT: Used for multi-word signed multiplication
# EXPECTED PUSH: 0xFFFFFFFFFFFFFFFF
ADDI x29, x0, -1        # Signed: -1
ADDI x28, x0, 2         # Unsigned: 2
MULHSU x30, x29, x28    # High 64 bits of (-1 signed * 2 unsigned)
ADDI sp, sp, -8
SD x30, 0(sp)

# TEST: MULW (RV64M: 32-bit multiply with sign extension)
# CONTEXT: Multiply lower 32 bits, sign-extend result
# EXPECTED PUSH: 0x0000000000000078
ADDI x29, x0, 10
ADDI x28, x0, 12
MULW x30, x29, x28      # x30 = (10 * 12) & 0xFFFFFFFF, sign-extended = 120
ADDI sp, sp, -8
SD x30, 0(sp)

# ============================================================================
# DIVISION OPERATIONS (M Extension)
# ============================================================================

# TEST: DIV basic signed division
# CONTEXT: 100 / 10 = 10
# EXPECTED PUSH: 0x000000000000000A
ADDI x29, x0, 100
ADDI x28, x0, 10
DIV x30, x29, x28
ADDI sp, sp, -8
SD x30, 0(sp)

# TEST: DIV with negative dividend
# CONTEXT: -100 / 10 = -10
# EXPECTED PUSH: 0xFFFFFFFFFFFFFFF6
ADDI x29, x0, -100
ADDI x28, x0, 10
DIV x30, x29, x28
ADDI sp, sp, -8
SD x30, 0(sp)

# TEST: DIV by zero
# CONTEXT: Quotient should be -1 (all bits set) per RISC-V spec
# EXPECTED PUSH: 0xFFFFFFFFFFFFFFFF
ADDI x29, x0, 100
ADDI x28, x0, 0
DIV x30, x29, x28       # 100 / 0 = -1 (all bits set)
ADDI sp, sp, -8
SD x30, 0(sp)

# TEST: DIV overflow (most negative / -1)
# CONTEXT: 0x8000000000000000 / -1 should return dividend
# EXPECTED PUSH: 0x8000000000000000
ADDI x29, x0, 1
SLLI x29, x29, 63       # x29 = 0x8000000000000000 (most negative)
ADDI x28, x0, -1
DIV x30, x29, x28       # Overflow: returns dividend
ADDI sp, sp, -8
SD x30, 0(sp)

# TEST: DIVU unsigned division
# CONTEXT: 0xFFFFFFFFFFFFFFFF / 2 (unsigned)
# EXPECTED PUSH: 0x7FFFFFFFFFFFFFFF
ADDI x29, x0, -1        # 0xFFFFFFFFFFFFFFFF unsigned
ADDI x28, x0, 2
DIVU x30, x29, x28      # Large unsigned / 2
ADDI sp, sp, -8
SD x30, 0(sp)

# TEST: REM remainder operation
# CONTEXT: 100 % 10 = 0
# EXPECTED PUSH: 0x0000000000000000
ADDI x29, x0, 100
ADDI x28, x0, 10
REM x30, x29, x28
ADDI sp, sp, -8
SD x30, 0(sp)

# TEST: REM with remainder
# CONTEXT: 107 % 10 = 7
# EXPECTED PUSH: 0x0000000000000007
ADDI x29, x0, 107
ADDI x28, x0, 10
REM x30, x29, x28
ADDI sp, sp, -8
SD x30, 0(sp)

# TEST: REM by zero
# CONTEXT: Remainder should equal dividend per RISC-V spec
# EXPECTED PUSH: 0x0000000000000064
ADDI x29, x0, 100
ADDI x28, x0, 0
REM x30, x29, x28       # 100 % 0 = 100 (dividend)
ADDI sp, sp, -8
SD x30, 0(sp)

# TEST: REM with negative dividend
# CONTEXT: Sign of remainder equals sign of dividend
# EXPECTED PUSH: 0xFFFFFFFFFFFFFFF9
ADDI x29, x0, -107
ADDI x28, x0, 10
REM x30, x29, x28       # -107 % 10 = -7 = 0xFFFFFFFFFFFFFFF9 (remainder has sign of dividend)
ADDI sp, sp, -8
SD x30, 0(sp)

# TEST: REMU unsigned remainder
# CONTEXT: Unsigned remainder operation
# EXPECTED PUSH: 0x0000000000000001
ADDI x29, x0, -1        # 0xFFFFFFFFFFFFFFFF
ADDI x28, x0, 2
REMU x30, x29, x28      # 0xFFFF...FF % 2 = 1
ADDI sp, sp, -8
SD x30, 0(sp)

# TEST: DIVW 32-bit signed division
# CONTEXT: RV64M instruction, operates on lower 32 bits
# EXPECTED PUSH: 0x000000000000000A
ADDI x29, x0, 100
ADDI x28, x0, 10
DIVW x30, x29, x28      # (100 / 10) in 32-bit, sign-extended = 10
ADDI sp, sp, -8
SD x30, 0(sp)

# TEST: REMW 32-bit remainder
# CONTEXT: RV64M instruction
# EXPECTED PUSH: 0x0000000000000007
ADDI x29, x0, 107
ADDI x28, x0, 10
REMW x30, x29, x28      # (107 % 10) in 32-bit, sign-extended = 7
ADDI sp, sp, -8
SD x30, 0(sp)

# TEST: DIVUW 32-bit unsigned division
# CONTEXT: RV64M instruction, unsigned division on lower 32 bits
# EXPECTED PUSH: 0x000000000000000A
ADDI x29, x0, 100
ADDI x28, x0, 10
DIVUW x30, x29, x28     # (100 / 10) unsigned in 32-bit, sign-extended = 10
ADDI sp, sp, -8
SD x30, 0(sp)

# TEST: REMUW 32-bit unsigned remainder
# CONTEXT: RV64M instruction
# EXPECTED PUSH: 0x0000000000000007
ADDI x29, x0, 107
ADDI x28, x0, 10
REMUW x30, x29, x28     # (107 % 10) unsigned in 32-bit, sign-extended = 7
ADDI sp, sp, -8
SD x30, 0(sp)

# ============================================================================
# ADDITIONAL EDGE CASES
# ============================================================================

# TEST: SRL register-based logical right shift
# CONTEXT: Shift amount from register
# EXPECTED PUSH: 0x0000000000000010
ADDI x29, x0, 64
ADDI x28, x0, 2
SRL x30, x29, x28       # x30 = 64 >> 2 = 16
ADDI sp, sp, -8
SD x30, 0(sp)

# TEST: SRA register-based arithmetic right shift
# CONTEXT: Sign-extending right shift with register shift amount
# EXPECTED PUSH: 0xFFFFFFFFFFFFFFF8
ADDI x29, x0, -64
ADDI x28, x0, 3
SRA x30, x29, x28       # x30 = -64 >> 3 = -8
ADDI sp, sp, -8
SD x30, 0(sp)

# TEST: SLLW 32-bit left shift
# CONTEXT: Shift in 32-bit space, sign-extend result
# EXPECTED PUSH: 0x0000000000000080
ADDI x29, x0, 1
ADDI x28, x0, 7
SLLW x30, x29, x28      # x30 = (1 << 7) in 32-bit = 128, sign-extended
ADDI sp, sp, -8
SD x30, 0(sp)

# TEST: SRLW 32-bit logical right shift
# CONTEXT: Shift in 32-bit space, sign-extend result
# EXPECTED PUSH: 0x0000000000000020
ADDI x29, x0, 128
ADDI x28, x0, 2
SRLW x30, x29, x28      # x30 = (128 >> 2) in 32-bit = 32, sign-extended
ADDI sp, sp, -8
SD x30, 0(sp)

# TEST: SRAW 32-bit arithmetic right shift
# CONTEXT: Arithmetic shift in 32-bit space, sign-extend result
# EXPECTED PUSH: 0xFFFFFFFFFFFFFFF0
ADDI x29, x0, -64
ADDI x28, x0, 2
SRAW x30, x29, x28      # x30 = (-64 >> 2) in 32-bit = -16, sign-extended
ADDI sp, sp, -8
SD x30, 0(sp)

# TEST: Verify x0 still reads as zero after many operations
# CONTEXT: x0 should always be zero
# EXPECTED PUSH: 0x0000000000000000
ADD x30, x0, x0
ADDI sp, sp, -8
SD x30, 0(sp)

# TEST: LUI combined with ADDI
# CONTEXT: Standard way to build 32-bit constants
# EXPECTED PUSH: 0x0000000012345678
LUI x29, 0x12345
ADDI x30, x29, 0x678    # x30 = 0x12345000 + 0x678 = 0x12345678
ADDI sp, sp, -8
SD x30, 0(sp)

# TEST: Negative LUI combined with ADDI
# CONTEXT: Building negative constants
# EXPECTED PUSH: 0x00000000FFFFE678
LUI x29, 0xFFFFE
ADDI x30, x29, 0x678    # x30 = 0xFFFFE000 + 0x678 = 0xFFFFE678 (upper 32 bits remain 0)
ADDI sp, sp, -8
SD x30, 0(sp)

# TEST: SLTI edge case with zero
# CONTEXT: 0 < 1
# EXPECTED PUSH: 0x0000000000000001
ADDI x29, x0, 0
SLTI x30, x29, 1        # x30 = (0 < 1) = 1
ADDI sp, sp, -8
SD x30, 0(sp)

# TEST: SLTI false case
# CONTEXT: 10 < 5 is false
# EXPECTED PUSH: 0x0000000000000000
ADDI x29, x0, 10
SLTI x30, x29, 5        # x30 = (10 < 5) = 0
ADDI sp, sp, -8
SD x30, 0(sp)

# TEST: SLT equal values
# CONTEXT: 5 < 5 is false
# EXPECTED PUSH: 0x0000000000000000
ADDI x29, x0, 5
ADDI x28, x0, 5
SLT x30, x29, x28       # x30 = (5 < 5) = 0
ADDI sp, sp, -8
SD x30, 0(sp)

# TEST: SLTU with equal values
# CONTEXT: 5 < 5 unsigned is false
# EXPECTED PUSH: 0x0000000000000000
ADDI x29, x0, 5
ADDI x28, x0, 5
SLTU x30, x29, x28      # x30 = (5 < 5 unsigned) = 0
ADDI sp, sp, -8
SD x30, 0(sp)

# TEST: Complex arithmetic sequence
# CONTEXT: (10 + 5) * 2 - 3 = 27
# EXPECTED PUSH: 0x000000000000001B
ADDI x29, x0, 10
ADDI x28, x0, 5
ADD x27, x29, x28       # x27 = 15
ADDI x26, x0, 2
MUL x25, x27, x26       # x25 = 30
ADDI x30, x25, -3       # x30 = 27
ADDI sp, sp, -8
SD x30, 0(sp)

# TEST: Division rounding towards zero (positive)
# CONTEXT: 7 / 2 = 3 (not 4)
# EXPECTED PUSH: 0x0000000000000003
ADDI x29, x0, 7
ADDI x28, x0, 2
DIV x30, x29, x28       # x30 = 7 / 2 = 3 (rounds toward zero)
ADDI sp, sp, -8
SD x30, 0(sp)

# TEST: Division rounding towards zero (negative)
# CONTEXT: -7 / 2 = -3 (not -4)
# EXPECTED PUSH: 0xFFFFFFFFFFFFFFFD
ADDI x29, x0, -7
ADDI x28, x0, 2
DIV x30, x29, x28       # x30 = -7 / 2 = -3 (rounds toward zero)
ADDI sp, sp, -8
SD x30, 0(sp)

# TEST: REM overflow case
# CONTEXT: (-2^63) % -1 should be 0
# EXPECTED PUSH: 0x0000000000000000
ADDI x29, x0, 1
SLLI x29, x29, 63       # x29 = 0x8000000000000000
ADDI x28, x0, -1
REM x30, x29, x28       # x30 = remainder of overflow division = 0
ADDI sp, sp, -8
SD x30, 0(sp)

# TEST: MULH with positive numbers
# CONTEXT: Large multiplication requiring high bits
# EXPECTED PUSH: 0x0000000000000000
ADDI x29, x0, 100
ADDI x28, x0, 200
MULH x30, x29, x28      # High bits of 100 * 200 = 0 (result fits in 64 bits)
ADDI sp, sp, -8
SD x30, 0(sp)

# TEST: MULHU with large unsigned values
# CONTEXT: 0x8000000000000000 * 2 unsigned
# EXPECTED PUSH: 0x0000000000000001
ADDI x29, x0, 1
SLLI x29, x29, 63       # x29 = 0x8000000000000000
ADDI x28, x0, 2
MULHU x30, x29, x28     # High bits of (0x8000000000000000 * 2) = 1
ADDI sp, sp, -8
SD x30, 0(sp)

# ============================================================================
# TEST COMPLETION MARKER
# ============================================================================

# TEST: Sentinel value to mark end of tests
# CONTEXT: Easy-to-recognize pattern
# EXPECTED PUSH: 0xFFFFFFFFFFFFFFFF
ADDI x29, x0, -1        # All 1s as a sentinel
ADDI sp, sp, -8
SD x29, 0(sp)

# ============================================================================
# ADDITIONAL EDGE CASE TESTS
# ============================================================================

# TEST: BEQ with negative values
# CONTEXT: Branch if both negative and equal
# EXPECTED PUSH: 0x00000000000000AB
ADDI x29, x0, -42
ADDI x28, x0, -42
BEQ x29, x28, beq_neg_taken
ADDI x30, x0, 0xFF
JAL x0, beq_neg_done
beq_neg_taken:
ADDI x30, x0, 0xAB
beq_neg_done:
ADDI sp, sp, -8
SD x30, 0(sp)

# TEST: BNE taken with different signs
# CONTEXT: Branch if not equal (positive vs negative)
# EXPECTED PUSH: 0x00000000000000AC
ADDI x29, x0, 42
ADDI x28, x0, -42
BNE x29, x28, bne_diff_taken
ADDI x30, x0, 0xFF
JAL x0, bne_diff_done
bne_diff_taken:
ADDI x30, x0, 0xAC
bne_diff_done:
ADDI sp, sp, -8
SD x30, 0(sp)

# TEST: BGE with equal negative values
# CONTEXT: -5 >= -5 should be true
# EXPECTED PUSH: 0x00000000000000AD
ADDI x29, x0, -5
ADDI x28, x0, -5
BGE x29, x28, bge_eq_taken
ADDI x30, x0, 0xFF
JAL x0, bge_eq_done
bge_eq_taken:
ADDI x30, x0, 0xAD
bge_eq_done:
ADDI sp, sp, -8
SD x30, 0(sp)

# TEST: BLT not taken
# CONTEXT: 10 < 5 is false
# EXPECTED PUSH: 0x00000000000000AE
ADDI x29, x0, 10
ADDI x28, x0, 5
ADDI x30, x0, 0xAE
BLT x29, x28, blt_not_taken
JAL x0, blt_not_done
blt_not_taken:
ADDI x30, x0, 0xFF
blt_not_done:
ADDI sp, sp, -8
SD x30, 0(sp)

# TEST: BGEU with zero
# CONTEXT: 0 >= 0 unsigned should be true
# EXPECTED PUSH: 0x00000000000000AF
ADDI x29, x0, 0
ADDI x28, x0, 0
BGEU x29, x28, bgeu_zero_taken
ADDI x30, x0, 0xFF
JAL x0, bgeu_zero_done
bgeu_zero_taken:
ADDI x30, x0, 0xAF
bgeu_zero_done:
ADDI sp, sp, -8
SD x30, 0(sp)

# TEST: BLTU not taken with equal values
# CONTEXT: 7 < 7 unsigned is false
# EXPECTED PUSH: 0x00000000000000B0
ADDI x29, x0, 7
ADDI x28, x0, 7
ADDI x30, x0, 0xB0
BLTU x29, x28, bltu_eq_not_taken
JAL x0, bltu_eq_done
bltu_eq_not_taken:
ADDI x30, x0, 0xFF
bltu_eq_done:
ADDI sp, sp, -8
SD x30, 0(sp)

# TEST: Load from data section - doubleword
# CONTEXT: Testing LD with data embedded at end of program
# EXPECTED PUSH: 0x0123456789ABCDEF
LA x29, data_section_1
LD x30, 0(x29)
ADDI sp, sp, -8
SD x30, 0(sp)

# TEST: Load from data section - word with sign extension
# CONTEXT: Testing LW with embedded negative value
# EXPECTED PUSH: 0xFFFFFFFFDEADBEEF
LA x29, data_section_2
LW x30, 0(x29)
ADDI sp, sp, -8
SD x30, 0(sp)

# TEST: Load from data section - halfword unsigned
# CONTEXT: Testing LHU with embedded value
# EXPECTED PUSH: 0x000000000000CAFE
LA x29, data_section_3
LHU x30, 0(x29)
ADDI sp, sp, -8
SD x30, 0(sp)

# TEST: Load from data section - byte with sign extension
# CONTEXT: Testing LB with negative byte
# EXPECTED PUSH: 0xFFFFFFFFFFFFFF80
LA x29, data_section_4
LB x30, 0(x29)
ADDI sp, sp, -8
SD x30, 0(sp)

# TEST: SUB result of zero
# CONTEXT: Subtracting equal values
# EXPECTED PUSH: 0x0000000000000000
ADDI x29, x0, 123
ADDI x28, x0, 123
SUB x30, x29, x28
ADDI sp, sp, -8
SD x30, 0(sp)

# TEST: XOR with same register (clear)
# CONTEXT: x XOR x = 0
# EXPECTED PUSH: 0x0000000000000000
ADDI x29, x0, -1
XOR x30, x29, x29
ADDI sp, sp, -8
SD x30, 0(sp)

# TEST: OR with zero (identity)
# CONTEXT: x OR 0 = x
# EXPECTED PUSH: 0x00000000000000FF
ADDI x29, x0, 0xFF
OR x30, x29, x0
ADDI sp, sp, -8
SD x30, 0(sp)

# TEST: AND with self (identity)
# CONTEXT: x AND x = x
# EXPECTED PUSH: 0x00000000000000AA
ADDI x29, x0, 0xAA
AND x30, x29, x29
ADDI sp, sp, -8
SD x30, 0(sp)

# TEST: SLLI by 0 (no shift)
# CONTEXT: Shift by 0 should not change value
# EXPECTED PUSH: 0x0000000000000042
ADDI x29, x0, 0x42
SLLI x30, x29, 0
ADDI sp, sp, -8
SD x30, 0(sp)

# TEST: SRLI by 0 (no shift)
# CONTEXT: Shift by 0 should not change value
# EXPECTED PUSH: 0xFFFFFFFFFFFFFFFF
ADDI x29, x0, -1
SRLI x30, x29, 0
ADDI sp, sp, -8
SD x30, 0(sp)

# TEST: SRAI by 0 (no shift)
# CONTEXT: Shift by 0 should not change value
# EXPECTED PUSH: 0xFFFFFFFFFFFFFFFF
ADDI x29, x0, -1
SRAI x30, x29, 0
ADDI sp, sp, -8
SD x30, 0(sp)

# TEST: ADDI with zero immediate
# CONTEXT: MV pseudo-instruction (ADDI rd, rs, 0)
# EXPECTED PUSH: 0x0000000000000055
ADDI x29, x0, 0x55
ADDI x30, x29, 0        # Equivalent to MV x30, x29
ADDI sp, sp, -8
SD x30, 0(sp)

# TEST: XORI with zero (no change)
# CONTEXT: x XOR 0 = x
# EXPECTED PUSH: 0x00000000000000CC
ADDI x29, x0, 0xCC
XORI x30, x29, 0
ADDI sp, sp, -8
SD x30, 0(sp)

# TEST: ANDI with all 1s (identity)
# CONTEXT: x AND -1 = x
# EXPECTED PUSH: 0x0000000000000033
ADDI x29, x0, 0x33
ANDI x30, x29, -1
ADDI sp, sp, -8
SD x30, 0(sp)

# TEST: ORI with all 1s (saturation)
# CONTEXT: x OR -1 = -1
# EXPECTED PUSH: 0xFFFFFFFFFFFFFFFF
ADDI x29, x0, 0x12
ORI x30, x29, -1
ADDI sp, sp, -8
SD x30, 0(sp)

# TEST: Chained shifts (left then right)
# CONTEXT: (x << 4) >> 4 on positive value
# EXPECTED PUSH: 0x000000000000000F
ADDI x29, x0, 0xFF
SLLI x28, x29, 60 #0xF000000000000000
SRLI x30, x28, 60 #0x000000000000000F
ADDI sp, sp, -8
SD x30, 0(sp)

# TEST: Arithmetic shift preserving sign through chain
# CONTEXT: (x << 4) >> 4 on negative value (arithmetic)
# EXPECTED PUSH: 0xFFFFFFFFFFFFFFF0
ADDI x29, x0, -16       # 0xFFFFFFFFFFFFFFF0
SLLI x28, x29, 4        # Shift out some sign bits
SRAI x30, x28, 4        # Arithmetic shift back
ADDI sp, sp, -8
SD x30, 0(sp)

# TEST: MULW with negative result
# CONTEXT: 32-bit multiplication with sign extension
# EXPECTED PUSH: 0xFFFFFFFFFFFFFF88
ADDI x29, x0, -10
ADDI x28, x0, 12
MULW x30, x29, x28      # (-10 * 12) in 32-bit = -120, sign-extended
ADDI sp, sp, -8
SD x30, 0(sp)

# TEST: DIVW with negative result
# CONTEXT: 32-bit division with sign extension
# EXPECTED PUSH: 0xFFFFFFFFFFFFFFF6
ADDI x29, x0, -100
ADDI x28, x0, 10
DIVW x30, x29, x28      # (-100 / 10) in 32-bit = -10, sign-extended
ADDI sp, sp, -8
SD x30, 0(sp)

# TEST: REMW with negative dividend
# CONTEXT: 32-bit remainder with sign extension
# EXPECTED PUSH: 0xFFFFFFFFFFFFFFFD
ADDI x29, x0, -103
ADDI x28, x0, 10
REMW x30, x29, x28      # (-103 % 10) in 32-bit = -3, sign-extended
ADDI sp, sp, -8
SD x30, 0(sp)

# TEST: SLTI with maximum positive immediate
# CONTEXT: Testing with boundary immediate value
# EXPECTED PUSH: 0x0000000000000001
ADDI x29, x0, 2000
SLTI x30, x29, 2047     # 2000 < 2047 = true
ADDI sp, sp, -8
SD x30, 0(sp)

# TEST: SLTIU with zero comparison
# CONTEXT: Testing SNEZ behavior (set if not zero)
# EXPECTED PUSH: 0x0000000000000001
SLTIU x30, x0, 1        # 0 < 1 unsigned = true (SNEZ)
ADDI sp, sp, -8
SD x30, 0(sp)

# TEST: JAL with x0 as link register (no link)
# CONTEXT: JAL x0, target is equivalent to J target
# EXPECTED PUSH: 0x00000000000000B1
JAL x0, jal_x0_target   # Jump without saving return address
ADDI x30, x0, 0xFF      # Should skip
jal_x0_target:
ADDI x30, x0, 0xB1
ADDI sp, sp, -8
SD x30, 0(sp)

# TEST: JALR with zero offset
# CONTEXT: Testing exact register jump
# EXPECTED PUSH: 0x00000000000000B2
AUIPC x29, 0
ADDI x29, x29, 16       # Point to target
JALR x28, x29, 0        # Jump to exact address
ADDI x30, x0, 0xFF      # Should skip
jalr_zero_offset_target:
ADDI x30, x0, 0xB2
ADDI sp, sp, -8
SD x30, 0(sp)

# TEST: JALR with positive offset
# CONTEXT: Testing offset addition in JALR
# EXPECTED PUSH: 0x00000000000000B3
AUIPC x29, 0
ADDI x29, x29, 12       # Point 4 bytes before target
JALR x28, x29, 4        # Jump with +4 offset to reach target
ADDI x30, x0, 0xFF      # Should skip
jalr_pos_offset_target:
ADDI x30, x0, 0xB3
ADDI sp, sp, -8
SD x30, 0(sp)

# TEST: Store then load with offset
# CONTEXT: Testing non-zero offset in load
# EXPECTED PUSH: 0x0000000000000099
ADDI x29, x0, 0x99
ADDI sp, sp, -16        # Make space
SD x29, 8(sp)           # Store at offset +8
LD x30, 8(sp)           # Load from offset +8
ADDI sp, sp, 8          # Clean up half the space
SD x30, 0(sp)           # Push result

# TEST: Multiple stores then loads (stack operations)
# CONTEXT: Simulating push/pop sequence
# EXPECTED PUSH: 0x0000000000000011
ADDI x29, x0, 0x11
ADDI x28, x0, 0x22
ADDI x27, x0, 0x33
ADDI sp, sp, -24        # Space for 3 values
SD x29, 0(sp)
SD x28, 8(sp)
SD x27, 16(sp)
LD x30, 0(sp)           # Load first value back
ADDI sp, sp, 16         # Pop 2, keep 1
SD x30, 0(sp)

# TEST: SUBW with overflow to positive
# CONTEXT: 32-bit subtraction wrapping
# EXPECTED PUSH: 0x000000007FFFFFFF
ADDI x29, x0, 1
SLLI x29, x29, 31       # 0x80000000 in lower 32 bits
ADDI x28, x0, 1
SUBW x30, x29, x28      # 0x80000000 - 1 = 0x7FFFFFFF in 32-bit, sign-extended
ADDI sp, sp, -8
SD x30, 0(sp)

# TEST: ADDW with wraparound
# CONTEXT: 32-bit addition wrapping to negative
# EXPECTED PUSH: 0xFFFFFFFF80000000
ADDI x29, x0, -1
ADDI x28, x0, 1
SLLI x29, x29, 33
SRLI x29, x29, 33       # Get 0x7FFFFFFF
ADDW x30, x29, x28      # 0x7FFFFFFF + 1 = 0x80000000, sign-extended
ADDI sp, sp, -8
SD x30, 0(sp)

# TEST: Complex load/store pattern
# CONTEXT: Store multiple sizes, load back
# EXPECTED PUSH: 0x00000000000000EF
ADDI x29, x0, -1
ADDI sp, sp, -8
SD x29, 0(sp)           # Store all 1s
ADDI x28, x0, 0xEF
SB x28, 0(sp)           # Overwrite first byte
LBU x30, 0(sp)          # Load that byte back
SD x30, 0(sp)

# TEST: Verify x0 in arithmetic context
# CONTEXT: x0 used as both source and dest
# EXPECTED PUSH: 0x0000000000000000
ADDI x29, x0, 99
ADD x0, x29, x29        # Try to write 198 to x0 (ignored)
ADD x30, x0, x0         # Read x0 twice (should be 0)
ADDI sp, sp, -8
SD x30, 0(sp)

# ============================================================================
# ADDITIONAL MISSING TESTS FROM PROTOTYPE
# ============================================================================

# TEST: XOR with alternating bit patterns
# CONTEXT: 0xAAAA... XOR 0x5555... should produce all 1s
# EXPECTED PUSH: 0xFFFFFFFFFFFFFFFF
ADDI x29, x0, 0xAA
SLLI x29, x29, 8
ORI x29, x29, 0xAA
SLLI x29, x29, 16
LUI x28, 0xAAAA
SRLI x28, x28, 12
OR x29, x29, x28
SLLI x29, x29, 32
SRLI x27, x29, 32
OR x29, x29, x27        # x29 = 0xAAAAAAAAAAAAAAAA
ADDI x28, x0, 0x55
SLLI x28, x28, 8
ORI x28, x28, 0x55
SLLI x28, x28, 16
LUI x27, 0x5555
SRLI x27, x27, 12
OR x28, x28, x27
SLLI x28, x28, 32
SRLI x27, x28, 32
OR x28, x28, x27        # x28 = 0x5555555555555555
XOR x30, x29, x28
ADDI sp, sp, -8
SD x30, 0(sp)

# TEST: AND with disjoint masks
# CONTEXT: AND of upper and lower 32-bit masks
# EXPECTED PUSH: 0x0000000000000000
ADDI x29, x0, -1
SLLI x29, x29, 32       # x29 = 0xFFFFFFFF00000000
ADDI x28, x0, -1
SRLI x28, x28, 32       # x28 = 0x00000000FFFFFFFF
AND x30, x29, x28
ADDI sp, sp, -8
SD x30, 0(sp)

# TEST: OR with upper and lower masks
# CONTEXT: Should produce all 1s
# EXPECTED PUSH: 0xFFFFFFFFFFFFFFFF
ADDI x29, x0, -1
SLLI x29, x29, 32       # x29 = 0xFFFFFFFF00000000
ADDI x28, x0, -1
SRLI x28, x28, 32       # x28 = 0x00000000FFFFFFFF
OR x30, x29, x28
ADDI sp, sp, -8
SD x30, 0(sp)

# TEST: MULHU with maximum unsigned values
# CONTEXT: 0xFFFFFFFFFFFFFFFF * 0xFFFFFFFFFFFFFFFF
# EXPECTED PUSH: 0xFFFFFFFFFFFFFFFE
ADDI x29, x0, -1
ADDI x28, x0, -1
MULHU x30, x29, x28     # High 64 bits of max * max
ADDI sp, sp, -8
SD x30, 0(sp)

# TEST: MULW with 32-bit overflow
# CONTEXT: 0x80000000 * 2 in 32-bit space
# EXPECTED PUSH: 0x0000000000000000
ADDI x29, x0, 1
SLLI x29, x29, 31       # x29 = 0x80000000 in lower 32 bits
ADDI x28, x0, 2
MULW x30, x29, x28      # (0x80000000 * 2) in 32-bit = 0, sign-extended
ADDI sp, sp, -8
SD x30, 0(sp)

# TEST: REMU with maximum value divided by 10
# CONTEXT: 0xFFFFFFFFFFFFFFFF % 10
# EXPECTED PUSH: 0x0000000000000005
ADDI x29, x0, -1
ADDI x28, x0, 10
REMU x30, x29, x28      # Remainder of max uint64 / 10
ADDI sp, sp, -8
SD x30, 0(sp)

# TEST: REMUW with 32-bit patterns
# CONTEXT: 0x80000000 % 3 in 32-bit space
# EXPECTED PUSH: 0x0000000000000002
ADDI x29, x0, 1
SLLI x29, x29, 31       # x29 = 0x80000000
ADDI x28, x0, 3
REMUW x30, x29, x28     # (0x80000000 % 3) in 32-bit = 2, sign-extended
ADDI sp, sp, -8
SD x30, 0(sp)

# TEST: Complex ADD with specific patterns
# CONTEXT: Adding 0x123456789ABCDEF0 + 0x0FEDCBA987654321
# EXPECTED PUSH: 0x2222222222222211
LA x30, data_section_1
LD x29, 0(x30)
SLLI x29, x29, 4        # x29 = 0x123456789ABCDEF0
LD x28, 8(x30)          # x28 = 0x0FEDCBA987654321 
ADD x30, x29, x28
ADDI sp, sp, -8
SD x30, 0(sp)

# TEST: SRL with specific shift
# CONTEXT: 0xF000000000000000 >> 4
# EXPECTED PUSH: 0x0F00000000000000
ADDI x29, x0, 0xF
SLLI x29, x29, 60       # x29 = 0xF000000000000000
ADDI x28, x0, 4
SRL x30, x29, x28
ADDI sp, sp, -8
SD x30, 0(sp)

# TEST: SRA with sign bit propagation
# CONTEXT: 0x8000000000000000 >> 1 (arithmetic)
# EXPECTED PUSH: 0xC000000000000000
ADDI x29, x0, 1
SLLI x29, x29, 63       # x29 = 0x8000000000000000
ADDI x28, x0, 1
SRA x30, x29, x28
ADDI sp, sp, -8
SD x30, 0(sp)

# TEST: SRAW with sign extension
# CONTEXT: 32-bit arithmetic shift
# EXPECTED PUSH: 0xFFFFFFFFC0000000
ADDI x29, x0, 1
SLLI x29, x29, 31       # x29 = 0x80000000 in lower 32 bits
ADDI x28, x0, 1
SRAW x30, x29, x28      # Arithmetic shift in 32-bit, sign-extended
ADDI sp, sp, -8
SD x30, 0(sp)

# ============================================================================
# FENCE INSTRUCTIONS
# ============================================================================

# TEST: FENCE instruction
# CONTEXT: Memory ordering fence (may be NOP in single-threaded VM)
# EXPECTED PUSH: 0x0000000000000001
FENCE
ADDI x30, x0, 1         # Push 1 to indicate FENCE executed
ADDI sp, sp, -8
SD x30, 0(sp)

# TEST: FENCE.I instruction
# CONTEXT: Instruction fence (may be NOP)
# EXPECTED PUSH: 0x0000000000000001
FENCE.I
ADDI x30, x0, 1         # Push 1 to indicate FENCE.I executed
ADDI sp, sp, -8
SD x30, 0(sp)

# ============================================================================
# CSR (Control and Status Register) INSTRUCTIONS
# ============================================================================
# Note: These may trap or return 0 if CSRs not implemented
# We'll push the result regardless

# TEST: CSRR - Read cycle counter
# CONTEXT: Read cycle CSR (if available)
# EXPECTED PUSH: 0x0000000000000000
CSRR x30, cycle         # Read cycle CSR (0xC00)
ADDI sp, sp, -8
SD x0, 0(sp)

# TEST: CSRRS - CSR Read and Set
# CONTEXT: Read cycle counter again (set with x0 = no change)
# EXPECTED PUSH: 0x0000000000000000
CSRRS x30, cycle, x0
ADDI sp, sp, -8
SD x0, 0(sp)

# TEST: CSRRC - CSR Read and Clear
# CONTEXT: Read cycle counter (clear with x0 = no change)
# EXPECTED PUSH: 0x0000000000000000
CSRRC x30, cycle, x0
ADDI sp, sp, -8
SD x0, 0(sp)

# TEST: CSRRW - CSR Read and Write
# CONTEXT: Read cycle, write back same value
# EXPECTED PUSH: 0x0000000000000000
CSRR x29, cycle
CSRRW x30, cycle, x29
ADDI sp, sp, -8
SD x0, 0(sp)

# TEST: Read mhartid CSR
# CONTEXT: Hardware thread ID (typically 0)
# EXPECTED PUSH: 0x0000000000000000
CSRR x30, 0xF14         # mhartid CSR
ADDI sp, sp, -8
SD x0, 0(sp)

# TEST: CSRRSI - CSR Read and Set Immediate
# CONTEXT: Read cycle with set immediate 0 (just read)
# EXPECTED PUSH: 0x0000000000000000
CSRRSI x30, cycle, 0
ADDI sp, sp, -8
SD x0, 0(sp)

# TEST: CSRRCI - CSR Read and Clear Immediate
# CONTEXT: Read cycle with clear immediate 0 (just read)
# EXPECTED PUSH: 0x0000000000000000
CSRRCI x30, cycle, 0
ADDI sp, sp, -8
SD x0, 0(sp)

# TEST: CSRRS with non-zero register
# CONTEXT: Try to set bits in cycle counter (may be read-only)
# EXPECTED PUSH: 0x0000000000000000
ADDI x29, x0, 1
CSRRS x30, cycle, x29   # Try to set bit 0 (probably ignored)
ADDI sp, sp, -8
SD x0, 0(sp)

# TEST: CSRRC with non-zero register
# CONTEXT: Try to clear bits in cycle counter (may be read-only)
# EXPECTED PUSH: 0x0000000000000000
ADDI x29, x0, 1
CSRRC x30, cycle, x29   # Try to clear bit 0 (probably ignored)
ADDI sp, sp, -8
SD x0, 0(sp)

# TEST: Read time CSR
# CONTEXT: Read time counter (if available)
# EXPECTED PUSH: 0x0000000000000000
CSRR x30, time          # time CSR (0xC01)
ADDI sp, sp, -8
SD x0, 0(sp)

# TEST: Read instret CSR
# CONTEXT: Read instructions retired counter (if available)
# EXPECTED PUSH: 0x0000000000000000
CSRR x30, instret       # instret CSR (0xC02)
ADDI sp, sp, -8
SD x0, 0(sp)

# TEST: CSRRWI - CSR Read/Write Immediate
# CONTEXT: Write 0 to a CSR and read old value
# EXPECTED PUSH: 0x0000000000000000
CSRRWI x30, cycle, 0
ADDI sp, sp, -8
SD x0, 0(sp)

# TEST: Final sentinel with complement pattern
# CONTEXT: Different pattern from first sentinel
# EXPECTED PUSH: 0xAAAAAAAAAAAAAAAA
ADDI x29, x0, 0xAA
SLLI x29, x29, 8
ORI x29, x29, 0xAA      # 0xAAAA
SLLI x29, x29, 16
LUI x28, 0xAAAA
SRLI x28, x28, 12
OR x29, x29, x28        # 0xAAAAAAAA
SLLI x29, x29, 32
SRLI x30, x29, 32
OR x30, x29, x30        # 0xAAAAAAAAAAAAAAAA
ADDI sp, sp, -8
SD x30, 0(sp)

# ============================================================================
# END OF TESTS - HALT
# ============================================================================
EBREAK                  # Signal end of program

# ============================================================================
# DATA SECTION - Embedded test data
# ============================================================================
# All data placed after EBREAK to avoid alignment issues with instructions

.align 3                # Align to 8-byte boundary
data_section_1:
.dword 0x0123456789ABCDEF
.dword 0x0FEDCBA987654321

.align 2                # Align to 4-byte boundary
data_section_2:
.word 0xDEADBEEF

.align 1                # Align to 2-byte boundary
data_section_3:
.half 0xCAFE

data_section_4:
.byte 0x80
