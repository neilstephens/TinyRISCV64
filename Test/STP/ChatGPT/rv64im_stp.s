# TEST: Initialize register x2
# EXPECTED PUSH: 0x0000000000000005
ADDI x2, x0, 5
PUSH x2

# TEST: Initialize register x3
# EXPECTED PUSH: 0x000000000000000A
ADDI x3, x0, 10
PUSH x3

# TEST: ADD (simple)
# CONTEXT: x2 = 5, x3 = 10
# EXPECTED PUSH: 0x000000000000000F
ADD x5, x2, x3
PUSH x5

# TEST: ADDI (simple)
# CONTEXT: x5 = 15
# EXPECTED PUSH: 0x0000000000000010
ADDI x6, x5, 1
PUSH x6

# TEST: ADDI (sign extension)
# EXPECTED PUSH: 0xFFFFFFFFFFFFFFFF
ADDI x7, x0, -1
PUSH x7

# TEST: ADD overflow (min + min)
# EXPECTED PUSH: 0x0000000000000000
ADDI x10, x0, 1
SLLI x10, x10, 63    # x10 = 0x8000000000000000 (min int)
SUB  x11, x0, x10    # x11 = 0x8000000000000000
ADD  x12, x10, x11   # wrap-around sum = 0
PUSH x12

# TEST: AND (bitwise)
# EXPECTED PUSH: 0x0000000000000000
AND  x13, x2, x3     # 0b0101 & 0b1010 = 0
PUSH x13

# TEST: OR (bitwise)
# EXPECTED PUSH: 0x000000000000000F
OR   x14, x2, x3     # 0b0101 | 0b1010 = 0b1111 (15)
PUSH x14

# TEST: XOR (bitwise)
# EXPECTED PUSH: 0x000000000000000F
XOR  x15, x2, x3     # 0b0101 ^ 0b1010 = 0b1111 (15)
PUSH x15

# TEST: SLLI (left shift immediate)
# CONTEXT: x6 = 16 (0x10)
# EXPECTED PUSH: 0x0000000000000020
ADDI x6, x0, 16
SLLI x16, x6, 1      # 0x10 << 1 = 0x20
PUSH x16

# TEST: SLL (left shift register, large shift)
# EXPECTED PUSH: 0x0000000000000020
ADDI x18, x0, 65
SLL  x17, x6, x18    # shift by 65 mod 64 = 1, result = 0x20
PUSH x17

# TEST: SRLI (logical right shift immediate)
# EXPECTED PUSH: 0x0FFFFFFFFFFFFFFF
ADDI x13, x0, -1
SRLI x20, x13, 4     # 0xFFFFFFFFFFFFFFFF >>4 = 0x0FFFFFFFFFFFFFFF
PUSH x20

# TEST: SRAI (arithmetic right shift immediate)
# EXPECTED PUSH: 0xFFFFFFFFFFFFFFFF
SRAI x21, x13, 4     # arithmetic keeps sign: -1 >>4 = -1
PUSH x21

# TEST: SRL (logical right shift register, large shift)
# EXPECTED PUSH: 0x7FFFFFFFFFFFFFFF
SRL  x22, x13, x18   # -1 (all 1s) >>1 logically = 0x7FFFFFFFFFFFFFFF
PUSH x22

# TEST: SRA (arithmetic right shift register, large shift)
# EXPECTED PUSH: 0xFFFFFFFFFFFFFFFF
SRA  x23, x13, x18   # arithmetic >>1 on -1 remains -1
PUSH x23

# TEST: SLT (signed comparison)
# CONTEXT: x2 = -1, x3 = 10
# EXPECTED PUSH: 0x0000000000000001
ADDI x2, x0, -1
SLT  x24, x2, x3     # -1 < 10 is true (1)
PUSH x24

# TEST: SLTU (unsigned comparison)
# EXPECTED PUSH: 0x0000000000000000
SLTU x25, x2, x3     # as unsigned, 0xFFFF.. > 10, so false (0)
PUSH x25

# TEST: BNE (not taken)
# EXPECTED PUSH: 0x0000000000000001
BNE  x0, x0, SKIP_BNE
ADDI x29, x0, 1
SKIP_BNE:
PUSH x29         # BNE x0,x0 is false (not taken), so we set x29=1 and push it

# TEST: SB and LB (sign-extended byte)
# CONTEXT: store 0xAB at (sp+1)
# EXPECTED PUSH: 0xFFFFFFFFFFFFFFAB
ADDI x29, sp, 1
ADDI x30, x0, 171     # 0xAB
SB   x30, 0(x29)
LB   x31, 0(x29)      # loads byte 0xAB, sign-extended => 0xFFFFFFFFFFFFFFAB
PUSH x31

