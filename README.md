# Virtual Memory Allocator

![C](https://img.shields.io/badge/C-17-00599C?logo=c&logoColor=white)
![CMake](https://img.shields.io/badge/CMake-3.20%2B-064F8C?logo=cmake&logoColor=white)

A `malloc`/`free` implementation in C that combines a chunk pool, a buddy allocator and direct `mmap` calls behind a single API.

*Ensimag – 2025 | Team of 1*

## Table of Contents

- [Overview](#overview)
- [Features](#features)
- [How It Works](#how-it-works)
- [Project Structure](#project-structure)
- [Getting Started](#getting-started)
- [Example](#example)
- [Debugging](#debugging)
- [Design Choices](#design-choices)
- [Acknowledgments](#acknowledgments)

## Overview

A memory allocator hands out blocks of memory on request and takes them back when they are freed, so that programs do not have to ask the kernel for memory on every allocation.

This allocator works like the GNU C library's `malloc`: it exposes `emalloc` and `efree`, and routes each request to one of three strategies based on its size:

| Class | Requested size | Strategy | Actual block size |
|---|---|---|---|
| Small | 1 to 64 bytes | Pool of fixed-size chunks | 96 bytes |
| Medium | 65 to 131,071 bytes | Buddy allocator | Next power of two, markers included |
| Large | 131,072 bytes (128 KiB) and above | Direct `mmap` / `munmap` | Size plus markers, rounded up to 8 bytes |

## Features

- Three size classes with dedicated allocation strategies
- Block marking with an address-derived magic value
- Overflow detection on `efree`
- Recursive doubling of the small pool and of the medium memory
- Buddy splitting and merging, with the buddy found by XOR
- Interactive shell (`memshell`) for manual testing
- Works in both 32-bit and 64-bit processes
- Unit tests with GoogleTest

## How It Works

### Block marking

Every block is marked at both ends, which lets `efree` recover its class and size and detect overflows. Each of the four fields takes 8 bytes, in 32-bit and 64-bit processes alike:

```text
   8 bytes  8 bytes                                    8 bytes  8 bytes
 +--------+--------+----------------------+----------+--------+--------+
 |  size  | magic  | memory requested     |  unused  | magic  |  size  |
 |        |        | by the user          |          |        |        |
 +--------+--------+----------------------+----------+--------+--------+
 ^                 ^
 block start       pointer returned to the caller
```

`size` is the total block size: the requested size, plus the unused padding, plus 4 * 8 bytes of markers.

The magic value comes from one round of Knuth's MMIX linear congruential generator, seeded with the block address. Its two low bits are then replaced by the block type:

```text
 +----------------------------------+-------+
 | high 62 bits of the MMIX value   | S/M/L |
 +----------------------------------+-------+
```

```c
magic = knuth_mmix_one_round((unsigned long)address) & ~0b11ULL;
magic |= kind;   // 0 = small, 1 = medium, 2 = large
```

`knuth_mmix_one_round` computes `in * 6364136223846793005UL % 1442695040888963407UL`.

`efree` reads the header to recover the size and type of the block, then compares the footer with the header. A write past the end of the block changes the footer magic, which makes an `assert` stop the program.

### Small allocations

Small requests share a pool of 96-byte chunks: 64 bytes of user data plus 32 bytes of markers. Free chunks form a singly linked list, with each chunk storing the address of the next one in its first word, so allocating and freeing are both constant-time operations.

When the list is empty, the allocator maps a new pool with `mmap`. The first pool holds 128 chunks, and each new pool is twice as large as the previous one (recursive doubling).

### Medium allocations

Medium requests use the buddy algorithm. The size plus markers is rounded up to a power of two, 2^k, and the block is taken from `arena.TZL[k]`. This table (*Table des Zones Libres*) holds one linked list of free blocks per block size.

**Splitting.** If list `k` is empty, the allocator takes the smallest larger free block. It cuts that block in half repeatedly until it reaches size 2^k. If no large enough block exists, it maps more memory, starting at 128 KiB and doubling each time.

**Finding the buddy.** The two halves of a block of size 2^(k+1) differ in a single address bit, because blocks are aligned on their own size:

```c
buddy = (void *)((uintptr_t)block ^ (1UL << k));
```

To guarantee that alignment, the allocator maps twice the size it needs and moves the start of the region to the next multiple of that size.

**Merging.** On `efree`, the allocator looks for the buddy in list `k`. If it is free, the buddy leaves the list and the two blocks merge into one of size 2^(k+1). This repeats at each level until no free buddy is found, then the resulting block goes into the matching list.

### Large allocations

Large requests go straight to `mmap`, and `efree` returns them to the system with `munmap`. The mapped size includes the markers and up to 7 unused bytes, which round it up to a multiple of 8 so that the footer stays aligned.

## Project Structure

```text
.
├── AUTHORS                 # Project member
├── CMakeLists.txt          # Build, test and packaging configuration
├── LICENSE                 # GPLv3 license text
├── README.md               # This file
├── README.org              # Original notes from the project skeleton
├── gpl.txt                 # GPL license text
├── build/                  # Out-of-source build directory
├── src/
│   ├── mem.c               # emalloc/efree entry points, dispatch by size
│   ├── mem.h               # Public API
│   ├── mem_internals.c     # Block marking, memory requests to mmap
│   ├── mem_internals.h     # Constants, arena structure, internal prototypes
│   ├── mem_small.c         # Chunk pool
│   ├── mem_medium.c        # Buddy allocator
│   ├── mem_large.c         # Direct mmap and munmap
│   └── memshell.c          # Interactive test shell
└── tests/
    ├── README.md           # Notes on the test suite
    ├── alloctest.cc        # GoogleTest entry point
    ├── mempymodule.c       # Legacy Python binding, not part of the build
    ├── test_buddy.cc       # Buddy splitting and merging test
    ├── test_buddy.h        # Buddy test declarations
    ├── test_generic.cc     # Basic and random allocation tests
    ├── test_generic.h      # Generic test declarations
    ├── test_mark.cc        # Block marking test
    ├── test_run.H          # Random allocation sequence helpers
    ├── test_run_cpp.cc     # Random allocation sequence
    └── test_run_cpp.h      # Random sequence declarations
```

## Getting Started

### Prerequisites

The project targets Linux and needs these tools. On Debian or Ubuntu, they install with:

```sh
sudo apt install gcc g++ cmake make gdb libgtest-dev
```

CMake 3.20 or later is required.

### Build and test

```sh
cd build
cmake ..
```

Generates the Makefiles in `build/`, and checks that GoogleTest and the `AUTHORS` file are present.

```sh
make
```

Compiles the `libensimalloc.so` library, the `alloctest` test program and the `memshell` shell.

```sh
make test
```

Runs the test suite through CTest and prints a one-line summary.

```sh
make check
```

Runs `alloctest` directly, with the result of each test.

```sh
./alloctest --help
```

Lists the GoogleTest options, including the ones that select individual tests.

```sh
./alloctest --gtest_filter=Medium.buddy
```

Runs a single test, here the buddy test.

```sh
./memshell
```

Starts the interactive shell, where `help` lists the available commands and `exit` quits.

```sh
make package_source
```

Creates a source archive in `build/`.

### 32-bit build

The 32-bit libraries install on Ubuntu with:

```sh
sudo dpkg --add-architecture i386
sudo apt update
sudo apt install gcc-multilib g++-multilib libgtest-dev:i386
```

Then, from the repository root:

```sh
mkdir build32
cd build32
```

Creates a separate build directory.

```sh
cmake .. -DCMAKE_C_FLAGS=-m32 -DCMAKE_CXX_FLAGS=-m32 -DCMAKE_EXE_LINKER_FLAGS=-m32 -DCMAKE_SHARED_LINKER_FLAGS=-m32
```

Generates Makefiles that compile and link everything for 32-bit x86.

```sh
make
make test
make check
```

Compiles the 32-bit binaries and runs the same 12 tests as the 64-bit build.

## Example

A session that allocates one block of each class, frees two of them, then reuses a small chunk:

```sh
printf 'alloc 32\nalloc 1000\nalloc 200000\nused\nfree 2\nfree 1\nalloc 16\nused\nexit\n' | ./memshell
```

```text
**** Mini-shell de test pour l'allocateur mémoire ****
	Tapez help pour la liste des commandes

>1 0x737B4CA17010
>2 0x737B4C880010
>3 0x737B4C849010
>1 0x737B4CA17010 0x20
2 0x737B4C880010 0x3E8
3 0x737B4C849010 0x30D40
>>>4 0x737B4CA17010
>4 0x737B4CA17010 0x10
3 0x737B4C849010 0x30D40
>
```

`alloc` prints the block identifier and address. `used` lists each live block with its identifier, address and requested size in hexadecimal. Block 4 receives the same address as block 1, because the freed chunk is back at the head of the free list.

## Debugging

`memshell` can run under gdb to inspect the state of the allocator, for example the free lists. The allocator lives in a shared library that gdb loads only when the program starts, so allow pending breakpoints first:

```text
$ gdb ./memshell
(gdb) set breakpoint pending on
(gdb) break emalloc_medium
(gdb) run
>alloc 65
```

`alloc 65` is the smallest request that reaches `emalloc_medium`, because 64 bytes still counts as a small allocation. Once the breakpoint stops the program, inspect the call and step out of the function:

```text
(gdb) print size
(gdb) print arena.TZL
(gdb) finish
(gdb) print arena.TZL[8]
```

`print arena.TZL` shows the whole table, where index `k` holds the free blocks of size 2^k. After `finish`, `arena.TZL[8]` holds the 256-byte buddy left over from the split. `layout src` shows the source code next to the gdb prompt.

To find out which instruction changes the head of the small free list, start a new gdb session and set a watchpoint:

```text
(gdb) set breakpoint pending on
(gdb) break emalloc_small
(gdb) run
>alloc 16
(gdb) watch arena.chunkpool
(gdb) continue
```

## Design Choices

- **Three strategies instead of one.** Each size class has a different cost profile: small blocks need speed, medium blocks need limited fragmentation, and large blocks can go back to the system as soon as they are freed.
- **Fixed 96-byte chunks.** All small requests get the same block size, so a free chunk can serve any of them, and no searching or splitting is needed.
- **Free lists stored inside free blocks.** The list pointer uses the first word of each free block, so free blocks need no separate bookkeeping.
- **Recursive doubling.** Doubling each new region keeps the number of `mmap` calls low as demand grows.
- **Size-aligned medium regions.** Mapping twice the required size wastes address space, but aligned blocks make the buddy address a single XOR.
- **Address-derived magic value.** Tying the marker to the block address makes it unlikely that stray data matches the expected value.
- **Zeroing freed medium blocks.** Clearing a block on `efree` costs time proportional to its size, but it guarantees that the first word of an unused buddy is a null list pointer after a later split.

## Acknowledgments

The project skeleton, the test suite and `memshell` were provided by the Ensimag teaching team. The code is distributed under the GPLv3+ license, like the original skeleton.
