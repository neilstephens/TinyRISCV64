/*
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

/*
 * This example code shows how the ecall ABI in TinyElfRISCV64.h can be used.
 * This code was writen for use with picolibc - it implements the set of OS functions that picolibc needs to compile ISO C code
 *   see: https://github.com/picolibc/picolibc/blob/main/doc/os.md
 *
 * a pre-compiled static library of this code is provided, so you can compile like:
 *    Examples$ riscv64-unknown-elf-gcc -L. --oslib=TinyElfSysCall -march=rv64im -mabi=lp64 -nostartfiles -static -T vm.ld -O3 YourCode.c -o YourBin
 */

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/times.h>
#include <sys/time.h>

/* Using Linux RISC-V64 syscall numbers */
#define SYS_unlinkat        35
#define SYS_openat          56
#define SYS_close           57
#define SYS_lseek           62
#define SYS_read            63
#define SYS_write           64
#define SYS_fstatat         79
#define SYS_fstat           80
#define SYS_exit            93
#define SYS_times           153
#define SYS_gettimeofday    169
#define SYS_rt_sigprocmask  135
#define SYS_getrandom       278

/* Core ecall helper — all 6 argument slots, unused ones pass 0 */
static inline long __syscall(long n,
                             long a0, long a1, long a2,
                             long a3, long a4, long a5)
{
    register long _n  asm("a7") = n;
    register long _a0 asm("a0") = a0;
    register long _a1 asm("a1") = a1;
    register long _a2 asm("a2") = a2;
    register long _a3 asm("a3") = a3;
    register long _a4 asm("a4") = a4;
    register long _a5 asm("a5") = a5;
    asm volatile ("ecall"
        : "+r"(_a0)
        : "r"(_n), "r"(_a1), "r"(_a2), "r"(_a3), "r"(_a4), "r"(_a5)
        : "memory");
    return _a0;
}

/* Converts a negative kernel return value into errno + -1 */
static inline int __check(long r)
{
    if (r < 0) { errno = (int)-r; return -1; }
    return (int)r;
}

void _exit(int status)
{
    __syscall(SYS_exit, status, 0, 0, 0, 0, 0);
    __builtin_unreachable();
}

int open(const char *path, int flags, ...)
{
    /* Extract mode from varargs only when O_CREAT is set */
    unsigned mode = 0;
    if (flags & O_CREAT)
    {
        __builtin_va_list ap;
        __builtin_va_start(ap, flags);
        mode = __builtin_va_arg(ap, unsigned int);
        __builtin_va_end(ap);
    }
    return __check(__syscall(SYS_openat, AT_FDCWD, (long)path, flags, mode, 0, 0));
}

int close(int fd)
{
    return __check(__syscall(SYS_close, fd, 0, 0, 0, 0, 0));
}

ssize_t read(int fd, void *buf, size_t count)
{
    return (ssize_t)__check(__syscall(SYS_read, fd, (long)buf, (long)count, 0, 0, 0));
}

ssize_t write(int fd, const void *buf, size_t count)
{
    return (ssize_t)__check(__syscall(SYS_write, fd, (long)buf, (long)count, 0, 0, 0));
}

off_t lseek(int fd, off_t offset, int whence)
{
    return (off_t)__check(__syscall(SYS_lseek, fd, (long)offset, whence, 0, 0, 0));
}

int unlink(const char *path)
{
    return __check(__syscall(SYS_unlinkat, AT_FDCWD, (long)path, 0, 0, 0, 0));
}

int fstat(int fd, struct stat *buf)
{
    return __check(__syscall(SYS_fstat, fd, (long)buf, 0, 0, 0, 0));
}

int stat(const char *path, struct stat *buf)
{
    /* flags = 0: follow symlinks, matching classic stat() behaviour */
    return __check(__syscall(SYS_fstatat, AT_FDCWD, (long)path, (long)buf, 0, 0, 0));
}

int gettimeofday(struct timeval *tv, void *tz)
{
    return __check(__syscall(SYS_gettimeofday, (long)tv, (long)tz, 0, 0, 0, 0));
}

clock_t times(struct tms *buf)
{
    long r = __syscall(SYS_times, (long)buf, 0, 0, 0, 0, 0);
    if (r < 0) { errno = (int)-r; return (clock_t)-1; }
    return (clock_t)r;
}

int sigprocmask(int how, const sigset_t *set, sigset_t *oldset)
{
    /* rt_sigprocmask requires the sigset size as a 4th argument */
    return __check(__syscall(SYS_rt_sigprocmask,
                             how, (long)set, (long)oldset,
                             sizeof(sigset_t), 0, 0));
}

int getentropy(void *buf, size_t len)
{
    /* getrandom with flags=0
     * Sizes above 256 bytes are rejected by the spec. */
    if (len > 256) { errno = EIO; return -1; }
    return __check(__syscall(SYS_getrandom, (long)buf, (long)len, 0, 0, 0, 0));
}

static int stdout_put(char c, FILE *f)
{
    (void)f;
    return write(1, &c, 1) == 1 ? 0 : EOF;
}

static int stderr_put(char c, FILE *f)
{
    (void)f;
    return write(2, &c, 1) == 1 ? 0 : EOF;
}

static int stdin_get(FILE *f)
{
    (void)f;
    unsigned char c;
    return read(0, &c, 1) == 1 ? c : EOF;
}

static FILE stdin_f  = FDEV_SETUP_STREAM(NULL, stdin_get, NULL, _FDEV_SETUP_READ);
static FILE stdout_f = FDEV_SETUP_STREAM(stdout_put, NULL, NULL, _FDEV_SETUP_WRITE);
static FILE stderr_f = FDEV_SETUP_STREAM(stderr_put, NULL, NULL, _FDEV_SETUP_WRITE);
FILE* const stdin = &stdin_f;
FILE* const stdout = &stdout_f;
FILE* const stderr = &stderr_f;
