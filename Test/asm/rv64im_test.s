# 
# MIT License
# 
# Copyright (c) 2025 Neil Stephens
# 
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
# 
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
# 
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
# 

# RV64IM Opcode Test Suite
# Tests opcodes with edge cases designed to uncover VM defects
# Results pushed after each test

_start:
    # Stack pointer (x2/sp) assumed to be already initialized
    
    # Initialize test registers with known patterns
    li x1, 0xDEADBEEFCAFEBABE
    li x3, 0xFFFFFFFFFFFFFFFF  # All ones
    li x4, 0x8000000000000000  # Most negative 64-bit
    li x5, 0x7FFFFFFFFFFFFFFF  # Most positive 64-bit
    li x6, 0x0000000000000001  # One
    li x7, 0x0000000000000000  # Zero
    
    #========================================
    # RV64I Base Integer Instructions
    #========================================
    
    # LUI - Load Upper Immediate
    lui x8, 0x12345
    sd x8, -8(sp)
    addi sp, sp, -8
    # Result: 0x0000000012345000
    
    # AUIPC - Add Upper Immediate to PC
    auipc x9, 0x0
    # Don't push - PC-relative, value varies
    
    # JAL - Jump and Link
    jal x10, jal_target
jal_target:
    sd x10, -8(sp)
    addi sp, sp, -8
    # Result: 0x0000000000000054
    #(PC+4 of JAL instruction)
    
    # JALR - Jump and Link Register
    la x11, jalr_target
    jalr x12, x11, 0
jalr_target:
    sd x12, -8(sp)
    addi sp, sp, -8
    # Result: 0x0000000000000068
    #(PC+4 of JALR instruction)
    
    #========================================
    # Branch Instructions (test without jumping)
    #========================================
    
    # BEQ - Branch if Equal
    li x13, 42
    li x14, 42
    beq x13, x14, beq_taken
    li x15, 0  # Should not execute
    j beq_done
beq_taken:
    li x15, 1  # Should execute
beq_done:
    sd x15, -8(sp)
    addi sp, sp, -8
    # Result: 0x0000000000000001
    
    # BNE - Branch if Not Equal
    li x16, 42
    li x17, 43
    bne x16, x17, bne_taken
    li x18, 0
    j bne_done
bne_taken:
    li x18, 1
bne_done:
    sd x18, -8(sp)
    addi sp, sp, -8
    # Result: 0x0000000000000001
    
    # BLT - Branch if Less Than (signed)
    li x19, -1
    li x20, 1
    blt x19, x20, blt_taken
    li x21, 0
    j blt_done
blt_taken:
    li x21, 1
blt_done:
    sd x21, -8(sp)
    addi sp, sp, -8
    # Result: 0x0000000000000001
    
    # BGE - Branch if Greater or Equal (signed)
    li x22, 1
    li x23, -1
    bge x22, x23, bge_taken
    li x24, 0
    j bge_done
bge_taken:
    li x24, 1
bge_done:
    sd x24, -8(sp)
    addi sp, sp, -8
    # Result: 0x0000000000000001
    
    # BLTU - Branch if Less Than (unsigned)
    li x25, 0x7FFFFFFFFFFFFFFF
    li x26, 0x8000000000000000
    bltu x25, x26, bltu_taken
    li x27, 0
    j bltu_done
bltu_taken:
    li x27, 1
bltu_done:
    sd x27, -8(sp)
    addi sp, sp, -8
    # Result: 0x0000000000000001
    
    # BGEU - Branch if Greater or Equal (unsigned)
    li x28, 0x8000000000000000
    li x29, 0x7FFFFFFFFFFFFFFF
    bgeu x28, x29, bgeu_taken
    li x30, 0
    j bgeu_done
bgeu_taken:
    li x30, 1
