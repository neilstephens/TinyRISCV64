/*
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

#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <vector>
#include <inttypes.h>
#include <fstream>
#include <sstream>

#include "../../TinyElfRISCV64.h"

extern "C"
{
//This is the code that was compiled and assembled into RV64IM.
//include it here so we can run it natively to compare results.

//This was assembled to plain bytecode
#include "stress.c" //See stress.s for the assembly

#define NO_SHA512SUM_MAIN
//This was compiled and linked into a static elf executable for the vm
//	riscv64-unknown-elf-gcc -L. --oslib=TinyElfSysCall -march=rv64im -mabi=lp64 -nostartfiles -static -T vm.ld sha512.c -o sha512sum
#include "sha512.c"
#undef NO_SHA512SUM_MAIN
}

int run_raw(TinyRISCV64::VM& vm, const char* bin_file);
int run_elf(TinyRISCV64::ElfVM& vm, const char* data_file, TinyRISCV64::u64 entry_point);

int main(int argc, char** argv)
{
	if (argc < 2)
	{
		std::fprintf(stderr, "Usage: %s <bin_file> [data_file]\n", argv[0]);
		return 1;
	}
	const char* bin_file = argv[1];

	// Create VM with a modest stack (4 KiB)
	TinyRISCV64::ElfVM vm(4096);
	bool bin_is_elf;
	const char* data_file;
	TinyRISCV64::u64 entry_point;
	try
	{
		entry_point = vm.program_load(bin_file);
		bin_is_elf = true;
		if (argc < 3)
		{
			std::fprintf(stderr, "Error: no data file provided.\n");
			return 1;
		}
		data_file = argv[2];
	}
	catch(const std::exception &e)
	{
		std::fprintf(stderr, "Loading as elf failed: '%s' , assuming raw bytecode.\n", e.what());
		bin_is_elf = false;
	}

	return bin_is_elf ? run_elf(vm,data_file,entry_point) : run_raw(vm,bin_file);
}

int run_elf(TinyRISCV64::ElfVM& vm, const char* data_file, TinyRISCV64::u64 entry_point)
{
	const TinyRISCV64::u64 stdin_fd = 0, stdout_fd = 1, stderr_fd = 2;
	try
	{
		//map input fd
		auto pDataStream = std::make_shared<std::fstream>(data_file,std::ios::in | std::ios::binary);
		if (!pDataStream || pDataStream->fail())
			throw std::invalid_argument("Failed to open data file: " + std::string(data_file));
		vm.map_fd(stdin_fd,pDataStream);

		//map output fd
		auto pOutStream = std::make_shared<std::stringstream>();
		vm.map_fd(stdout_fd,pOutStream);

		//map error fd
		auto pErrStream = std::make_shared<std::stringstream>();
		vm.map_fd(stderr_fd,pErrStream);

		vm.execute_program(entry_point,100UL*1024*1024); //100 million instructions max
		std::string vm_output;
		*pOutStream >> vm_output;

		#if defined(WIN32) || defined(_WIN32) || defined(__WIN32)
		pDataStream->close();
		char sha_hex[129];
		if(!get_sha512_lowercase(data_file, sha_hex, sizeof(sha_hex)))
			throw std::runtime_error("Failed to compute SHA-512 hash of data file natively on host");
		#else
		//reset the file stream
		pDataStream->clear();
		pDataStream->seekg(0, std::ios::beg);

		char buf[1024];
		struct sha512 sha;
		sha512_init(&sha);
		do
		{
			pDataStream->read(buf, sizeof(buf));
			sha512_append(&sha, buf, pDataStream->gcount());
		}while(pDataStream->gcount() > 0);
		char sha_hex[SHA512_HEX_SIZE];
		sha512_finalize_hex(&sha, sha_hex);
		#endif
		const std::string native_output(sha_hex);

		if(vm_output != native_output)
		{
			auto msg = "Program output: '"+vm_output+"' != '"+native_output
			           +"'\n"+"Program StdErr: '"+pErrStream->str()+"'";
			throw std::runtime_error(msg);
		}
	}
	catch (const std::exception &e)
	{
		std::fprintf(stderr, "error: %s\n", e.what());
		return 1;
	}
	std::printf("PASS\n");
	return 0;
}

int run_raw(TinyRISCV64::VM& vm, const char* bin_file)
{
	try
	{
		//create a buffer to process
		constexpr size_t buf_size = 1024;
		std::vector<uint8_t> buf(buf_size);

		// Initialize buffer with deterministic 64-bit LCG-derived bytes
		uint64_t x = 0x0123456789abcdefULL;
		for (size_t i = 0; i < buf_size; ++i)
		{
			x = x * 6364136223846793005ULL + 1ULL;
			buf[i] = static_cast<uint8_t>(x >> 56);
		}

		//copy the buffer for comparison later
		std::vector<uint8_t> native_buf(buf);

		vm.VM::program_load(bin_file);
		auto data_addr_buf = vm.map_data_mem(buf.data(),buf.size());

		//the program implements get_addrs(u8*,sz,u64*,u64*)

		// make room on the stack for the results
		auto stack_addr_src = vm.stack_push<uint64_t>(0);
		auto stack_addr_dst = vm.stack_push<uint64_t>(0);

		//set the fn arg registers
		vm.register_set(10,data_addr_buf);
		vm.register_set(11,buf.size());
		vm.register_set(12,stack_addr_src);
		vm.register_set(13,stack_addr_dst);

		vm.execute_program();

		//Get the results
		auto res = vm.register_get(10);
		auto dst = vm.stack_pop<uint64_t>();
		auto src = vm.stack_pop<uint64_t>();
		std::printf("res = 0x%016" PRIx64 "\n", res);
		std::printf("src = 0x%016" PRIx64 "\n", src);
		std::printf("dst = 0x%016" PRIx64 "\n", dst);

		uint64_t native_src=0, native_dst=0;
		uint64_t native_res = get_addrs(native_buf.data(),native_buf.size(),&native_src,&native_dst);
		std::printf("native_res = 0x%016" PRIx64 "\n", native_res);
		std::printf("native_src = 0x%016" PRIx64 "\n", native_src);
		std::printf("native_dst = 0x%016" PRIx64 "\n", native_dst);

		std::cout<<"Buffer equal : "<<(native_buf==buf)<<std::endl;
		std::cout<<"Result equal : "<<(native_res==res)<<std::endl;
		std::cout<<"Source equal : "<<(native_src==src)<<std::endl;
		std::cout<<"Destin equal : "<<(native_dst==dst)<<std::endl;

		if(native_buf!=buf
		   || native_res!=res
		   || native_src!=src
		   || native_dst!=dst)
			return 1;

	}
	catch (const std::exception &e)
	{
		std::fprintf(stderr, "error: %s\n", e.what());
		return 1;
	}
	return 0;
}

