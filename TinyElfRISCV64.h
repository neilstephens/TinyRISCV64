/*
 * TinyRISCV64 extension to add elf loading and syscall ABIs
 *
 * https://github.com/neilstephens/TinyRISCV64
 *
 * MIT License
 *
 * Copyright (c) 2025 Neil Stephens
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

#ifndef TINYELFRISCV64_H
#define TINYELFRISCV64_H

#include "TinyRISCV64.h"

#include <cstdint>
#include <vector>
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

class ElfVM : public VM
{
private:
	// File-descriptor to iostream mapping (populated via map_fd)
	std::unordered_map<u64, std::shared_ptr<std::iostream>> fd_streams;

public:
	ElfVM(const size_t stack_size = 4096, const size_t max_program_size = 1024*1024)
		: VM(stack_size,max_program_size) {}

	// Load program from elf file and return the entry_point addr
	//   resets state and invalidates previous virtual addrs
	u64 program_load(const std::string& prog_filename) override
	{
		auto [prog, entry] = load_elf(prog_filename, max_prog_size);
		program = std::move(prog);
		reset();
		return entry;
	}

	// Map a host iostream to a guest file descriptor number.
	void map_fd(const u64 fd, std::shared_ptr<std::iostream> stream)
	{
		fd_streams[fd] = std::move(stream);
	}

private:

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

	void handle_semihost() override
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

	void handle_ecall() override
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
			throw std::invalid_argument("Not an ELF file (bad magic number)");

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

#endif // TINYELFRISCV64_H
