# **Test Plan for RISC-V RV64IM Virtual Machine Self-Test Program (STP)**

## **1. Objectives**
The purpose of this test plan is to guide the creation of a **Self-Test Program (STP)** in plain RISC-V assembly for validating an RV64IM virtual machine implementation. The STP should:

- Verify correctness of all RV64IM instruction classes.
- Target **boundary conditions and edge cases** rather than brute-force exhaustive testing.
- Produce a **traceable output** via stack operations for later comparison.
- Be **portable** across any RV64IM machine with a modest stack.

---

## **2. Scope**
The STP will cover:

- **Integer Register Operations (RV64I)**:
  - Arithmetic: `ADD`, `SUB`, `ADDI`
  - Logical: `AND`, `OR`, `XOR`
  - Shifts: `SLL`, `SRL`, `SRA`
  - Comparisons: `SLT`, `SLTU`
- **Control Flow**:
  - Branches: `BEQ`, `BNE`, `BLT`, `BGE`
  - Jumps: `JAL`, `JALR`
- **Memory Access**:
  - Loads: `LB`, `LH`, `LW`, `LD`
  - Stores: `SB`, `SH`, `SW`, `SD`
- **Multiplication/Division (M Extension)**:
  - `MUL`, `MULH`, `DIV`, `REM`
- **At least one test for each and every opcode**:
  - many tests for some opcodes and sequences with likely edge case, exceptions or idiosyncracies
  - maybe only one test for simple opcodes just to check they're accepted at all

---

## **3. Common Pitfalls & Edge Cases**
- **Sign Extension Issues**:
  - Immediate values in `ADDI`, `SLTI` vs. register-based operations.
- **Overflow/Underflow**:
  - `ADD` and `SUB` with max/min 64-bit values.
- **Shift Amounts**:
  - `SLL`, `SRL`, `SRA` with shift > 63 bits.
- **Division by Zero**:
  - `DIV` and `REM` behavior.
- **Memory Alignment**:
  - Misaligned loads/stores.
- **Branch Offsets**:
  - Negative offsets and near-boundary jumps.
- **Other cases based on a deep knowledge of the ISA and VM typical VM implementations**
  - special cases with explicit behaviour in the RISC-V ISA
  - Pitfalls of VMs running on various host platforms and implemented in specific languages (especially C++)

### **Additional Boundary Condition Ideas**
- Test **wrap-around behavior** for signed and unsigned arithmetic.
- Use **alternating bit patterns** (e.g., `0xAAAAAAAAAAAAAAAA`, `0x5555555555555555`).
- Validate **sign vs. zero extension** in loads (`LB` vs. `LBU`).
- Loading and storing **test data in the program memory**

---

## **4. Smart Testing Principles**
- **Smarter, Not Harder**:
  - Avoid brute force; focus on **likely bug zones**:
    - Incorrect sign extension in immediate instructions.
    - Mishandled edge cases in division and remainder.
    - Off-by-one errors in branch offsets.
  - Use **knowledge of typical VM implementations**:
    - Many VMs optimize shift operations—test shifts beyond 63 bits.
    - Multiplication high-word (`MULH`) often implemented incorrectly.
    - Memory alignment traps may be skipped—test unaligned addresses.

---

## **5. Structure of STP**
- **Plain Assembly**:
  - No ELF directives; flat bytecode sequence.
  - Assume stack pointer initialized before first instruction.
  - Can embed raw data for test values if required ()
- **Test Blocks**:
  - Each block tests one instruction or related group.
  - Push intermediate and final results onto stack.
- **End of Program**:
  - Last instruction: `EBREAK` to signal completion.
  - Stack dump handled externally by instrumentation.

---

## **6. Comment Format for Expected Results**
Use consistent comments for automated parsing. Each push to the stack shall have a comment block with a description and the expected value. The comments will be parsed and used (along with the block of instructions) to annotate and compare to the actual stack values after the STP is executed. No empty line between comment blocks and associated asm instructions.

```asm
# TEST: Trivial ADD
# CONTEXT: x2 = 5, x3 = 10 (set in previous tests)
# EXPECTED PUSH: 0x000000000000000F
ADD x5, x2, x3
PUSH x5

# TEST: Trivial ADDI
# CONTEXT: x5 = 15
# EXPECTED PUSH: 0x0000000000000010
ADDI x6, x5, 1 # additional comments ok (and encouraged to explain test)
# just no blank lines within the test
PUSH x6
# the entire asm block will be used to annotate the test results
# this is still part of the 'Trivial ADDI test block'

# TEST: Next test
# CONTEXT: blah
# EXPECTED PUSH: 0x0123456789101112
PUSH 0x0123456789101112
```

---

## **7. Comparison Strategy**
For context on how the STP will be used:
- **Run STP on multiple VMs**:
  - Load as raw bytecode into reference VMs (e.g., Spike, QEMU).
- **Compare Outputs**:
  - Extract expected results from comments.
  - Compare actual stack dumps across VMs and against expected values.

---

## **8. Loading & Execution**
For context on how the STP will be used:
- Assemble STP into **flat binary** using a basic assembler, into plain raw bytecode.
- Load raw bytecode directly into VM memory.
- Execute until `EBREAK`.
- External instrumentation dumps stack for analysis.

---

## **9. Key Principles Recap**
- Target **edge cases and likely implementation bugs**.
- Use **equivalence sequences** for self-validation.
- Keep STP **portable and minimal**, yet comprhensive.
- Ensure **traceability** via stack and comments.