bgeu_done:
    sd x30, -8(sp)
    addi sp, sp, -8
    # Result: 0x0000000000000001
    
    #========================================
    # Load/Store Instructions
    #========================================
    
    # Setup memory area
    la x31, test_data
    
    # LB - Load Byte (sign-extended)
    lb x8, 0(x31)
    sd x8, -8(sp)
    addi sp, sp, -8
    # Result: 0x0000000000000000
    #(first byte is 0x00)
    
    # LH - Load Halfword (sign-extended)
    lh x9, 0(x31)
    sd x9, -8(sp)
    addi sp, sp, -8
    # Result: 0x0000000000000000
    #(first halfword is 0x0000)
    
    # LW - Load Word (sign-extended)
    lw x10, 0(x31)
    sd x10, -8(sp)
    addi sp, sp, -8
    # Result: 0x0000000000000000
    #(first word is 0x00000000)
    
    # LD - Load Doubleword
    ld x11, 0(x31)
    sd x11, -8(sp)
    addi sp, sp, -8
    # Result: 0x0000000000000000
    
    # LBU - Load Byte Unsigned
    lbu x12, 8(x31)
    sd x12, -8(sp)
    addi sp, sp, -8
    # Result: 0x00000000000000FF
    #(first byte of second dword)
    
    # LB - Load Byte (sign-extended from 0xFF)
    lb x8, 8(x31)
    sd x8, -8(sp)
    addi sp, sp, -8
    # Result: 0xFFFFFFFFFFFFFFFF
    #(sign-extended from 0xFF)
    
    # LHU - Load Halfword Unsigned
    lhu x13, 8(x31)
    sd x13, -8(sp)
    addi sp, sp, -8
    # Result: 0x000000000000FFFF
    #(first halfword of second dword)
    
    # LH - Load Halfword (sign-extended from 0xFFFF)
    lh x9, 8(x31)
    sd x9, -8(sp)
    addi sp, sp, -8
    # Result: 0xFFFFFFFFFFFFFFFF
    #(sign-extended from 0xFFFF)
    
    # LWU - Load Word Unsigned (RV64I)
    lwu x14, 8(x31)
    sd x14, -8(sp)
    addi sp, sp, -8
    # Result: 0x00000000FFFFFFFF
    #(first word of second dword)
    
    # LW - Load Word (sign-extended from 0xFFFFFFFF)
    lw x10, 8(x31)
    sd x10, -8(sp)
    addi sp, sp, -8
    # Result: 0xFFFFFFFFFFFFFFFF
    #(sign-extended from 0xFFFFFFFF)
    
    # Store instructions (write then read back)
    li x15, 0x123456789ABCDEF0
    
    # SB - Store Byte
    sb x15, 8(x31)
    lb x16, 8(x31)
    sd x16, -8(sp)
    addi sp, sp, -8
    # Result: 0xFFFFFFFFFFFFFFF0
    #(sign-extended)
    
    # SH - Store Halfword
    sh x15, 16(x31)
    lh x17, 16(x31)
    sd x17, -8(sp)
    addi sp, sp, -8
    # Result: 0xFFFFFFFFFFFFDEF0
    #(sign-extended)
    
    # SW - Store Word
    sw x15, 24(x31)
    lw x18, 24(x31)
    sd x18, -8(sp)
    addi sp, sp, -8
    # Result: 0xFFFFFFFF9ABCDEF0
    #(sign-extended)
    
    # SD - Store Doubleword
    sd x15, 32(x31)
    ld x19, 32(x31)
    sd x19, -8(sp)
    addi sp, sp, -8
    # Result: 0x123456789ABCDEF0
    
    #========================================
    # Immediate Arithmetic Instructions
    #========================================
    
    # ADDI - Add Immediate
    li x20, 100
    addi x21, x20, -50
    sd x21, -8(sp)
    addi sp, sp, -8
    # Result: 0x0000000000000032
    #(50)
    
    # ADDI overflow test
    li x22, 0x7FFFFFFFFFFFFFFF
    addi x23, x22, 1
    sd x23, -8(sp)
    addi sp, sp, -8
    # Result: 0x8000000000000000
    #(overflow wraps)
    
    # SLTI - Set Less Than Immediate (signed)
    li x24, -5
    slti x25, x24, 0
    sd x25, -8(sp)
    addi sp, sp, -8
    # Result: 0x0000000000000001
    
    # SLTIU - Set Less Than Immediate Unsigned
    li x26, 0xFFFFFFFFFFFFFFFF
    sltiu x27, x26, 1
    sd x27, -8(sp)
    addi sp, sp, -8
    # Result: 0x0000000000000000
    
    # XORI - XOR Immediate
    li x28, 0xFF00FF00FF00FF00
    xori x29, x28, -1
    sd x29, -8(sp)
    addi sp, sp, -8
    # Result: 0x00FF00FF00FF00FF
    
    # ORI - OR Immediate
    li x30, 0x1234567812345678
    ori x8, x30, 0x0F0
    sd x8, -8(sp)
    addi sp, sp, -8
    # Result: 0x12345678123456F8
    
    # ANDI - AND Immediate
    li x9, 0xFFFFFFFFFFFFFFFF
    andi x10, x9, 0x555
    sd x10, -8(sp)
    addi sp, sp, -8
    # Result: 0x0000000000000555
    
    # SLLI - Shift Left Logical Immediate
    li x11, 0x0000000000000003
    slli x12, x11, 62
    sd x12, -8(sp)
    addi sp, sp, -8
    # Result: 0xC000000000000000
    
    # SRLI - Shift Right Logical Immediate
    li x13, 0xF000000000000000
    srli x14, x13, 60
    sd x14, -8(sp)
    addi sp, sp, -8
    # Result: 0x000000000000000F
    
    # SRAI - Shift Right Arithmetic Immediate
    li x15, 0x8000000000000000
    srai x16, x15, 63
    sd x16, -8(sp)
    addi sp, sp, -8
    # Result: 0xFFFFFFFFFFFFFFFF
    #(sign-extended)
    
    #========================================
    # RV64I 32-bit Immediate Instructions
    #========================================
    
    # ADDIW - Add Immediate Word
    li x17, 0x7FFFFFFF
    addiw x18, x17, 1
    sd x18, -8(sp)
    addi sp, sp, -8
    # Result: 0xFFFFFFFF80000000
    #(32-bit overflow, sign-extended)
    
    # SLLIW - Shift Left Logical Immediate Word
    li x19, 0x00000001
    slliw x20, x19, 31
    sd x20, -8(sp)
    addi sp, sp, -8
    # Result: 0xFFFFFFFF80000000
    #(sign-extended)
    
    # SRLIW - Shift Right Logical Immediate Word
    li x21, 0xFFFFFFFF80000000
    srliw x22, x21, 31
    sd x22, -8(sp)
    addi sp, sp, -8
    # Result: 0x0000000000000001
    
    # SRAIW - Shift Right Arithmetic Immediate Word
    li x23, 0xFFFFFFFF80000000
    sraiw x24, x23, 31
    sd x24, -8(sp)
    addi sp, sp, -8
    # Result: 0xFFFFFFFFFFFFFFFF
    #(sign-extended)
    
    #========================================
    # Register-Register Arithmetic
    #========================================
    
    # ADD - Add
    li x25, 0x123456789ABCDEF0
    li x26, 0x0FEDCBA987654321
    add x27, x25, x26
    sd x27, -8(sp)
    addi sp, sp, -8
    # Result: 0x2222222222222211
    
    # SUB - Subtract
    li x28, 0x2222222222222222
    li x29, 0x1111111111111111
    sub x30, x28, x29
    sd x30, -8(sp)
    addi sp, sp, -8
    # Result: 0x1111111111111111
    
    # SLL - Shift Left Logical
    li x8, 0x0000000000000001
    li x9, 63
    sll x10, x8, x9
    sd x10, -8(sp)
    addi sp, sp, -8
    # Result: 0x8000000000000000
    
    # SLT - Set Less Than
    li x11, -1
    li x12, 1
    slt x13, x11, x12
    sd x13, -8(sp)
    addi sp, sp, -8
    # Result: 0x0000000000000001
    
    # SLTU - Set Less Than Unsigned
    li x14, 0xFFFFFFFFFFFFFFFF
    li x15, 0x0000000000000001
    sltu x16, x14, x15
    sd x16, -8(sp)
    addi sp, sp, -8
    # Result: 0x0000000000000000
    
    # XOR - Exclusive OR
    li x17, 0xAAAAAAAAAAAAAAAA
    li x18, 0x5555555555555555
    xor x19, x17, x18
    sd x19, -8(sp)
    addi sp, sp, -8
    # Result: 0xFFFFFFFFFFFFFFFF
    
    # SRL - Shift Right Logical
    li x20, 0xF000000000000000
    li x21, 4
    srl x22, x20, x21
    sd x22, -8(sp)
    addi sp, sp, -8
    # Result: 0x0F00000000000000
    
    # SRA - Shift Right Arithmetic
    li x23, 0x8000000000000000
    li x24, 1
    sra x25, x23, x24
    sd x25, -8(sp)
    addi sp, sp, -8
    # Result: 0xC000000000000000
    
    # OR - Bitwise OR
    li x26, 0x0F0F0F0F0F0F0F0F
    li x27, 0xF0F0F0F0F0F0F0F0
    or x28, x26, x27
    sd x28, -8(sp)
    addi sp, sp, -8
    # Result: 0xFFFFFFFFFFFFFFFF
    
    # AND - Bitwise AND
    li x29, 0xFFFFFFFF00000000
    li x30, 0x00000000FFFFFFFF
    and x8, x29, x30
    sd x8, -8(sp)
    addi sp, sp, -8
    # Result: 0x0000000000000000
    
    #========================================
    # RV64I 32-bit Register-Register
    #========================================
    
    # ADDW - Add Word
    li x9, 0x7FFFFFFF
    li x10, 0x00000001
    addw x11, x9, x10
    sd x11, -8(sp)
    addi sp, sp, -8
    # Result: 0xFFFFFFFF80000000
    
    # SUBW - Subtract Word
    li x12, 0x80000000
    li x13, 0x00000001
    subw x14, x12, x13
    sd x14, -8(sp)
    addi sp, sp, -8
    # Result: 0x000000007FFFFFFF
    
    # SLLW - Shift Left Logical Word
    li x15, 0x00000001
    li x16, 31
    sllw x17, x15, x16
    sd x17, -8(sp)
    addi sp, sp, -8
    # Result: 0xFFFFFFFF80000000
    
    # SRLW - Shift Right Logical Word
    li x18, 0xFFFFFFFF80000000
    li x19, 31
    srlw x20, x18, x19
    sd x20, -8(sp)
    addi sp, sp, -8
    # Result: 0x0000000000000001
    
    # SRAW - Shift Right Arithmetic Word
    li x21, 0xFFFFFFFF80000000
    li x22, 1
    sraw x23, x21, x22
    sd x23, -8(sp)
    addi sp, sp, -8
    # Result: 0xFFFFFFFFC0000000
    
    #========================================
    # RV64M Extension (Multiply/Divide)
    #========================================
    
    # MUL - Multiply (lower 64 bits)
    li x24, 0x0000000100000000
    li x25, 0x0000000100000000
    mul x26, x24, x25
    sd x26, -8(sp)
    addi sp, sp, -8
    # Result: 0x0000000000000000
    #(lower 64 bits)
    
    # MULH - Multiply High (signed × signed)
    li x27, 0x8000000000000000
    li x28, 2
    mulh x29, x27, x28
    sd x29, -8(sp)
    addi sp, sp, -8
    # Result: 0xFFFFFFFFFFFFFFFF
    #(negative result)
    
    # MULHSU - Multiply High (signed × unsigned)
    li x30, -1
    li x8, 2
    mulhsu x9, x30, x8
    sd x9, -8(sp)
    addi sp, sp, -8
    # Result: 0xFFFFFFFFFFFFFFFF
    
    # MULHU - Multiply High (unsigned × unsigned)
    li x10, 0xFFFFFFFFFFFFFFFF
    li x11, 0xFFFFFFFFFFFFFFFF
    mulhu x12, x10, x11
    sd x12, -8(sp)
    addi sp, sp, -8
    # Result: 0xFFFFFFFFFFFFFFFE
    
    # DIV - Divide (signed)
    li x13, -100
    li x14, 3
    div x15, x13, x14
    sd x15, -8(sp)
    addi sp, sp, -8
    # Result: 0xFFFFFFFFFFFFFFDF
    #(-33, rounded)
    
    # DIV overflow case
    li x16, 0x8000000000000000
    li x17, -1
    div x18, x16, x17
    sd x18, -8(sp)
    addi sp, sp, -8
    # Result: 0x8000000000000000
    #(overflow exception case)
    
    # DIV by zero
    li x19, 100
    li x20, 0
    div x21, x19, x20
    sd x21, -8(sp)
    addi sp, sp, -8
    # Result: 0xFFFFFFFFFFFFFFFF
    #(-1)
    
    # DIVU - Divide (unsigned)
    li x22, 0xFFFFFFFFFFFFFFFF
    li x23, 2
    divu x24, x22, x23
    sd x24, -8(sp)
    addi sp, sp, -8
    # Result: 0x7FFFFFFFFFFFFFFF
    
    # DIVU by zero
    li x25, 100
    li x26, 0
    divu x27, x25, x26
    sd x27, -8(sp)
    addi sp, sp, -8
    # Result: 0xFFFFFFFFFFFFFFFF
    
    # REM - Remainder (signed)
    li x28, 100
    li x29, 7
    rem x30, x28, x29
    sd x30, -8(sp)
    addi sp, sp, -8
    # Result: 0x0000000000000002
    
    # REM with negative dividend
    li x8, -100
    li x9, 7
    rem x10, x8, x9
    sd x10, -8(sp)
    addi sp, sp, -8
    # Result: 0xFFFFFFFFFFFFFFFE
    #(-2, remainder takes sign of dividend)
    
    # REM by zero
    li x11, 100
    li x12, 0
    rem x13, x11, x12
    sd x13, -8(sp)
    addi sp, sp, -8
    # Result: 0x0000000000000064
    #(dividend)
    
    # REMU - Remainder (unsigned)
    li x14, 0xFFFFFFFFFFFFFFFF
    li x15, 10
    remu x16, x14, x15
    sd x16, -8(sp)
    addi sp, sp, -8
    # Result: 0x0000000000000005
    #(remainder of max_uint64 / 10)
    
    # REMU by zero
    li x17, 100
    li x18, 0
    remu x19, x17, x18
    sd x19, -8(sp)
    addi sp, sp, -8
    # Result: 0x0000000000000064
    #(dividend)
    
    #========================================
    # RV64M 32-bit Multiply/Divide
    #========================================
    
    # MULW - Multiply Word
    li x20, 0x0000000080000000
    li x21, 2
    mulw x22, x20, x21
    sd x22, -8(sp)
    addi sp, sp, -8
    # Result: 0x0000000000000000
    
    # DIVW - Divide Word (signed)
    li x23, 0xFFFFFFFF80000000  # -2147483648 (32-bit)
    li x24, -1
    divw x25, x23, x24
    sd x25, -8(sp)
    addi sp, sp, -8
    # Result: 0xFFFFFFFF80000000
    #(overflow case)
    
    # DIVW by zero
    li x26, 100
    li x27, 0
    divw x28, x26, x27
    sd x28, -8(sp)
    addi sp, sp, -8
    # Result: 0xFFFFFFFFFFFFFFFF
    
    # DIVUW - Divide Word (unsigned)
    li x29, 0xFFFFFFFF80000000
    li x30, 2
    divuw x8, x29, x30
    sd x8, -8(sp)
    addi sp, sp, -8
    # Result: 0x0000000040000000
    
    # DIVUW by zero
    li x9, 100
    li x10, 0
    divuw x11, x9, x10
    sd x11, -8(sp)
    addi sp, sp, -8
    # Result: 0xFFFFFFFFFFFFFFFF
    #(0xFFFFFFFF sign-extended)
    
    # REMW - Remainder Word (signed)
    li x12, 100
    li x13, 7
    remw x14, x12, x13
    sd x14, -8(sp)
    addi sp, sp, -8
    # Result: 0x0000000000000002
    
    # REMW by zero
    li x15, 100
    li x16, 0
    remw x17, x15, x16
    sd x17, -8(sp)
    addi sp, sp, -8
    # Result: 0x0000000000000064
    
    # REMUW - Remainder Word (unsigned)
    li x18, 0x0000000080000000
    li x19, 3
    remuw x20, x18, x19
    sd x20, -8(sp)
    addi sp, sp, -8
    # Result: 0x0000000000000002
    
    # REMUW by zero
    li x21, 100
    li x22, 0
    remuw x23, x21, x22
    sd x23, -8(sp)
    addi sp, sp, -8
    # Result: 0x0000000000000064
    
    #========================================
    # System Instructions
    #========================================
    
    # FENCE - Memory ordering (no visible effect in single-threaded)
    fence
    
    # FENCE.I - Instruction fence (no visible effect in this test)
    fence.i
    
    # ECALL would trap to supervisor, skip in user-mode test
    # EBREAK - Environment break (end of test)
    
    #========================================
    # CSR Instructions (if accessible)
    #========================================
    
    # CSRRW - CSR Read/Write
    # Read cycle counter (if available)
    csrr x24, cycle
    sd x24, -8(sp)
    addi sp, sp, -8
    # Result: some positive value (cycle count)
    
    # CSRRS - CSR Read/Set
    csrr x25, cycle
    sd x25, -8(sp)
    addi sp, sp, -8
    # Result: some positive value
    
    # CSRRC - CSR Read/Clear (read without clearing)
    csrrc x26, cycle, x0
    sd x26, -8(sp)
    addi sp, sp, -8
    # Result: some positive value
    
    # CSRRWI - CSR Read/Write Immediate
    # Try to read mhartid (hardware thread ID)
    csrr x27, 0xF14  # mhartid
    sd x27, -8(sp)
    addi sp, sp, -8
    # Result: 0x0000000000000000
    #(typically single-hart)
    
    # CSRRSI - CSR Read/Set Immediate (read only)
    csrrsi x28, cycle, 0
    sd x28, -8(sp)
    addi sp, sp, -8
    # Result: some positive value
    
    # CSRRCI - CSR Read/Clear Immediate (read only)
    csrrci x29, cycle, 0
    sd x29, -8(sp)
    addi sp, sp, -8
    # Result: some positive value
    
    #========================================
    # End of Tests
    #========================================
    
    # Store final stack pointer for reference
    mv x30, sp
    
    # Break to debugger/simulator
    ebreak
    
    # Infinite loop in case execution continues
halt:
    j halt

#========================================
# Test Data Section
#========================================
test_data:
    .dword 0x0000000000000000  # Zero (for testing zero extension)
    .dword 0xFFFFFFFFFFFFFFFF  # All ones (for testing sign extension)
    .dword 0x8000000000000000  # Most negative 64-bit
    .dword 0x7FFFFFFFFFFFFFFF  # Most positive 64-bit
    .space 64  # Space for store tests
