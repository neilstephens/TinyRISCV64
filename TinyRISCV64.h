/*
 * TinyRISCV64 - RV64IM Virtual Machine
 *
 * https://github.com/neilstephens/TinyRISCV64
 *
 * This is a derivative work based on tinyriscv by inixyz (https://github.com/inixyz/tinyriscv)
 * The core instruction processing logic was ported from C to C++,
 * converted from handling RV32IM to RV64IM, and
 * transplanted into this new class.
 *
 * MIT License
 *
 * Original work Copyright (c) 2023 Alexandru-Florin Ene
 * Modified work Copyright (c) 2025 Neil Stephens
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef TINYRISCV64_H
#define TINYRISCV64_H

#include <cstdint>
#include <vector>
#include <span>
#include <sstream>
#include <stdexcept>
#include <fstream>
#include <cstring>

namespace TinyRISCV64
{

using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using i8 = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;

class VM
{
private:
	u64 pc;                  // Program counter
	std::vector<u8> program; // Program memory
	std::array<u64,32> x;    // Registers x0-x31
	std::vector<u8> stack;   // Stack memory
	std::span<u8> data;      // Data memory
	bool halted;             // Program exited

	//virtual addressing:
	//  0 to memory_size-1 : data memory
	//  memory_size to memory_size+stack_size-1 : stack memory

public:
	VM(const size_t stack_size):
		stack(stack_size)
	{
		for(auto& xn : x) xn=0;
	}

	// Set register value (x0-x31, x0 is always 0)
	void set_register(int reg, u64 value)
	{
		if (reg < 0 || reg >= 32)
			throw std::invalid_argument("Invalid register number");
		if (reg != 0) x[reg] = value;
	}

	// Get register value
	u64 get_register(int reg) const
	{
		if (reg < 0 || reg >= 32)
			throw std::invalid_argument("Invalid register number");
		return x[reg];
	}

	// Set memory for data access
	void set_memory(u8* mem, size_t size)
	{
		data = std::span(mem,size);
	}

	// Load program from file
	void load_program(const std::string& filename)
	{
		std::ifstream fin(filename, std::ios::binary | std::ios::ate);
		if (!fin)
			throw std::invalid_argument("Failed to open program file: " + filename);

		size_t size = fin.tellg();
		if (size > 1024L * 1024)
			throw std::invalid_argument("Program too large (max 1MB)");

		fin.seekg(0, std::ios::beg);
		program.resize(size);
		fin.read(reinterpret_cast<char*>(program.data()), size);
		validate_program();
	}

	// Load program from memory
	void load_program(const void* prog, size_t size)
	{
		if (size > 1024L * 1024)
			throw std::invalid_argument("Program too large (max 1MB)");

		program.resize(size);
		std::memcpy(program.data(), prog, size);
		validate_program();
	}

	// Execute program
	void execute_program(u64 entry_point = 0, size_t max_instructions = 100000)
	{
		pc = entry_point;
		halted = false;
		size_t count = 0;

		if(data.empty())
			throw std::invalid_argument("No memory context set for execution");

		//x2 - stack pointer (sp)
		x[2] = data.size()+stack.size();

		while (!halted && pc+3 < program.size())
		{
			if (++count > max_instructions)
				throw std::runtime_error("Maximum instruction count exceeded");

			const u32 inst = *reinterpret_cast<const u32*>(&program[pc]);
			pc += 4;
			execute_instruction(inst);
		}
	}

private:

	// Validate all the instructions in the program
	void validate_program()
	{
		auto backup_mem = data;
		data = {};

		std::ostringstream err;
		for(pc = 0; pc+3 < program.size(); pc+=4)
		{
			try
			{
				auto tmp_pc = pc;
				const u32 inst = *reinterpret_cast<const u32*>(&program[pc]);
				pc += 4;
				execute_instruction(inst);
				pc = tmp_pc;
			}
			catch (const std::invalid_argument &e)
			{
				err << std::string("VM Exception: ") + e.what() << std::string("\n");
			}
			catch (const std::runtime_error &)
			{}
		}

		data = backup_mem;
		for(auto& xn : x) xn=0;
		//x2 - stack pointer (sp)
		x[2] = data.size()+stack.size();

		auto err_str = err.str();
		if(err_str != "")
			throw std::invalid_argument("Invalid Program: "+err_str);
	}

	void execute_instruction(u32 inst)
	{
		// Decode
		const u8 opcode = inst & 0x7f;
		const u8 funct3 = (inst >> 12) & 0x7;
		const u8 funct7 = (inst >> 25) & 0x7f;
		const u8 rd = (inst >> 7) & 0x1f;
		const u8 rs1 = (inst >> 15) & 0x1f;
		const u8 rs2 = (inst >> 20) & 0x1f;
		const u8 shamt = rs2;

		const i64 imm_i = static_cast<i64>(static_cast<i32>(inst) >> 20);
		const i64 imm_s = (imm_i & ~0x1fLL) | rd;
		const i64 imm_b = ((static_cast<i64>(static_cast<i32>(inst & 0x80000000)) >> 19) |
					 ((inst & 0x80) << 4) | ((inst >> 20) & 0x7e0) | ((inst >> 7) & 0x1e));
		const i64 imm_j = ((static_cast<i64>(static_cast<i32>(inst & 0x80000000)) >> 11) |
					 (inst & 0xff000) | ((inst >> 9) & 0x800) | ((inst >> 20) & 0x7fe));
		const u64 imm_u = inst & 0xfffff000;

		// Execute
		x[0] = 0; // Ensure x0 stays zero

		switch(opcode)
		{
			case 0x37: x[rd] = imm_u; break;               // LUI
			case 0x17: x[rd] = (pc - 4) + imm_u; break;    // AUIPC
			case 0x6f: x[rd] = pc; pc += imm_j - 4; break; // JAL
			case 0x67:                                     // JALR
			{
				const u64 target = (x[rs1] + imm_i) & ~1ULL;
				x[rd] = pc;
				pc = target;
				break;
			}
			case 0x63: exec_branch(funct3, rs1, rs2, imm_b); break;                  // Branch
			case 0x03: exec_load(funct3, rd, rs1, imm_i); break;                     // Load
			case 0x23: exec_store(funct3, rs1, rs2, imm_s); break;                   // Store
			case 0x13: exec_alu_imm(funct3, funct7, rd, rs1, imm_i, shamt); break;   // ALU immediate
			case 0x1b: exec_alu_imm32(funct3, funct7, rd, rs1, imm_i, shamt); break; // ALU immediate 32-bit
			case 0x33: exec_alu_reg(funct3, funct7, rd, rs1, rs2); break;            // ALU register
			case 0x3b: exec_alu_reg32(funct3, funct7, rd, rs1, rs2); break;          // ALU register 32-bit
			case 0x0f: break;                                                        // FENCE (nop)
			case 0x73:                                                               // SYSTEM
				if (inst == 0x00100073) halted = true;                             // EBREAK
				break;
			default: throw std::invalid_argument("Unknown opcode");
		}
	}

	// Memory access helpers
	template<typename T>
	T& mem_ref(u64 addr)
	{
		if (addr + sizeof(T) > data.size()+stack.size())
			throw std::runtime_error("Memory access out of bounds");
		if (addr < data.size() && addr + sizeof(T) > data.size()) //between data and stack
			throw std::runtime_error("Memory access out of bounds");

		return (addr < data.size()) ? *reinterpret_cast<T*>(data.data() + addr)
							: *reinterpret_cast<T*>(stack.data() + addr - data.size());
	}

	template<typename T>
	T mem_load(u64 addr)
	{
		return mem_ref<T>(addr);
	}

	template<typename T>
	void mem_store(u64 addr, T value)
	{
		mem_ref<T>(addr) = value;
	}

	// Instruction execution helpers
	inline void exec_branch(u8 funct3, u8 rs1, u8 rs2, i64 imm)
	{
		bool taken = false;
		switch(funct3)
		{
			case 0: taken = (x[rs1] == x[rs2]); break;                                     // BEQ
			case 1: taken = (x[rs1] != x[rs2]); break;                                     // BNE
			case 4: taken = (static_cast<i64>(x[rs1]) < static_cast<i64>(x[rs2])); break;  // BLT
			case 5: taken = (static_cast<i64>(x[rs1]) >= static_cast<i64>(x[rs2])); break; // BGE
			case 6: taken = (x[rs1] < x[rs2]); break;                                      // BLTU
			case 7: taken = (x[rs1] >= x[rs2]); break;                                     // BGEU
			default: throw std::invalid_argument("Unknown branch operation");
		}
		if (taken) pc += imm - 4;
	}

	inline void exec_load(u8 funct3, u8 rd, u8 rs1, i64 imm)
	{
		const u64 addr = x[rs1] + imm;
		switch(funct3)
		{
			case 0: x[rd] = static_cast<i64>(mem_load<i8>(addr)); break;  // LB
			case 1: x[rd] = static_cast<i64>(mem_load<i16>(addr)); break; // LH
			case 2: x[rd] = static_cast<i64>(mem_load<i32>(addr)); break; // LW
			case 3: x[rd] = mem_load<u64>(addr); break;                   // LD
			case 4: x[rd] = mem_load<u8>(addr); break;                    // LBU
			case 5: x[rd] = mem_load<u16>(addr); break;                   // LHU
			case 6: x[rd] = mem_load<u32>(addr); break;                   // LWU
			default: throw std::invalid_argument("Unknown load operation");
		}
	}

	inline void exec_store(u8 funct3, u8 rs1, u8 rs2, i64 imm)
	{
		const u64 addr = x[rs1] + imm;
		switch(funct3)
		{
			case 0: mem_store<u8>(addr, x[rs2]); break;  // SB
			case 1: mem_store<u16>(addr, x[rs2]); break; // SH
			case 2: mem_store<u32>(addr, x[rs2]); break; // SW
			case 3: mem_store<u64>(addr, x[rs2]); break; // SD
			default: throw std::invalid_argument("Unknown store operation");
		}
	}

	inline void exec_alu_imm(u8 funct3, u8 funct7, u8 rd, u8 rs1, i64 imm, u8 shamt)
	{
		switch(funct3)
		{
			case 0: x[rd] = x[rs1] + imm; break;                   // ADDI
			case 1: x[rd] = x[rs1] << shamt; break;                // SLLI
			case 2: x[rd] = static_cast<i64>(x[rs1]) < imm; break; // SLTI
			case 3: x[rd] = x[rs1] < static_cast<u64>(imm); break; // SLTIU
			case 4: x[rd] = x[rs1] ^ imm; break;                   // XORI
			case 5:
				if (funct7 == 0) x[rd] = x[rs1] >> shamt;                         // SRLI
				else x[rd] = static_cast<u64>(static_cast<i64>(x[rs1]) >> shamt); // SRAI
				break;
			case 6: x[rd] = x[rs1] | imm; break; // ORI
			case 7: x[rd] = x[rs1] & imm; break; // ANDI
			default: throw std::invalid_argument("Unknown alu_imm operation");
		}
	}

	inline void exec_alu_imm32(u8 funct3, u8 funct7, u8 rd, u8 rs1, i32 imm, u8 shamt)
	{
		u32 result;
		switch(funct3)
		{
			case 0: result = static_cast<u32>(x[rs1]) + imm; break;    // ADDIW
			case 1: result = static_cast<u32>(x[rs1]) << shamt; break; // SLLIW
			case 5:
				if (funct7 == 0)
					result = static_cast<u32>(x[rs1]) >> shamt; // SRLIW
				else
					result = static_cast<u32>(static_cast<i32>(x[rs1]) >> shamt); // SRAIW
				break;
			default: throw std::invalid_argument("Unknown alu_imm32 operation");
		}
		x[rd] = static_cast<i64>(static_cast<i32>(result)); // Sign-extend
	}

	inline void exec_alu_reg(u8 funct3, u8 funct7, u8 rd, u8 rs1, u8 rs2)
	{
		const u8 op = funct7 << 3 | funct3;
		switch(op)
		{
			case 0x000: x[rd] = x[rs1] + x[rs2]; break;                                               // ADD
			case 0x100: x[rd] = x[rs1] - x[rs2]; break;                                               // SUB
			case 0x001: x[rd] = x[rs1] << (x[rs2] & 0x3f); break;                                     // SLL
			case 0x002: x[rd] = static_cast<i64>(x[rs1]) < static_cast<i64>(x[rs2]); break;           // SLT
			case 0x003: x[rd] = x[rs1] < x[rs2]; break;                                               // SLTU
			case 0x004: x[rd] = x[rs1] ^ x[rs2]; break;                                               // XOR
			case 0x005: x[rd] = x[rs1] >> (x[rs2] & 0x3f); break;                                     // SRL
			case 0x105: x[rd] = static_cast<u64>(static_cast<i64>(x[rs1]) >> (x[rs2] & 0x3f)); break; // SRA
			case 0x006: x[rd] = x[rs1] | x[rs2]; break;                                               // OR
			case 0x007: x[rd] = x[rs1] & x[rs2]; break;                                               // AND

			// M extension
			case 0x008: x[rd] = x[rs1] * x[rs2]; break; // MUL
			case 0x009: x[rd] = (static_cast<i128>(static_cast<i64>(x[rs1])) *
				               static_cast<i128>(static_cast<i64>(x[rs2]))) >> 64; break; // MULH
			case 0x00a: x[rd] = (static_cast<i128>(static_cast<i64>(x[rs1])) *
				               static_cast<u128>(x[rs2])) >> 64; break; // MULHSU
			case 0x00b: x[rd] = (static_cast<u128>(x[rs1]) *
				               static_cast<u128>(x[rs2])) >> 64; break; // MULHU
			case 0x00c:
				if (x[rs2]) x[rd] = static_cast<u64>(static_cast<i64>(x[rs1]) / static_cast<i64>(x[rs2]));
				else x[rd] = -1ULL;
				break; // DIV
			case 0x00d:
				if (x[rs2]) x[rd] = x[rs1] / x[rs2];
				else x[rd] = -1ULL;
				break; // DIVU
			case 0x00e:
				if (x[rs2]) x[rd] = static_cast<u64>(static_cast<i64>(x[rs1]) % static_cast<i64>(x[rs2]));
				else x[rd] = x[rs1];
				break; // REM
			case 0x00f:
				if (x[rs2]) x[rd] = x[rs1] % x[rs2];
				else x[rd] = x[rs1];
				break; // REMU
			default: throw std::invalid_argument("Unknown alu_reg operation");
		}
	}

	inline void exec_alu_reg32(u8 funct3, u8 funct7, u8 rd, u8 rs1, u8 rs2)
	{
		const u8 op = funct7 << 3 | funct3;
		i32 result;
		u32 a = static_cast<u32>(x[rs1]);
		u32 b = static_cast<u32>(x[rs2]);

		switch(op)
		{
			case 0x000: result = a + b; break;                             // ADDW
			case 0x100: result = a - b; break;                             // SUBW
			case 0x001: result = a << (b & 0x1f); break;                   // SLLW
			case 0x005: result = a >> (b & 0x1f); break;                   // SRLW
			case 0x105: result = static_cast<i32>(a) >> (b & 0x1f); break; // SRAW

			// M extension 32-bit
			case 0x008: result = static_cast<i32>(a * b); break; // MULW
			case 0x00c:
				if (b) result = static_cast<i32>(a) / static_cast<i32>(b);
				else result = -1;
				break; // DIVW
			case 0x00d:
				if (b) result = a / b;
				else result = -1;
				break; // DIVUW
			case 0x00e:
				if (b) result = static_cast<i32>(a) % static_cast<i32>(b);
				else result = static_cast<i32>(a);
				break; // REMW
			case 0x00f:
				if (b) result = a % b;
				else result = static_cast<i32>(a);
				break; // REMUW
			default: throw std::invalid_argument("Unknown alu_reg32 operation");
		}
		x[rd] = static_cast<i64>(result); // Sign-extend to 64 bits
	}

	// For 128-bit multiplication on platforms without __int128
	#if defined(__SIZEOF_INT128__)
	using i128 = __int128;
	using u128 = unsigned __int128;
	#else
	//FIXME: provide emulation below instead of throwing
	struct i128
	{
		operator u64() const { throw std::invalid_argument("missing i128 support on this platform"); }
	};
	struct u128
	{
		operator u64() const { throw std::invalid_argument("missing i128 support on this platform"); }
	};
	inline i128 operator*(i128 a, i128 b)
	{
		throw std::invalid_argument("missing i128 support on this platform");
	}
	inline u128 operator*(u128 a, u128 b)
	{
		throw std::invalid_argument("missing i128 support on this platform");
	}
	inline i128 operator>>(i128 a, int b)
	{
		throw std::invalid_argument("missing i128 support on this platform");
	}
	inline u128 operator>>(u128 a, int b)
	{
		throw std::invalid_argument("missing i128 support on this platform");
	}
	#endif
};

} // namespace TinyRISCV64

#endif // TINYRISCV64_H