# TEST: LBU (unsigned byte)
# EXPECTED PUSH: 0x00000000000000AB
LBU  x32, 0(x29)      # loads unsigned byte => 0x00000000000000AB
PUSH x32

# TEST: SH and LH (sign-extended halfword)
# CONTEXT: store 0x8001 at (sp+2)
# EXPECTED PUSH: 0xFFFFFFFFFFFF8001
ADDI x29, sp, 2
ADDI x30, x0, 1
SLLI x30, x30, 15    # x30 = 0x8000
ADDI x30, x30, 1     # x30 = 0x8001
SH   x30, 0(x29)
LH   x31, 0(x29)     # loads 0x8001 as signed 16-bit => 0xFFFFFFFFFFFF8001
PUSH x31

# TEST: LHU (unsigned halfword)
# EXPECTED PUSH: 0x0000000000008001
LHU  x32, 0(x29)     # loads unsigned halfword => 0x0000000000008001
PUSH x32

# TEST: SW and LW (sign-extended word)
# CONTEXT: store 0x80000001 at (sp+4)
# EXPECTED PUSH: 0xFFFFFFFF80000001
ADDI x29, sp, 4
ADDI x30, x0, 1
SLLI x30, x30, 31    # x30 = 0x80000000
ADDI x30, x30, 1     # x30 = 0x80000001
SW   x30, 0(x29)
LW   x31, 0(x29)     # loads 0x80000001 sign-extended => 0xFFFFFFFF80000001
PUSH x31

# TEST: LWU (unsigned word)
# EXPECTED PUSH: 0x0000000080000001
LWU  x32, 0(x29)     # loads unsigned word => 0x0000000080000001
PUSH x32

# TEST: Misaligned SH then LH
# CONTEXT: store 0x1234 at (sp+1)
# EXPECTED PUSH: 0x0000000000001234
ADDI x29, sp, 1
ADDI x30, x0, 1
SLLI x30, x30, 12    # x30 = 0x1000
ADDI x30, x30, 564   # x30 = 0x1234
SH   x30, 0(x29)
LH   x31, 0(x29)     # may succeed (writes at unaligned addr); expected load 0x1234
PUSH x31

# TEST: MUL (signed)
# CONTEXT: x2 = -1, x3 = 10
# EXPECTED PUSH: 0xFFFFFFFFFFFFFFF6
MUL  x32, x2, x3     # -1 * 10 = -10 => 0xFFFFFFFFFFFFFFF6
PUSH x32

# TEST: MUL (positive)
# EXPECTED PUSH: 0x000000000000001E
ADDI x2, x0, 3
MUL  x32, x2, x3     # 3 * 10 = 30 => 0x1E
PUSH x32

# TEST: MULH (unsigned high-word)
# CONTEXT: x2 = 3<<32, x3 = 5<<32
# EXPECTED PUSH: 0x000000000000000F
ADDI x2, x0, 3
SLLI x2, x2, 32
ADDI x3, x0, 5
SLLI x3, x3, 32
MULH x32, x2, x3    # (3*5)<<64 => high 64 bits = 15 (0xF)
PUSH x32

# TEST: DIV (positive)
# CONTEXT: x2 = 5, x3 = 2
# EXPECTED PUSH: 0x0000000000000002
ADDI x2, x0, 5
ADDI x3, x0, 2
DIV  x33, x2, x3     # 5/2 = 2
PUSH x33

# TEST: REM (positive)
# EXPECTED PUSH: 0x0000000000000001
REM  x34, x2, x3     # 5%2 = 1
PUSH x34

# TEST: DIV (negative)
# CONTEXT: x2 = -5, x3 = 2
# EXPECTED PUSH: 0xFFFFFFFFFFFFFFFE
ADDI x2, x0, -5
DIV  x33, x2, x3     # -5/2 = -2 (towards zero)
PUSH x33

# TEST: REM (negative)
# EXPECTED PUSH: 0xFFFFFFFFFFFFFFFF
REM  x34, x2, x3     # -5%2 = -1
PUSH x34

# TEST: DIV by zero
# EXPECTED PUSH: 0xFFFFFFFFFFFFFFFF
DIV  x33, x2, x0     # division by 0 => quotient all 1s (per spec)
PUSH x33

# TEST: REM by zero
# EXPECTED PUSH: 0xFFFFFFFFFFFFFFFB
REM  x34, x2, x0     # remainder = dividend = -5 => 0xFFFFFFFFFFFFFFFB
PUSH x34

EBREAK
