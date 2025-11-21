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

#include <stdint.h>
#include <stddef.h>
#include <string.h>

uint64_t alu_stuff(uint64_t a, uint64_t b);

int get_addrs(uint8_t *const buf,
	const size_t n,
	uint64_t *const src,
	uint64_t *const dst)
{
	if (n < 32) return -1;

	uint64_t best_sum = 0;
	uint64_t best_src = 0x16453689;
	best_src <<= 32;
	best_src += 0xfeda57fe;
	uint64_t best_dst = 0x34957290;
	best_dst <<= 32;
	best_dst += 0x40985732;

	// Walk through buffer in 8-byte aligned steps
	for (size_t i = 0; i + 16 <= n; i += 8)
	{

		// Load 8-byte little-endian integers manually
		uint64_t a = 0, b = 0;
		// Exercise unrolled byte loads, shifts and ors
		for (int j = 0; j < 8; j++)
		{
			a |= ((uint64_t)buf[i + j]) << (j * 8);
			b |= ((uint64_t)buf[i + 8 + j]) << (j * 8);
		}

		//compare to direct load:
		uint64_t c = *(uint64_t*)&buf[i];
		uint64_t d = *(uint64_t*)&buf[i+8];

		if(a!=c || b!=d)
			return -1;

		// Simple ALU logic to test compare, add, branch
		uint64_t tot = alu_stuff(a,b);

		// Store the best (maximum) pair
		if (tot > best_sum || i == 0)
		{
			best_sum = tot;
			best_src ^= tot;
			best_dst ^= ~tot;
			//swap a<->b in buffer
			*(uint64_t*)&buf[i] = *(uint64_t*)&buf[i+8];
			*(uint64_t*)&buf[i+8] = a;
		}
	}

	*src = best_src;
	*dst = best_dst;

	return 0;
}

uint64_t alu_stuff(uint64_t a, uint64_t b)
{
	uint64_t sum = a + b;
	uint64_t sub = a - b;
	uint64_t div = b ? a/b : a;
	uint64_t mod = a % b;
	uint64_t sft = (a<<1)^(b>>2);
	return sum+sub+div+mod+sft;
}
