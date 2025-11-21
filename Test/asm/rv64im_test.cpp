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
#include <deque>
#include <inttypes.h>

#include "../../TinyRISCV64.h"

int main(int argc, char** argv)
{
	try
	{
		// Create VM with a modest stack (4 KiB)
		TinyRISCV64::VM vm(4 * 1024, "rv64im_test.dat");

		//save the stack pointer
		auto sp_before = vm.register_get(2);

		vm.execute_program();

		//dump the stack
		std::deque<uint64_t> deq;
		while (vm.register_get(2) < sp_before)
			deq.push_front(vm.stack_pop<uint64_t>());
		// print format 0xABCDEF0123456789 in CAPS!
		for(const auto& v : deq)
			std::printf("# Result: 0x%016" PRIX64 "\n", v);
	}
	catch (const std::exception &e)
	{
		std::fprintf(stderr, "error: %s\n", e.what());
		return 1;
	}
	return 0;
}

