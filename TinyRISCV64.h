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
#include <stdexcept>
#include <fstream>
#include <cstring>
#include <array>
#include <format>
#include <iostream>
#include <random>
#include <unordered_map>
#include <memory>

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
	u64 pc;                    // Program counter
	std::vector<u8> program;   // Program memory
	std::array<u64,32> x{};    // Registers x0-x31
	std::vector<u8> stack;     // Stack memory
	std::span<u8> data;        // Data memory
	bool halted{false};        // Program exited
	const size_t max_prog_size;// Maximum allowed program image size (bytes)

	// File-descriptor to iostream mapping (populated via map_fd)
	std::unordered_map<u64, std::shared_ptr<std::iostream>> fd_streams;

	// Virtual addressing:
	static constexpr
	u64 p_beg = 0;   // Program mem begin
	u64 p_end;       // Program mem end
							/* 64 overflow detection addresses */
	u64 d_beg;       // Data mem begin
	u64 d_end;       // Data mem end
							/* 64 overflow detection addresses */
	u64 s_beg;       // Stack mem begin
	u64 s_end;       // Stack mem end

public:
	VM(const size_t stack_size = 4096, const size_t max_program_size = 0xFFFFFFFFFFFFFFFFUL)
		: stack(stack_size), max_prog_size(max_program_size) { reset(); }

	// Load program from file and return the virtual start addr or entry_point (elf)
	//   resets state and invalidates previous virtual addrs
	u64 program_load(const std::string& prog_filename, const bool isElf = false)
	{
		if (isElf)
		{
			auto [prog, entry] = load_elf(prog_filename, max_prog_size);
			program = std::move(prog);
			reset();
			return entry;
		}
		program = load_program(prog_filename, max_prog_size);
		reset();
		return p_beg;
	}

	// Copy bytecode and return the starting virtual addr
	//   resets state and invalidates previous virtual addrs
	u64 program_load(const u8* const prog, size_t prog_size)
	{
		if (prog_size > max_prog_size)
			throw std::invalid_argument(std::format("Program too large (max {} bytes)", max_prog_size));
		program.resize(prog_size);
		std::memcpy(program.data(), prog, prog_size);
		reset();
		return p_beg;
	}

	// Map virtual addresses to the referenced data
	//   resets state and invalidates previous virtual addrs
	u64 map_data_mem(u8* const mem, const size_t mem_size)
	{
		data = {mem,mem_size};
		reset();
		return d_beg;
	}

	// Set register value (x0-x31, x0 is always 0)
	void register_set(const size_t reg, const u64 value)
	{
		if (reg < 0 || reg >= 32)
			throw std::invalid_argument("Invalid register number");
		if (reg != 0) x[reg] = value;
	}

	// Get register value
	u64 register_get(const size_t reg) const
	{
		if (reg < 0 || reg >= 32)
			throw std::invalid_argument("Invalid register number");
		return x[reg];
	}

	// Push a value onto the stack and return the virtual address (stack pointer)
	template<typename T>
	u64 stack_push(const T& val)
	{
		x[2] -= sizeof(T);
		mem_store(x[2],val);
		return x[2];
	}

	template<typename T>
	T stack_pop()
	{
		x[2] += sizeof(T);
		return mem_load<T>(x[2]-sizeof(T));
	}

	template<typename T>
	T stack_peek()
	{
		return mem_load<T>(x[2]);
	}

	// Execute program
	void execute_program(const u64 entry_point = p_beg, const size_t max_instructions = 100000)
	{
		pc = entry_point;
		halted = false;
		size_t count = 0;

		if(program.size() < 4)
			throw std::runtime_error("Program too small (must be at least 4 bytes)");

		while (!halted)
		{
			if (pc > program.size()-4)
				throw std::runtime_error("PC jumped program region");
			if (++count > max_instructions)
				throw std::runtime_error("Maximum instruction count exceeded");

			execute_instruction();

			if(pc == program.size())
				halted = true;
		}
	}

	void reset()
	{
		for(auto& xn : x) xn=0;
		//x1 - return address (ra)
		x[1] = program.size();
		//x2 - stack pointer (sp)
		x[2] = program.size()+64+data.size()+64+stack.size();
		//x8 - frame pointer (s0 / fp)
		x[8] = x[2];

		p_end = program.size();
		/* 64 overflow detection addresses */
		d_beg = program.size()+64;
		d_end = program.size()+64+data.size();
		/* 64 overflow detection addresses */
		s_beg = program.size()+64+data.size()+64;
		s_end = program.size()+64+data.size()+64+stack.size();
	}

	// Map a host iostream to a guest file descriptor number.
	// Typical use: map fds 0/1/2 for stdin/stdout/stderr before calling execute_program.
	// The read, write, close, and lseek ecalls are dispatched through this map;
	// all other file-system ecalls (openat, fstat, …) return -ENOSYS.
	void map_fd(const u64 fd, std::shared_ptr<std::iostream> stream)
	{
		fd_streams[fd] = std::move(stream);
	}

