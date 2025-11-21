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
 
get_addrs:
        addi    sp,sp,-128
        sd      ra,120(sp)
        sd      s0,112(sp)
        addi    s0,sp,128
        sd      a0,-104(s0)
        sd      a1,-112(s0)
        sd      a2,-120(s0)
        sd      a3,-128(s0)
        ld      a4,-112(s0)
        li      a5,31
        bgtu    a4,a5,.L2
        li      a5,-1
        j       .L3
.L2:
        sd      zero,-24(s0)
        li      a5,373633024
        addi    a5,a5,1673
        sd      a5,-32(s0)
        ld      a5,-32(s0)
        slli    a5,a5,32
        sd      a5,-32(s0)
        ld      a4,-32(s0)
        li      a5,2137862144
        addi    a5,a5,-1025
        slli    a5,a5,1
        add     a5,a4,a5
        sd      a5,-32(s0)
        li      a5,882208768
        addi    a5,a5,656
        sd      a5,-40(s0)
        ld      a5,-40(s0)
        slli    a5,a5,32
        sd      a5,-40(s0)
        ld      a4,-40(s0)
        li      a5,1083723776
        addi    a5,a5,1842
        add     a5,a4,a5
        sd      a5,-40(s0)
        sd      zero,-48(s0)
        j       .L4
.L11:
        sd      zero,-56(s0)
        sd      zero,-64(s0)
        sw      zero,-68(s0)
        j       .L5
.L6:
        lw      a4,-68(s0)
        ld      a5,-48(s0)
        add     a5,a4,a5
        ld      a4,-104(s0)
        add     a5,a4,a5
        lbu     a5,0(a5)
        mv      a4,a5
        lw      a5,-68(s0)
        slliw   a5,a5,3
        sext.w  a5,a5
        sll     a5,a4,a5
        ld      a4,-56(s0)
        or      a5,a4,a5
        sd      a5,-56(s0)
        lw      a4,-68(s0)
        ld      a5,-48(s0)
        add     a5,a4,a5
        addi    a5,a5,8
        ld      a4,-104(s0)
        add     a5,a4,a5
        lbu     a5,0(a5)
        mv      a4,a5
        lw      a5,-68(s0)
        slliw   a5,a5,3
        sext.w  a5,a5
        sll     a5,a4,a5
        ld      a4,-64(s0)
        or      a5,a4,a5
        sd      a5,-64(s0)
        lw      a5,-68(s0)
        addiw   a5,a5,1
        sw      a5,-68(s0)
.L5:
        lw      a5,-68(s0)
        sext.w  a4,a5
        li      a5,7
        ble     a4,a5,.L6
        ld      a4,-104(s0)
        ld      a5,-48(s0)
        add     a5,a4,a5
        ld      a5,0(a5)
        sd      a5,-80(s0)
        ld      a5,-48(s0)
        addi    a5,a5,8
        ld      a4,-104(s0)
        add     a5,a4,a5
        ld      a5,0(a5)
        sd      a5,-88(s0)
        ld      a4,-56(s0)
        ld      a5,-80(s0)
        bne     a4,a5,.L7
        ld      a4,-64(s0)
        ld      a5,-88(s0)
        beq     a4,a5,.L8
.L7:
        li      a5,-1
        j       .L3
.L8:
        ld      a1,-64(s0)
        ld      a0,-56(s0)
        call    alu_stuff
        sd      a0,-96(s0)
        ld      a4,-96(s0)
        ld      a5,-24(s0)
        bgtu    a4,a5,.L9
        ld      a5,-48(s0)
        bne     a5,zero,.L10
.L9:
        ld      a5,-96(s0)
        sd      a5,-24(s0)
        ld      a4,-32(s0)
        ld      a5,-96(s0)
        xor     a5,a4,a5
        sd      a5,-32(s0)
        ld      a4,-96(s0)
        ld      a5,-40(s0)
        xor     a5,a4,a5
        not     a5,a5
        sd      a5,-40(s0)
        ld      a5,-48(s0)
        addi    a5,a5,8
        ld      a4,-104(s0)
        add     a4,a4,a5
        ld      a3,-104(s0)
        ld      a5,-48(s0)
        add     a5,a3,a5
        ld      a4,0(a4)
        sd      a4,0(a5)
        ld      a5,-48(s0)
        addi    a5,a5,8
        ld      a4,-104(s0)
        add     a5,a4,a5
        ld      a4,-56(s0)
        sd      a4,0(a5)
.L10:
        ld      a5,-48(s0)
        addi    a5,a5,8
        sd      a5,-48(s0)
.L4:
        ld      a5,-48(s0)
        addi    a5,a5,16
        ld      a4,-112(s0)
        bgeu    a4,a5,.L11
        ld      a5,-120(s0)
        ld      a4,-32(s0)
        sd      a4,0(a5)
        ld      a5,-128(s0)
        ld      a4,-40(s0)
        sd      a4,0(a5)
        li      a5,0
.L3:
        mv      a0,a5
        ld      ra,120(sp)
        ld      s0,112(sp)
        addi    sp,sp,128
        jr      ra
alu_stuff:
        addi    sp,sp,-80
        sd      ra,72(sp)
        sd      s0,64(sp)
        addi    s0,sp,80
        sd      a0,-72(s0)
        sd      a1,-80(s0)
        ld      a4,-72(s0)
        ld      a5,-80(s0)
        add     a5,a4,a5
        sd      a5,-32(s0)
        ld      a4,-72(s0)
        ld      a5,-80(s0)
        sub     a5,a4,a5
        sd      a5,-40(s0)
        ld      a5,-80(s0)
        beq     a5,zero,.L13
        ld      a4,-72(s0)
        ld      a5,-80(s0)
        divu    a5,a4,a5
        sd      a5,-24(s0)
        j       .L14
.L13:
        ld      a5,-72(s0)
        sd      a5,-24(s0)
.L14:
        ld      a4,-72(s0)
        ld      a5,-80(s0)
        remu    a5,a4,a5
        sd      a5,-48(s0)
        ld      a5,-72(s0)
        slli    a4,a5,1
        ld      a5,-80(s0)
        srli    a5,a5,2
        xor     a5,a4,a5
        sd      a5,-56(s0)
        ld      a4,-32(s0)
        ld      a5,-40(s0)
        add     a4,a4,a5
        ld      a5,-24(s0)
        add     a4,a4,a5
        ld      a5,-48(s0)
        add     a4,a4,a5
        ld      a5,-56(s0)
        add     a5,a4,a5
        mv      a0,a5
        ld      ra,72(sp)
        ld      s0,64(sp)
        addi    sp,sp,80
        jr      ra