private:

	// Load program from file
	static std::vector<u8> load_program(const std::string& filename, const size_t max_size)
	{
		std::ifstream fin(filename, std::ios::binary | std::ios::ate);
		if (!fin)
			throw std::invalid_argument("Failed to open program file: " + filename);

		size_t size = fin.tellg();
		if (size > max_size)
			throw std::invalid_argument(std::format("Program too large (max {})", max_size));

		fin.seekg(0, std::ios::beg);
		std::vector<u8> prog(size);
		fin.read(reinterpret_cast<char*>(prog.data()), size);
		return prog;
	}

	void execute_instruction()
	{
		u32 inst; memcpy(&inst,&program[pc],4);
		pc += 4;

		// Decode
		const u8 opcode = inst & 0x7f;
		const u8 funct3 = (inst >> 12) & 0x7;
		const u8 funct7 = (inst >> 25) & 0x7f;
		const u8 rd = (inst >> 7) & 0x1f;
		const u8 rs1 = (inst >> 15) & 0x1f;
		const u8 rs2 = (inst >> 20) & 0x1f;

		const i64 imm_i = static_cast<i64>(static_cast<i32>(inst) >> 20);
		const i64 imm_s = (imm_i & ~0x1fLL) | rd;
		const i64 imm_b = ((static_cast<i64>(static_cast<i32>(inst & 0x80000000)) >> 19) |
					 ((inst & 0x80) << 4) | ((inst >> 20) & 0x7e0) | ((inst >> 7) & 0x1e));
		const i64 imm_j = ((static_cast<i64>(static_cast<i32>(inst & 0x80000000)) >> 11) |
					 (inst & 0xff000) | ((inst >> 9) & 0x800) | ((inst >> 20) & 0x7fe));
		const u64 imm_u = static_cast<u64>(static_cast<i64>(static_cast<i32>(inst & 0xfffff000)));

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
			case 0x13: exec_alu_imm(funct3, rd, rs1, imm_i); break;                  // ALU immediate
			case 0x1b: exec_alu_imm32(funct3, rd, rs1, imm_i); break;                // ALU immediate 32-bit
			case 0x33: exec_alu_reg(funct3, funct7, rd, rs1, rs2); break;            // ALU register
			case 0x3b: exec_alu_reg32(funct3, funct7, rd, rs1, rs2); break;          // ALU register 32-bit
			case 0x0f: break;                                                        // FENCE (nop)
			case 0x73: exec_system(inst, funct3, funct7, rd, rs1, rs2, imm_i); break;// SYSTEM
			default: throw std::invalid_argument("Unknown opcode");
		}
	}

	// Memory access helpers
	template<typename T>
	u8* mem_ptr(u64 addr)
	{
		if (addr > 0xFFFFFFFFFFFFFFF0ULL) //guard against wrap-around
			throw std::runtime_error("Memory access out of bounds");

		const u64 addr_max = addr + sizeof(T) - 1;

		if(addr_max < p_end)
			return program.data() + addr;
		if(addr >= d_beg && addr_max < d_end)
			return data.data() + addr - d_beg;
		if(addr >= s_beg && addr_max < s_end)
			return stack.data() + addr - s_beg;

		throw std::runtime_error("Memory access out of bounds");
	}

	template<typename T>
	T mem_load(u64 addr)
	{
		T value;
		memcpy(&value, mem_ptr<T>(addr), sizeof(T));
		return value;
	}

	template<typename T>
	void mem_store(u64 addr, T value)
	{
		memcpy(mem_ptr<T>(addr), &value, sizeof(T));
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

	inline void exec_alu_imm(u8 funct3, u8 rd, u8 rs1, i64 imm)
	{
		switch(funct3)
		{
			case 0: x[rd] = x[rs1] + imm; break;                             // ADDI
			case 1: x[rd] = x[rs1] << (static_cast<u64>(imm) & 0x3f); break; // SLLI
			case 2: x[rd] = static_cast<i64>(x[rs1]) < imm; break;           // SLTI
			case 3: x[rd] = x[rs1] < static_cast<u64>(imm); break;           // SLTIU
			case 4: x[rd] = x[rs1] ^ imm; break;                             // XORI
			case 5:
				if (!(imm&0x400)) x[rd] = x[rs1] >> (static_cast<u64>(imm) & 0x3f);    // SRLI
				else x[rd] = static_cast<u64>(static_cast<i64>(x[rs1]) >> (imm&0x3f)); // SRAI
				break;
			case 6: x[rd] = x[rs1] | imm; break; // ORI
			case 7: x[rd] = x[rs1] & imm; break; // ANDI
			default: throw std::invalid_argument("Unknown alu_imm operation");
		}
	}

	inline void exec_alu_imm32(u8 funct3, u8 rd, u8 rs1, i32 imm)
	{
		u32 result;
		switch(funct3)
		{
			case 0: result = static_cast<u32>(x[rs1]) + imm; break;             // ADDIW
			case 1: result = static_cast<u32>(x[rs1]) << (imm & 0x1f); break;   // SLLIW
			case 5:
				if (!(imm&0x400))
					result = static_cast<u32>(x[rs1]) >> (imm & 0x1f);                 // SRLIW
				else
					result = static_cast<u32>(static_cast<i32>(x[rs1]) >> (imm&0x1f)); // SRAIW
				break;
			default: throw std::invalid_argument("Unknown alu_imm32 operation");
		}
		x[rd] = static_cast<i64>(static_cast<i32>(result)); // Sign-extend
	}

	inline void exec_alu_reg(u8 funct3, u8 funct7, u8 rd, u8 rs1, u8 rs2)
	{
		const auto op = funct7 << 3 | funct3;
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
			case 0x008: x[rd] = x[rs1] * x[rs2]; break;                                               // MUL
			case 0x009: x[rd] = mulh(x[rs1],x[rs2]); break;                                           // MULH
			case 0x00a: x[rd] = mulhsu(x[rs1],x[rs2]); break;                                         // MULHSU
			case 0x00b: x[rd] = mulhu(x[rs1],x[rs2]); break;                                          // MULHU
			case 0x00c: // DIV
				if (x[rs2]) x[rd] = (static_cast<i64>(x[rs1]) == INT64_MIN && static_cast<i64>(x[rs2]) == -1)
							  ? static_cast<u64>(INT64_MIN)
							  : static_cast<u64>(static_cast<i64>(x[rs1]) / static_cast<i64>(x[rs2]));
				else x[rd] = 0xFFFFFFFFFFFFFFFFULL;
				break;
			case 0x00d: // DIVU
				if (x[rs2]) x[rd] = x[rs1] / x[rs2];
				else x[rd] = 0xFFFFFFFFFFFFFFFFULL;
				break;
			case 0x00e: // REM
				if (x[rs2]) x[rd] = (static_cast<i64>(x[rs1]) == INT64_MIN && static_cast<i64>(x[rs2]) == -1)
							  ? 0ULL
							  : static_cast<u64>(static_cast<i64>(x[rs1]) % static_cast<i64>(x[rs2]));
				else x[rd] = x[rs1];
				break;
			case 0x00f: // REMU
				if (x[rs2]) x[rd] = x[rs1] % x[rs2];
				else x[rd] = x[rs1];
				break;
			default: throw std::invalid_argument("Unknown alu_reg operation");
		}
	}

	inline void exec_alu_reg32(u8 funct3, u8 funct7, u8 rd, u8 rs1, u8 rs2)
	{
		const auto op = funct7 << 3 | funct3;
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
				if (b) result = (static_cast<i32>(a) == INT32_MIN && static_cast<i32>(b) == -1)
						    ? static_cast<i32>(INT32_MIN)
						    : static_cast<i32>(a) / static_cast<i32>(b);
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

	// For 128-bit multiplication - TODO: use platform intrinsics (_umul128 on MSVC and __int128 specifically for GCC/Clang)
	#if defined(__SIZEOF_INT128__)
	using i128 = __int128; using u128 = unsigned __int128;
	static inline uint64_t mulh(i64 a, i64 b) { return (static_cast<i128>(a) * static_cast<i128>(b)) >> 64; }
	static inline uint64_t mulhu(u64 a, u64 b) { return (static_cast<u128>(a) * static_cast<u128>(b)) >> 64; }
	static inline uint64_t mulhsu(i64 a, u64 b) { return (static_cast<i128>(a) * static_cast<u128>(b)) >> 64; }
	#else
	//Provide emulation on platforms without __int128
	static inline std::pair<u64,u64> mulu64_128(u64 a, u64 b)
	{
		constexpr u64 TRUNC32 = 0xFFFFFFFFULL;
		const u64 a_lo = a & TRUNC32;
		const u64 a_hi = a >> 32;
		const u64 b_lo = b & TRUNC32;
		const u64 b_hi = b >> 32;

		u64 p0 = a_lo * b_lo;
		u64 p1 = a_lo * b_hi;
		u64 p2 = a_hi * b_lo;
		u64 p3 = a_hi * b_hi;

		u64 mid = (p0 >> 32) + (p1 & TRUNC32) + (p2 & TRUNC32);
		u64 lo = (p0 & TRUNC32) | (mid << 32);
		u64 hi = p3 + (p1 >> 32) + (p2 >> 32) + (mid >> 32);

		return {hi, lo};
	}
	static inline i64 mulh(i64 a, i64 b)
	{
		bool neg = (a < 0) ^ (b < 0);
		u64 abs_a = (a < 0) ? (0ULL - static_cast<u64>(a)) : static_cast<u64>(a);
		u64 abs_b = (b < 0) ? (0ULL - static_cast<u64>(b)) : static_cast<u64>(b);

		auto [hi, lo] = mulu64_128(abs_a, abs_b);

		if (neg) //2's compliment
		{
			hi = ~hi;
			lo = ~lo;
			lo += 1;
			if (lo == 0) ++hi;
		}
		return static_cast<i64>(hi);
	}
	static inline i64 mulhsu(i64 a, u64 b)
	{
		if(a < 0)
		{
			u64 abs_a = 0ULL - static_cast<u64>(a);

			auto [hi, lo] = mulu64_128(abs_a, b);

			//2's compliment
			hi = ~hi;
			lo = ~lo;
			lo += 1;
			if (lo == 0) ++hi;

			return static_cast<i64>(hi);
		}
		return mulu64_128(static_cast<u64>(a),b).first;
	}
	static inline uint64_t mulhu(u64 a, u64 b){ return mulu64_128(a,b).first; }
	#endif

	// SYSTEM instruction dispatch (opcode 0x73)
	void exec_system(u32 inst, u8 funct3, u8 funct7, u8 rd, u8 rs1, u8 rs2, i32 imm)
	{
		if (funct3 != 0)
		{
			handle_csr(inst,funct3,funct7,rd,rs1,rs2,imm);
			return;
		}

		switch (inst)
		{
			case 0x00000073: // ECALL
				handle_ecall();
				return;

			case 0x00100073: // EBREAK
			{
				// Check for the semihosting magic bracket:
				//   slli zero,zero,0x1f  (0x01f01013)  <-- instruction before ebreak
				//   ebreak
				//   srai zero,zero,0x7   (0x40705013)  <-- instruction after ebreak
				// pc is already advanced past the ebreak at this point.
				const bool has_prev = (pc >= 8) && mem_load<u32>(pc - 8) == 0x01f01013u;
				const bool has_next = (pc + 3 < p_end) && mem_load<u32>(pc) == 0x40705013u;
				if (has_prev && has_next)
					handle_semihost();
				else
					halted = true;
				return;
			}

			case 0x10500073: // WFI  (wait for interrupt)
				throw std::runtime_error(
				    "WFI (wait-for-interrupt) is not supported in this VM; "
				    "remove interrupt-driven idle loops from bare-metal code");

			case 0x30200073: // MRET
			case 0x10200073: // SRET
			case 0x00200073: // URET
				throw std::runtime_error(
				    std::format("Privilege-mode return instruction (MRET/SRET/URET, inst=0x{:x}) at pc=0x{:x}: this VM has no privilege levels",
				    inst, pc - 4));

			default:
				throw std::invalid_argument(
				    std::format("Unknown SYSTEM instruction 0x{:x} at pc=0x{:x}",
				    inst, pc - 4));
		}
	}

	// CSR instructions (CSRRW, CSRRS, CSRRC, CSRRWI, CSRRSI, CSRRCI)
	void handle_csr(u32 inst, u8 funct3, u8 funct7, u8 rd, u8 rs1, u8 rs2, i32 imm)
	{
		// Stub: All CSRs read as 0; write side-effects are ignored
		if (rd != 0)
			x[rd] = 0;
	}

	// Read a null-terminated string from guest memory
	std::string mem_read_str(u64 addr)
	{
	    std::string s;
	    for (;;)
	    {
		  const char c = static_cast<char>(mem_load<u8>(addr++));
		  if (c == '\0') break;
		  s += c;
	    }
	    return s;
	}

	void handle_semihost()
	{
	    const u64 op  = x[10]; // a0 — semihosting operation
	    const u64 arg = x[11]; // a1 — pointer to argument block in guest memory

	    // Helper: read word N from the argument block
	    auto argv = [&](size_t n) { return mem_load<u64>(arg + n * 8); };

	    switch (op)
	    {
		  case 0x01: // SYS_OPEN(path_ptr, mode, path_len)
		  {
			const std::string path = mem_read_str(argv(0));
			const u64 mode = argv(1);

			// Special names map to the pre-existing stdio fds
			if (path == ":tt")
			{
				if(mode < 4)
					x[10] = 0; //stdin
				else if(mode < 8)
					x[10] = 1; //stdout
				else if(mode < 12)
					x[10] = 2; //stderr
				else
					x[10] = static_cast<u64>(-1LL);
			    return;
			}

			//No support for real files
			x[10] = static_cast<u64>(-1LL);
			return;
		  }

		  case 0x02: // SYS_CLOSE(fd)
		  {
			const u64 fd = argv(0);
			fd_streams.erase(fd);
			x[10] = 0;
			return;
		  }

		  case 0x03: // SYS_WRITEC(char_ptr) — write one character to stdout
		  {
			const char c = static_cast<char>(mem_load<u8>(arg));
			auto it = fd_streams.find(1);
			if (it != fd_streams.end())
			    it->second->put(c);
			x[10] = 0;
			return;
		  }

		  case 0x04: // SYS_WRITE0(str_ptr) — write null-terminated string to stdout
		  {
			const std::string s = mem_read_str(arg);
			auto it = fd_streams.find(1);
			if (it != fd_streams.end())
			    *it->second << s;
			x[10] = 0;
			return;
		  }

		  case 0x05: // SYS_WRITE(fd, buf_ptr, len) — returns bytes NOT written (0 = success)
		  {
			const u64 fd  = argv(0);
			const u64 buf = argv(1);
			u64 len = argv(2);

			auto it = fd_streams.find(fd);
			if (it == fd_streams.end())
			{
				x[10] = len; // nothing written
				return;
			}

			size_t i = 0;
			while(len > 0 && it->second->good())
			{
			    it->second->put(static_cast<char>(mem_load<u8>(buf + i++)));
			    len = it->second->good() ? len-1 : len;
			}

			x[10] = len;
			return;
		  }

		  case 0x06: // SYS_READ(fd, buf_ptr, len) — returns bytes NOT read (0 = success, len = EOF)
		  {
			const u64 fd  = argv(0);
			const u64 buf = argv(1);
			const u64 len = argv(2);

			auto it = fd_streams.find(fd);
			if (it == fd_streams.end())
			{
				x[10] = len;
				return;
			}

			std::vector<char> tmp(len);
			it->second->read(tmp.data(), static_cast<std::streamsize>(len));
			const u64 n = static_cast<u64>(it->second->gcount());
			for (u64 i = 0; i < n; ++i)
			    mem_store<u8>(buf + i, static_cast<u8>(tmp[i]));

			x[10] = len - n; // bytes NOT read
			return;
		  }

		  case 0x07: // SYS_READC — read one character from stdin
		  {
			auto it = fd_streams.find(0);
			if (it != fd_streams.end())
			{
				auto c = it->second->get();
				if (!it->second->eof() && !it->second->fail())
				{
					x[10] = static_cast<u64>(c);
					return;
				}
			}
			//Failure isn't an option in the spec; make something up
			x[10] = 0xFFFFFFFFFFFFFF04UL; //-1LL & ASCII_EOT
			return;
		  }

		  case 0x09: // SYS_ISTTY(fd)
		  {
			const u64 fd = argv(0);
			x[10] = (fd <= 2) ? 1 : 0;
			return;
		  }

		  case 0x0a: // SYS_SEEK(fd, offset)
		  {
			const u64 fd  = argv(0);
			const i64 off = static_cast<i64>(argv(1));
			auto it = fd_streams.find(fd);
			if (it == fd_streams.end()) { x[10] = static_cast<u64>(-1LL); return; }
			it->second->seekg(off, std::ios_base::beg);
			it->second->seekp(off, std::ios_base::beg);
			x[10] = it->second->good() ? 0 : static_cast<u64>(-1LL);
			return;
		  }

		  case 0x0c: // SYS_FLEN(fd)
		  {
			const u64 fd = argv(0);
			auto it = fd_streams.find(fd);
			if (it == fd_streams.end()) { x[10] = static_cast<u64>(-1LL); return; }
			auto& s = *it->second;
			const std::streampos cur = s.tellg();
			s.seekg(0, std::ios_base::end);
			const u64 len = static_cast<u64>(s.tellg());
			s.seekg(cur, std::ios_base::beg);
			x[10] = len;
			return;
		  }

		  case 0x11: // SYS_TIME — seconds since epoch
		  {
			x[10] = static_cast<u64>(std::time(nullptr));
			return;
		  }

		  case 0x18: // SYS_EXIT
		  {
			// arg is ADP_Stopped_ApplicationExit (0x20026) for normal exit,
			// or a struct pointer for extended info — treat as halt either way
			halted = true;
			x[10] = 0;
			return;
		  }

		  case 0x30: // SYS_EXIT_EXTENDED(reason, exit_code)
		  {
			halted = true;
			x[10] = argv(1); // propagate exit code
			return;
		  }

		  default:
			throw std::runtime_error(
			    std::format("Unsupported semihosting operation 0x{:x} at pc=0x{:x}",
			    op, pc - 4));
	    }
	}

	// ECALL (syscall) stubs
	void handle_ecall()
	{
		const u64 num = x[17]; // a7 — syscall number
		const u64 a0  = x[10];
		const u64 a1  = x[11];
		const u64 a2  = x[12];

		switch (num)
		{
			// ----- process lifecycle ------------------------------------------
			case 93:  // exit(status)
			case 94:  // exit_group(status)
				// Halt cleanly; caller can inspect x[10] for the exit code.
				halted = true;
				return;

			// ----- memory management / MMU ------------------------------------
			case 9:   // mmap
			case 215: // munmap
			case 222: // mmap2
			case 214: // brk
			case 226: // mprotect
			case 233: // madvise
			case 216: // remap_file_pages
			case 219: // mlock
			case 228: // mbind
			// MMU / paging operations are not supported; return -ENOSYS so the
			// caller can handle the failure rather than crashing the VM
			x[10] = static_cast<u64>(-38LL); // -ENOSYS
			return;

			// ----- file system / I/O ------------------------------------------
			case 57:  // close(fd)
			{
				fd_streams.erase(a0);
				x[10] = 0;
				return;
			}
			case 62:  // lseek(fd, offset, whence)
			{
				auto it = fd_streams.find(a0);
				if (it == fd_streams.end()) { x[10] = static_cast<u64>(-9LL); return; } // -EBADF
				const std::ios_base::seekdir dirs[] = {std::ios_base::beg, std::ios_base::cur, std::ios_base::end};
				if (a2 > 2) { x[10] = static_cast<u64>(-22LL); return; } // -EINVAL
				auto& s = *it->second;
				s.seekg(static_cast<std::streamoff>(static_cast<i64>(a1)), dirs[a2]);
				s.seekp(static_cast<std::streamoff>(static_cast<i64>(a1)), dirs[a2]);
				x[10] = s ? static_cast<u64>(s.tellg()) : static_cast<u64>(-29LL); // -ESPIPE
				return;
			}
			case 63:  // read(fd, buf, count)
			{
				auto it = fd_streams.find(a0);
				if (it == fd_streams.end()) { x[10] = static_cast<u64>(-9LL); return; } // -EBADF
				std::vector<char> buf(a2);
				it->second->read(buf.data(), static_cast<std::streamsize>(a2));
				const u64 n = static_cast<u64>(it->second->gcount());
				for (u64 i = 0; i < n; ++i)
					mem_store<u8>(a1 + i, static_cast<u8>(buf[i]));
				x[10] = n;
				return;
			}
			case 64:  // write(fd, buf, count)
			{
				auto it = fd_streams.find(a0);
				if (it == fd_streams.end()) { x[10] = static_cast<u64>(-9LL); return; } // -EBADF
				std::vector<char> buf(a2);
				for (u64 i = 0; i < a2; ++i)
					buf[i] = static_cast<char>(mem_load<u8>(a1 + i));
				it->second->write(buf.data(), static_cast<std::streamsize>(a2));
				x[10] = it->second->good() ? a2 : static_cast<u64>(-5LL); // -EIO
				return;
			}
			case 56:  // openat — requires a virtual filesystem; not supported
			case 65:  // readv
			case 66:  // writev
			case 79:  // fstatat
			case 80:  // fstat
				x[10] = static_cast<u64>(-38LL); // -ENOSYS
				return;

			case 160: // uname(buf)
				x[10] = static_cast<u64>(-38LL); // -ENOSYS — no OS identity on 'bare metal'
				return;

			case 278: // getrandom(buf, count, flags)
			{
				// flags (a2) are ignored — always behaves as a blocking, non-random-pool read
				std::random_device rng;
				const u64 count = a1;
				for (u64 i = 0; i < count; ++i)
					mem_store<u8>(a0 + i, static_cast<u8>(rng()));
				x[10] = count; // return number of bytes written
				return;
			}

			// ----- time -------------------------------------------------------
			case 113: // clock_gettime
			case 169: // gettimeofday
				x[10] = static_cast<u64>(-22);// -EINVAL;
				return;

			case 174: // getuid
			case 175: // geteuid
			case 176: // getgid
			case 177: // getegid
				x[10] = 0; // report as root on bare metal
				return;

			case 96:  // set_tid_address
				x[10] = 1; // return a fake TID
				return;

			case 99:  // set_robust_list
			case 100: // get_robust_list
			case 261: // prlimit64
			case 132: // sigaltstack
			case 134: // rt_sigaction
			case 135: // rt_sigprocmask
				x[10] = 0; // NOP — return success
				return;

			case 220: // clone
			case 221: // execve
				x[10] = static_cast<u64>(-38LL); // -ENOSYS — multi-process not supported
				return;

			// ----- catch-all --------------------------------------------------
			default:
				throw std::runtime_error(
				    std::format("ecall: unsupported syscall number {} (a0={}, a1=0x{:x}, a2={}) at pc=0x{:x}",
				    num, a0, a1, a2, pc - 4));
		}
	}

	static std::pair<std::vector<u8>, u64> load_elf(const std::string& filename, const size_t max_size)
	{
		// ----- read entire file ------------------------------------------------
		std::ifstream fin(filename, std::ios::binary | std::ios::ate);
		if (!fin)
			throw std::invalid_argument(std::format("Failed to open ELF file: {}", filename));

		const size_t file_size = static_cast<size_t>(fin.tellg());
		if (file_size < sizeof(Elf64Ehdr))
			throw std::invalid_argument(std::format("File too small to be a valid ELF64 binary: {}", filename));

		fin.seekg(0, std::ios::beg);
		std::vector<u8> file_data(file_size);
		fin.read(reinterpret_cast<char*>(file_data.data()), file_size);

		// ----- parse and validate ELF header -----------------------------------
		Elf64Ehdr ehdr;
		std::memcpy(&ehdr, file_data.data(), sizeof(Elf64Ehdr));

		// Magic number
		if (ehdr.e_ident[0] != 0x7f || ehdr.e_ident[1] != 'E' ||
		    ehdr.e_ident[2] != 'L'  || ehdr.e_ident[3] != 'F')
			throw std::invalid_argument("Not an ELF file (bad magic number) — Use isElf=false for flat bytecode.");

		// 64-bit class
		if (ehdr.e_ident[4] != 2 /*ELFCLASS64*/)
			throw std::invalid_argument(
			    std::format("ELF is {}; recompile with riscv64-unknown-elf-gcc (EI_CLASS={})",
			    (ehdr.e_ident[4] == 1 ? "32-bit" : "unknown class"), ehdr.e_ident[4]));

		// Little-endian
		if (ehdr.e_ident[5] != 1 /*ELFDATA2LSB*/)
			throw std::invalid_argument(
			    std::format("ELF is big-endian; ensure the target triple is riscv64 (EI_DATA={})",
			    ehdr.e_ident[5]));

		// ELF version
		if (ehdr.e_ident[6] != 1 /*EV_CURRENT*/)
			throw std::invalid_argument(
			    std::format("Unknown ELF version (EI_VERSION={})", ehdr.e_ident[6]));

		// RISC-V architecture
		if (ehdr.e_machine != 0xF3 /*EM_RISCV*/)
			throw std::invalid_argument(
			    std::format("Not a RISC-V ELF (e_machine=0x{:x}); recompile targeting riscv64 (e.g. riscv64-unknown-elf-gcc)",
			    ehdr.e_machine));

		// Executable type
		if (ehdr.e_type == 3 /*ET_DYN*/)
			throw std::invalid_argument(
			    "ELF is a shared object / position-independent executable; "
			    "relink as a static executable with -static -no-pie");
		if (ehdr.e_type != 2 /*ET_EXEC*/)
			throw std::invalid_argument(
			    std::format("ELF is not an executable (e_type={}); expected ET_EXEC (2)",
			    ehdr.e_type));

		// RISC-V ISA / ABI flags
		constexpr u32 EF_RISCV_RVC            = 0x0001;
		constexpr u32 EF_RISCV_FLOAT_ABI_MASK = 0x0006;
		constexpr u32 EF_RISCV_RVE            = 0x0008;

		if (ehdr.e_flags & EF_RISCV_FLOAT_ABI_MASK)
			throw std::invalid_argument(
			    std::format("ELF uses a hardware floating-point ABI (e_flags=0x{:x}); "
			    "this VM implements RV64IM (integer only). "
			    "Recompile with -march=rv64im -mabi=lp64", ehdr.e_flags));

		if (ehdr.e_flags & EF_RISCV_RVC)
			throw std::invalid_argument(
			    std::format("ELF contains RISC-V Compressed (C) extension instructions "
			    "(EF_RISCV_RVC set in e_flags=0x{:x}); "
			    "this VM only handles 32-bit instructions. "
			    "Recompile with -march=rv64im (omit 'c' from the march string) or add -mno-rvc",
			    ehdr.e_flags));

		if (ehdr.e_flags & EF_RISCV_RVE)
			throw std::invalid_argument(
			    std::format("ELF uses the RV32E reduced (16-register) integer ABI "
			    "(EF_RISCV_RVE in e_flags=0x{:x}); "
			    "recompile targeting riscv64", ehdr.e_flags));

		// Program header table
		if (ehdr.e_phoff == 0 || ehdr.e_phnum == 0)
			throw std::invalid_argument(
			    "ELF has no program headers; link as a static executable, "
			    "not a relocatable object (.o)");

		if (ehdr.e_phentsize < sizeof(Elf64Phdr))
			throw std::invalid_argument(
			    std::format("ELF program header entry size too small (e_phentsize={}; expected >= {})",
			    ehdr.e_phentsize, sizeof(Elf64Phdr)));

		const u64 phtab_end = ehdr.e_phoff + static_cast<u64>(ehdr.e_phnum) * ehdr.e_phentsize;
		if (phtab_end > file_size)
			throw std::invalid_argument("ELF program header table extends beyond end of file");

		// ----- first pass: validate segments and compute address span ----------
		bool has_interp  = false;
		bool has_dynamic = false;
		u64  vaddr_min   = ~u64(0);
		u64  vaddr_max   = 0;

		for (u16 i = 0; i < ehdr.e_phnum; ++i)
		{
			Elf64Phdr phdr;
			std::memcpy(&phdr,
					file_data.data() + ehdr.e_phoff + i * ehdr.e_phentsize,
					sizeof(Elf64Phdr));

			switch (phdr.p_type)
			{
				case 3: /*PT_INTERP*/  has_interp  = true; break;
				case 2: /*PT_DYNAMIC*/ has_dynamic = true; break;
				case 1: /*PT_LOAD*/
				{
					if (phdr.p_filesz > phdr.p_memsz)
						throw std::invalid_argument(
						    std::format("ELF PT_LOAD segment[{}]: p_filesz ({}) > p_memsz ({}) — malformed ELF",
						    i, phdr.p_filesz, phdr.p_memsz));

					if (phdr.p_offset + phdr.p_filesz > file_size)
						throw std::invalid_argument(
						    std::format("ELF PT_LOAD segment[{}] file data extends beyond end of file", i));

					vaddr_min = std::min(vaddr_min, phdr.p_vaddr);
					vaddr_max = std::max(vaddr_max, phdr.p_vaddr + phdr.p_memsz);
					break;
				}
				default: break;
			}
		}

		// Dynamic-linker / shared-library checks
		if (has_interp)
			throw std::invalid_argument(
			    "ELF requires a dynamic linker (PT_INTERP segment present); "
			    "recompile and link with -static");

		if (has_dynamic)
			throw std::invalid_argument(
			    "ELF contains dynamic linking information (PT_DYNAMIC segment present); "
			    "recompile and link with -static");

		if (vaddr_min == ~u64(0))
			throw std::invalid_argument(
			    "ELF has no loadable (PT_LOAD) segments — nothing to execute");

		// Size check
		if (vaddr_max > max_size)
			throw std::invalid_argument(
			    std::format("ELF virtual address span [0x{:x}, 0x{:x}) requires {} bytes which exceeds max_program_size={}; "
			    "construct the VM with a larger max_program_size",
			    vaddr_min, vaddr_max, vaddr_max, max_size));

		if (ehdr.e_entry < vaddr_min || ehdr.e_entry >= vaddr_max)
			throw std::invalid_argument(
			    std::format("ELF entry point 0x{:x} lies outside the loaded virtual address range [0x{:x}, 0x{:x}); "
			    "the binary may not have been linked correctly",
			    ehdr.e_entry, vaddr_min, vaddr_max));

		// ----- second pass: populate program image ----------------------------
		// Allocate zeroed image covering [0, vaddr_max).
		// Zero-initialisation takes care of the .bss region (p_memsz > p_filesz).
		std::vector<u8> prog(vaddr_max, 0);

		for (u16 i = 0; i < ehdr.e_phnum; ++i)
		{
			Elf64Phdr phdr;
			std::memcpy(&phdr,
					file_data.data() + ehdr.e_phoff + i * ehdr.e_phentsize,
					sizeof(Elf64Phdr));

			if (phdr.p_type != 1 /*PT_LOAD*/ || phdr.p_filesz == 0)
				continue;

			std::memcpy(prog.data() + phdr.p_vaddr,
					file_data.data() + phdr.p_offset,
					phdr.p_filesz);
		}

		return {std::move(prog), ehdr.e_entry};
	}

	struct Elf64Ehdr
	{
		u8  e_ident[16]; // Magic, class, data, version, OS/ABI, padding
		u16 e_type;      // Object type  (ET_EXEC=2, ET_DYN=3)
		u16 e_machine;   // Architecture (EM_RISCV=0xF3)
		u32 e_version;   // ELF version  (EV_CURRENT=1)
		u64 e_entry;     // Entry point virtual address
		u64 e_phoff;     // Program header table offset
		u64 e_shoff;     // Section header table offset
		u32 e_flags;     // Processor-specific flags
		u16 e_ehsize;    // ELF header size (bytes)
		u16 e_phentsize; // Program header entry size
		u16 e_phnum;     // Number of program header entries
		u16 e_shentsize; // Section header entry size
		u16 e_shnum;     // Number of section header entries
		u16 e_shstrndx;  // Section name string table index
	};
	static_assert(sizeof(Elf64Ehdr) == 64, "Elf64Ehdr must be 64 bytes");

	struct Elf64Phdr
	{
		u32 p_type;   // Segment type   (PT_LOAD=1, PT_DYNAMIC=2, PT_INTERP=3)
		u32 p_flags;  // Segment flags  (PF_X=1, PF_W=2, PF_R=4)
		u64 p_offset; // Offset in file
		u64 p_vaddr;  // Virtual address in memory
		u64 p_paddr;  // Physical address (unused here)
		u64 p_filesz; // Bytes present in the file image
		u64 p_memsz;  // Bytes required in memory (>= p_filesz; excess is .bss)
		u64 p_align;  // Alignment (must be power of two)
	};
	static_assert(sizeof(Elf64Phdr) == 56, "Elf64Phdr must be 56 bytes");
};

} // namespace TinyRISCV64

#endif // TINYRISCV64_H
