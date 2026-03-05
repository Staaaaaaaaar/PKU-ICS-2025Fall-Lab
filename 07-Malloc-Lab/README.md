# ICS - Malloc Lab | 令人头疼的内存碎片……

在 Malloc Lab 中，我们将自主实现一个正确、高效且快速的动态内存分配器，主要目标是最大化**内存利用率**和**吞吐量**。具体需要实现下面函数：

- `int mm_init(void)`：初始化分配器。
- `void *malloc(size_t size)`：分配内存块。
- `void free(void *ptr)`：释放内存块。
- `void *realloc(void *ptr, size_t size)`：调整内存块大小。
- `void *calloc(size_t nmemb, size_t size)`：分配内存块并初始化为零。
- `void mm_checkheap(int lineno)`：堆一致性检查器。

> [!TIP]
> 此实验具有广泛的设计空间，并且错误在分配器中尤其棘手且难以追踪，因此在设计和调试上花费的总时间很可能会超过编码所花的时间。

## 预备知识

在深入实现动态内存分配器之前，我们需要了解内存管理的核心概念和常见策略。

### 内存碎片

内存碎片是动态内存分配中的核心挑战，分为两类：

- **内部碎片**：分配给应用程序的内存块大于实际请求大小，多余空间无法被利用。通常发生在固定大小的分配策略中。
- **外部碎片**：虽然总空闲内存足够，但分散在多个不连续的小块中，无法满足大块内存请求。这是动态内存分配器的主要优化目标。

### 空闲块管理策略

#### 隐式空闲链表

- **原理**：通过块头部的大小和分配位隐式链接空闲块，每次分配需遍历整个堆。
- **优点**：实现简单，无需额外存储空间。
- **缺点**：分配时间复杂度为 O(n)，不适合大规模应用。

#### 显式空闲链表

- **原理**：在空闲块内部维护前驱和后继指针，形成双向链表。
- **优点**：提高空闲块查找效率，支持更复杂的分配策略。
- **缺点**：需要额外空间存储指针，最小块大小增加。

#### 分离空闲链表

- **原理**：将空闲块按大小分类，为每类维护独立链表。
- **优点**：显著减少搜索时间，提高分配速度；可针对不同大小类采用不同策略。
- **缺点**：实现复杂度增加，需要合理设计大小分类策略。

### 适配策略

#### 首次适配（First Fit）

- 从链表头部开始搜索，使用第一个足够大的空闲块。
- **优点**：实现简单，保留大块空闲空间。
- **缺点**：可能导致外部碎片累积，分配质量不稳定。

#### 最佳适配（Best Fit）

- 搜索整个链表，选择最接近请求大小的空闲块。
- **优点**：减少内部碎片，内存利用率高。
- **缺点**：搜索时间较长，可能产生大量难以利用的小碎片。

#### 下次适配（Next Fit）

- 从上次分配位置继续搜索，避免每次都从链表头部开始。
- **优点**：比首次适配更快，分配更均匀。
- **缺点**：可能导致内存利用率下降。

### 性能指标

- **内存利用率**：驱动程序使用的内存总量（即通过 malloc 分配但尚未通过 free 释放的内存，也即任一时刻有效负载的总和）与分配器使用的堆大小之间的峰值比率。
- **吞吐量**：每秒完成的平均操作次数。

## 前置准备

### trace 分析

本实验在 `traces/` 目录下提供了一些 trace 文件用于测试分配器的性能表现。trace 文件记录了一系列内存分配和释放操作，格式如下：

- `a <ptr> <size>`：分配 size 字节的内存，为 ptr 指针分配 size 字节的内存。当 size 字节不存在时，跳过。

- `f <ptr>`：释放 ptr 指针指向的内存。当 ptr 指针不存在时，跳过。

- `r <ptr> <size>`：重新分配 ptr 指针指向的内存，大小为 size 字节。当 ptr 指针不存在时，跳过。

为了能更直观地了解 trace 的内存使用情况，以方便后续设计分配器，我们可以编写一个简单的脚本来分析 trace 文件。

可以参考 [Arthals](https://arthals.ink/) 的版本，我添加了一些图表的可视化。

```python
#!/usr/bin/env python
# -*- encoding: utf-8 -*-
# @File    :   trace-freq.py

import csv
import os
import matplotlib.pyplot as plt
from collections import defaultdict

# 初始化频率表
alloc_freq = defaultdict(int)
realloc_freq = defaultdict(int)
combined_alloc_realloc_freq = defaultdict(int)
free_freq = defaultdict(int)

# 指针编号到大小的映射
pointer_size_map = {}


# 解析 .rep 文件
def parse_file(file_path):
    with open(file_path, "r") as file:
        for line in file:
            # 忽略非英文字符开头的行
            if not line[0].isalpha():
                continue

            parts = line.split()
            action = parts[0]
            pointer_id = int(parts[1])
            size = int(parts[2]) if len(parts) > 2 else None
            if action in ["a", "r"]:  # alloc or realloc
                if size is None:
                    continue
                if action == "a":
                    alloc_freq[size] += 1
                else:
                    realloc_freq[size] += 1
                combined_alloc_realloc_freq[size] += 1
                pointer_size_map[pointer_id] = size  # 更新指针编号到大小的映射
            elif action == "f":  # free
                size = pointer_size_map.get(pointer_id, None)
                if size is not None:
                    free_freq[size] += 1
                    del pointer_size_map[pointer_id]  # 移除映射


# 遍历 traces/ 目录下的所有 .rep 文件
files = [
    # "./traces/alaska.rep",
    "./traces/amptjp.rep",
    "./traces/bash.rep",
    "./traces/boat.rep",
    "./traces/binary2-bal.rep",
    "./traces/cccp.rep",
    "./traces/cccp-bal.rep",
    "./traces/chrome.rep",
    "./traces/coalesce-big.rep",
    # "./traces/coalescing-bal.rep",
    # "./traces/corners.rep",
    "./traces/cp-decl.rep",
    "./traces/exhaust.rep",
    "./traces/expr-bal.rep",
    "./traces/freeciv.rep",
    "./traces/ls.rep",
    # "./traces/malloc.rep",
    # "./traces/malloc-free.rep",
    "./traces/perl.rep",
    # "./traces/realloc.rep"
]
for filename in files:
    parse_file(filename)


# 输出CSV文件的函数
def output_csv(freq_dict, filename):
    with open(filename, "w", newline="") as csvfile:
        writer = csv.writer(csvfile)
        for size, freq in sorted(freq_dict.items(), key=lambda item: item[1], reverse=True):
            writer.writerow([size, freq])


# print(alloc_freq)

# 输出四个CSV文件
if not os.path.exists("trace-summary"):
    os.mkdir("trace-summary")

output_csv(alloc_freq, "trace-summary/alloc_freq.csv")
output_csv(realloc_freq, "trace-summary/realloc_freq.csv")
output_csv(combined_alloc_realloc_freq, "trace-summary/combined_alloc_realloc_freq.csv")
output_csv(free_freq, "trace-summary/free_freq.csv")


# 生成图表的函数
def plot_freq(freq_dict, title, filename):
    if not freq_dict:
        print(f"No data for {title}")
        return

    sizes = sorted(freq_dict.keys())
    freqs = [freq_dict[size] for size in sizes]

    plt.figure(figsize=(12, 6))
    plt.plot(sizes, freqs, alpha=0.7)

    plt.title(title)
    plt.xlabel("Block Size (bytes)")
    plt.ylabel("Frequency")
    plt.grid(True, which="both", ls="-", alpha=0.2)
    plt.savefig(filename)
    plt.close()


plot_freq(alloc_freq, "Allocation Frequency", "trace-summary/alloc_freq.png")
plot_freq(realloc_freq, "Reallocation Frequency", "trace-summary/realloc_freq.png")
plot_freq(combined_alloc_realloc_freq, "Combined Alloc/Realloc Frequency", "trace-summary/combined_alloc_realloc_freq.png")
plot_freq(free_freq, "Free Frequency", "trace-summary/free_freq.png")
```

### inline

`inline` 是 C99 标准引入的关键字，用于建议编译器将函数代码内联展开，而不是进行常规的函数调用。我们可以在一些频繁调用且代码较短的函数前添加 `inline` 关键字，以减少函数调用的开销，从而提高**吞吐量**。

### 表现分

本实验的评分由 80 分的表现分，10 分的代码风格分和 10 分的 `mm_checkheap` 函数实现质量组成。表现分根据分配器在一组 trace 上的**内存利用率**和**吞吐量**进行评估，具体计算方式如下：

$$
P(U,T) = 80\left(w\min(1,\frac{U-U_{min}}{U_{max}-U_{min}})+(1-w)\min(1,\frac{T-T_{min}}{T_{max}-T_{min}})\right)
$$

其中 $w=0.60,U_{min}=0.7,U_{max}=0.9,T_{min}=3000,T_{max}=12000$。意味着我们想要收获全部表现分，就需要使得内存利用率 U ≥ 0.9 和吞吐量 T ≥ 12000。

## 设计思路

为了让我们的动态内存分配器兼顾效率与速度，一个好的设计必不可少。当然适合本实验的设计并不只有一种，对于数据结构和策略算法都有许多不同的选择可供测试，下面提供我选择的方案。

- **地址空间**：由于实验中堆大小保证不会超过 $2^{32}$ 字节，我们可以将 8 字节的地址压缩为 4 字节的**相对堆底偏移**储存，但注意实际使用地址进行数据访问时需要将偏移转换为地址使用。
- **对齐**：我采用 8 字节对齐的规范。

### 边界标记

首先设计分配块和空闲块的结构。

为了能高效地实现空闲块的合并，我们可以使用**边界标记**技术，为每个**空闲块**加上**头部（header）**和**脚部（footer）**储存块大小信息（单位字节），而**分配块**只添加**头部**。由于堆大小不会超过 $2^{32}$ 字节，因此头部和尾部的大小可以设置为 4 字节，同时由于 8 字节对齐，低 3 位一定是 0，可以用来标记块的其他信息：

```text
31                     3  2  1  0
+----------------------+--+--+--+
|      Block Size      |  |PA| A|
+----------------------+--+--+--+
```

第 0 位表征**这个块**是否被分配，第 1 位表征**上个块**是否被分配。

因此分配块和空闲块的组织结构如下：

```text
    Allocated Block             Free Block
+----------------------+   +----------------------+
|  Header (Size|PA|1)  |   |  Header (Size|PA|0)  |
+----------------------+   +----------------------+
|                      |   |     Prev Offset      |
|       Payload        |   +----------------------+
|                      |   |     Next Offset      |
+----------------------+   +----------------------+
                           |       Empty          |
                           |                      |
                           +----------------------+
                           |  Footer (Size|PA|0)  |
                           +----------------------+
```

### 分离空闲链表

接着设计堆的组织结构，主要实现对空闲块的快速访问。

我们可以使用**分离空闲链表**来组织空闲块，将空闲块按照大小区间组织成不同的类，为每个类维护一个**链表**结构，从而每次查询空闲块时只需从对应大小的入口进入即可，可以大大减少访问时间。由于实验要求不能将数据储存在全局变量中，因此我选择将每个链表的表头储存在堆底之前，整体的堆结构如下：

```text
+----------------------+ <- mem_heap_lo
|    List Header 0     |
+----------------------+
|         ...          |
+----------------------+
|    List Header 11    |
+----------------------+
|       Padding        |
+----------------------+
|   Prologue Header    |
+----------------------+
|   Prologue Footer    |
+----------------------+
|                      |
|     User Blocks      |
|                      |
+----------------------+
|   Epilogue Header    |
+----------------------+ <- mem_heap_hi
```

### 最佳适配

再来解决如何选择合适的空闲块分配给特定大小空间的适配问题。

最常见的适配策略有**首次适配**，**下次适配**和**最佳适配**。几种方法各有优劣，可以在网上查阅到许多有关它们对**内存利用率**和**吞吐量**的影响，笔者就不过多赘述。理论上我们选择使用的分离空闲链表某种程度上也采取了最佳适配的思想，但在一些面向 trace 的调试之后我还是选择了**最佳适配**策略来提高内存利用率。

### 空闲块处理

关于连续的空闲块是否合并，在何时合并，新增的空闲块如何插入空闲链表等问题，笔者没有深入比较不同策略对分配器性能的影响，感兴趣的朋友可自行探索。我简单地选择了**空闲块立即合并**以及 **LIFO** 方式维护空闲链表。

### 交替放置

在分配内存空间时，如何将分配块放置在已经选择好的空闲块中也可以有不同的策略。

笔者在这里采用的**交替放置**的策略，具体而言就是若某一次操作将分配块放置在了空闲块的**头部**，那么下次操作就要将分配块放置在空闲块的**尾部**，反之亦然，如此交替放置。

如此奇怪的行为主要是为了解决 trace 中的一类特殊情况，我称之为**棋盘式结构**。其主要特征是大小不同的对象被交替分配，而只释放小对象，形成了高度外部碎片化的内存布局，而由于释放的都是小对象，外部碎片难以被有效利用，极大地降低了内存利用率。

```text
分配后：[A][B][A][B][A][B][A][B]...
释放后：[A][ ][A][ ][A][ ][A][ ]...
```

## 具体实现

实验禁止使用内存相关的任何标准库函数实现，但在 `memlib.c` 中提供了一些函数可供使用：

- `void *mem_sbrk(int incr)`：增加堆大小，返回指向新空间底部的指针。
- `void* mem_heap_lo()`: 返回堆底指针。
- `void* mem_heap_hi()`: 返回堆顶指针。

### 宏定义

我们需要在代码开头定义一些常用的宏，以便于后续使用。

```c
/* If you want debugging output, use the following macro.  When you hand
 * in, remove the #define DEBUG line. */
#define DEBUG
#ifdef DEBUG
#define dbg_printf(...) printf(__VA_ARGS__)
#else
#define dbg_printf(...)
#endif
```

上面的代码定义了调试宏，在希望调试输出的地方可以使用 `dbg_printf` 来打印出调试信息，提交时可以通过注释掉 `#define DEBUG` 来关闭调试输出。还可以利用调试宏来启用 `mm_checkheap` 函数来检查堆的完整性，只需在函数末尾添加如下代码：

```c
#ifdef DEBUG
    mm_checkheap(__LINE__);
#endif
```

接下来我们可以定义一些基本常量和函数来简化代码编写和提高可读性。值得注意的是，对于指针的存储，我们使用了**相对堆底偏移**来节省空间，因此我定义了 `OFF_TO_PTR` 和 `PTR_TO_OFF` 宏来实现偏移与指针的转换。此外，在涉及指针的运算以及指针所指向内容的读写时，需格外小心，可以使用强制类型转换来保证其正确性。

```c
/* 基本常量 */
#define ALIGNMENT 8
#define WSIZE 4                               /* 单字大小为 4 字节 */
#define DSIZE 8                               /* 双字大小为 8 字节 */
#define MIN_BLOCK_SIZE ALIGN(WSIZE * 4)       /* 头+尾+前驱偏移+后继偏移 */
#define CHUNKSIZE (1 << 13)                   /* 默认扩堆大小 */
#define LIST_MAX 12                           /* 分离链表数量 */

#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define MIN(x, y) ((x) < (y) ? (x) : (y))

/* 向上按 ALIGNMENT 对齐 */
#define ALIGN(p) (((size_t)(p) + (ALIGNMENT - 1)) & ~0x7)

/* 将大小与分配位封装进 4 字节字 */
/* 头/尾编码：bit0=本块是否已分配；bit1=前块是否已分配 */
#define PACK(size, alloc, prev_alloc) ((uint32_t)(size) | (alloc) | ((prev_alloc) ? 0x2 : 0))

/* 读写 4 字节头/尾 */
#define GET(p) (*(uint32_t *)(p))
#define PUT(p, val) (*(uint32_t *)(p) = (uint32_t)(val))

/* 偏移<->指针转换：堆底为基址，偏移 0 表示 NULL */
#define OFF_TO_PTR(off) ((off) == 0 ? NULL : (void *)((char *)mem_heap_lo() + (off)))
#define PTR_TO_OFF(ptr) ((ptr) == NULL ? 0u : (uint32_t)((char *)(ptr) - (char *)mem_heap_lo()))

/* 从头/尾中取大小与分配位 */
#define GET_SIZE(p) ((size_t)(GET(p) & ~(uint32_t)0x7))
#define GET_ALLOC(p) (GET(p) & 0x1)
#define GET_PREV_ALLOC(p) (GET(p) & 0x2)

/* 由块指针计算头尾地址 */
#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* 计算前后块地址（基于大小偏移） */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE))) /* 仅在前块空闲时使用（读取前块尾部） */

/* 空闲块载荷中存放链表前驱/后继（4 字节偏移） */
#define PREV_FREEP(bp) OFF_TO_PTR(GET(bp))
#define NEXT_FREEP(bp) OFF_TO_PTR(GET((char *)(bp) + WSIZE))
#define SET_PREV_FREEP(bp, ptr) PUT((char *)(bp), PTR_TO_OFF(ptr))
#define SET_NEXT_FREEP(bp, ptr) PUT((char *)(bp) + WSIZE, PTR_TO_OFF(ptr))
```

### mm_init

`mm_init` 函数用于初始化分配器，主要实现：

- 初始化分离空闲链表的表头。
- 创建前言块和结尾块。
- 扩展空堆。

```c
/*
 * mm_init - 初始化分配器状态并扩展空堆。
 */
int mm_init(void)
{
    size_t init_bytes = (LIST_MAX/2) * DSIZE + 4 * WSIZE;

    heap_listp = NULL;
    list_array = NULL;

    char *base = mem_sbrk((int)init_bytes);
    if (base == (void *)-1)
    {
        return -1;
    }

    list_array = base;
    for (int i = 0; i < LIST_MAX; i++)
    {
        PUT(list_array + i * WSIZE, 0); /* 存储头指针偏移 */
    }

    char *prologue = base + (LIST_MAX/2) * DSIZE;
    PUT(prologue, 0);                             /* 对齐填充 */
    PUT(prologue + WSIZE, PACK(DSIZE, 1, 1));     /* 前言头，前块视为已分配 */
    PUT(prologue + 2 * WSIZE, PACK(DSIZE, 1, 1)); /* 前言尾 */
    PUT(prologue + 3 * WSIZE, PACK(0, 1, 1));     /* 结尾头 */
    heap_listp = prologue + 2 * WSIZE;

    if (extend_heap(CHUNKSIZE / WSIZE) == NULL)
    {
        return -1;
    }
#ifdef DEBUG
    mm_checkheap(__LINE__);
#endif
    return 0;
}
```

#### extend_heap

```c
/* 扩展堆并返回新空闲块 */
static inline void *extend_heap(size_t words)
{
    size_t size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
    char *bp = mem_sbrk((int)size);
    if (bp == (void *)-1)
    {
        return NULL;
    }
    /* 新块的前块状态由旧结尾头记录 */
    int prev_alloc = GET_PREV_ALLOC(HDRP(bp)) ? 1 : 0;

    PUT(HDRP(bp), PACK(size, 0, prev_alloc));
    PUT(FTRP(bp), PACK(size, 0, prev_alloc));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1, 0)); /* 结尾块，前块空闲 */

    return coalesce(bp);
}
```

#### coalesce

```c
/* 与相邻空闲块合并并重新入链 */
static inline void *coalesce(void *bp)
{
    size_t prev_alloc = GET_PREV_ALLOC(HDRP(bp));
    void *next_bp = NEXT_BLKP(bp);
    size_t next_alloc = GET_ALLOC(HDRP(next_bp));
    size_t size = GET_SIZE(HDRP(bp));

    if (!prev_alloc)
    {
        void *prev_bp = PREV_BLKP(bp);
        size_t prev_size = GET_SIZE(HDRP(prev_bp));
        remove_free(prev_bp);
        size += prev_size;
        bp = prev_bp;
        prev_alloc = GET_PREV_ALLOC(HDRP(bp));
    }

    if (!next_alloc)
    {
        remove_free(next_bp);
        size += GET_SIZE(HDRP(next_bp));
    }

    PUT(HDRP(bp), PACK(size, 0, prev_alloc ? 1 : 0));
    PUT(FTRP(bp), PACK(size, 0, prev_alloc ? 1 : 0));
    insert_free(bp);
    set_next_prev_alloc(bp, 0);
    return bp;
}
```

#### remove_free

```c
/* 将空闲块从链表中移除 */
static inline void remove_free(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));
    int idx = list_index(size);
    uint32_t *head = (uint32_t *)(list_array + idx * WSIZE);
    void *prev = PREV_FREEP(bp);
    void *next = NEXT_FREEP(bp);

    if (prev)
    {
        SET_NEXT_FREEP(prev, next);
    }
    else
    {
        *head = PTR_TO_OFF(next);
    }
    if (next)
    {
        SET_PREV_FREEP(next, prev);
    }
}
```

#### list_index

```c
/* 根据块大小选择对应的分离链表下标 */
static inline int list_index(size_t size)
{
    int idx = 64 - __builtin_clzl(size - 1) - 4;
    if (idx >= LIST_MAX)
        return LIST_MAX - 1;
    if (idx < 0)
        return 0;
    return idx;
}
```

这里使用了 GCC 内置函数 `__builtin_clzl` 来计算无符号长整数的前导零位数，从而高效地确定块大小所属的区间，有助于提高吞吐量。

#### insert_free

```c
/* 将空闲块插入对应尺寸链表表头 */
static inline void insert_free(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));
    int idx = list_index(size);
    uint32_t *head = (uint32_t *)(list_array + idx * WSIZE);

    SET_NEXT_FREEP(bp, OFF_TO_PTR(*head));
    SET_PREV_FREEP(bp, NULL);
    if (*head != 0)
    {
        SET_PREV_FREEP(OFF_TO_PTR(*head), bp);
    }
    *head = PTR_TO_OFF(bp);
}
```

#### set_next_prev_alloc

```c
/* 更新后继块头部（如为空闲则连带尾部）的 prev_alloc 位 */
static inline void set_next_prev_alloc(void *bp, int prev_alloc)
{
    char *next = NEXT_BLKP(bp);
    uint32_t hdr = GET(HDRP(next));
    uint32_t new_hdr = (hdr & ~(uint32_t)0x2) | (prev_alloc ? 0x2 : 0);
    PUT(HDRP(next), new_hdr);

    if (!GET_ALLOC(HDRP(next)) && GET_SIZE(HDRP(next)) > 0)
    {
        uint32_t ftr = GET(FTRP(next));
        uint32_t new_ftr = (ftr & ~(uint32_t)0x2) | (prev_alloc ? 0x2 : 0);
        PUT(FTRP(next), new_ftr);
    }
}
```

### malloc

`malloc` 函数用于分配有效载荷的内存块，主要实现：

- 调整请求大小以满足对齐和最小块大小要求。
- 在分离空闲链表中查找合适的空闲块。
- 若未命中则扩展堆。
- 放置分配块。

```c
/*
 * malloc - 分配至少 size 字节有效载荷的块。
 */
void *malloc(size_t size)
{
    size_t asize;      /* 调整后块大小（含头部，若不足最小空闲块则抬高） */
    size_t extendsize; /* 若未命中则需要扩展的字节数 */
    char *bp;

    if (heap_listp == NULL)
    {
        if (mm_init() == -1)
        {
            return NULL;
        }
    }

    if (size == 0)
    {
        return NULL;
    }

    asize = adjust_block_size(size);

    if ((bp = find_fit(asize)) != NULL)
    {
        return place(bp, asize);
    }

    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize / WSIZE)) == NULL)
    {
        return NULL;
    }
    return place(bp, asize);
}
```

#### adjust_block_size

```c
/* 计算含头尾且按 8 对齐的块大小 */
static inline size_t adjust_block_size(size_t size)
{
    size_t asize = ALIGN(size + WSIZE); /* 已分配块仅含头部开销 */
    if (asize < MIN_BLOCK_SIZE)
    {
        asize = MIN_BLOCK_SIZE;
    }
    return asize;
}
```

#### find_fit

```c
/* 分离链表内进行最佳匹配查找 */
static inline void *find_fit(size_t asize)
{
    int idx = list_index(asize);
    for (int i = idx; i < LIST_MAX; i++)
    {
        void *best_bp = NULL;
        size_t best_size = 0;
        int count = 0;

        for (char *bp = OFF_TO_PTR(GET(list_array + i * WSIZE)); bp != NULL; bp = NEXT_FREEP(bp))
        {
            size_t curr_size = GET_SIZE(HDRP(bp));
            if (curr_size == asize)
            {
                return bp; /* 完美匹配 */
            }
            if (curr_size > asize)
            {
                if (!best_bp || curr_size < best_size)
                {
                    best_bp = bp;
                    best_size = curr_size;
                    if (curr_size - asize <= 32) /* 足够好的匹配 */
                        return bp;
                }
            }
            count++;
            if (count > 2 && best_bp) /* 限制搜索深度 */
                break;
        }
        if (best_bp)
            return best_bp;
    }
    return NULL;
}
```

这里的**搜索深度**和**匹配阈值**均为经验值，可以根据实际 trace 进行调试和优化。

#### place

```c
/* 放置分配块，若剩余足够大则分裂 */
static int place_strategy = 0;
static inline void *place(void *bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));
    int prev_alloc = GET_PREV_ALLOC(HDRP(bp)) ? 1 : 0;
    remove_free(bp);

    if (csize - asize >= MIN_BLOCK_SIZE)
    {
        if (place_strategy == 0) {
            PUT(HDRP(bp), PACK(asize, 1, prev_alloc));
            void *split = NEXT_BLKP(bp);
            size_t remainder = csize - asize;
            PUT(HDRP(split), PACK(remainder, 0, 1));
            PUT(FTRP(split), PACK(remainder, 0, 1));
            insert_free(split);
            set_next_prev_alloc(split, 0);
            place_strategy = 1;
            return bp;
        } else {
            size_t remainder = csize - asize;
            PUT(HDRP(bp), PACK(remainder, 0, prev_alloc));
            PUT(FTRP(bp), PACK(remainder, 0, prev_alloc));
            insert_free(bp);

            void *new_bp = NEXT_BLKP(bp);
            PUT(HDRP(new_bp), PACK(asize, 1, 0));
            set_next_prev_alloc(new_bp, 1);
            place_strategy = 0;
            return new_bp;
        }
    }
    else
    {
        PUT(HDRP(bp), PACK(csize, 1, prev_alloc));
        set_next_prev_alloc(bp, 1);
        return bp;
    }
}
```

这里采用了**交替放置**的策略来应对 trace 中的**棋盘式结构**问题。

### free

`free` 函数用于释放之前分配的块，主要实现：

- 标记块为空闲。
- 与相邻空闲块合并。
- 将新空闲块插入空闲链表。
- 更新后继块的 `prev_alloc` 位。

```c
/*
 * free - 释放之前分配的块。
 */
void free(void *ptr)
{
    if (ptr == NULL)
    {
        return;
    }

    if (heap_listp == NULL)
    {
        if (mm_init() == -1)
        {
            return;
        }
    }

    size_t size = GET_SIZE(HDRP(ptr));
    int prev_alloc = GET_PREV_ALLOC(HDRP(ptr)) ? 1 : 0;

    PUT(HDRP(ptr), PACK(size, 0, prev_alloc));
    PUT(FTRP(ptr), PACK(size, 0, prev_alloc));
    coalesce(ptr);
#ifdef DEBUG
    mm_checkheap(__LINE__);
#endif
}
```

### realloc

`realloc` 函数用于调整块大小，保留原内容，主要实现：

- 处理特殊情况（`NULL` 指针或大小为 0）。
- 若新大小小于等于当前块大小，则拆分块。
- 若新大小大于当前块大小，尝试扩展到下一个空闲块。
- 否则分配新块，复制内容并释放旧块。
- 返回新指针。

```c
/*
 * realloc - 调整块大小，保留原内容。
 */
void *realloc(void *oldptr, size_t size)
{
    if (oldptr == NULL)
    {
        return malloc(size);
    }
    if (size == 0)
    {
        free(oldptr);
        return NULL;
    }

    size_t oldsize = GET_SIZE(HDRP(oldptr));
    size_t asize = adjust_block_size(size);

    if (asize <= oldsize)
    {
        /* 拆分已分配块 */
        size_t remainder = oldsize - asize;
        if (remainder >= MIN_BLOCK_SIZE)
        {
            int prev_alloc = GET_PREV_ALLOC(HDRP(oldptr)) ? 1 : 0;
            PUT(HDRP(oldptr), PACK(asize, 1, prev_alloc));
            void *split = NEXT_BLKP(oldptr);
            PUT(HDRP(split), PACK(remainder, 0, 1));
            PUT(FTRP(split), PACK(remainder, 0, 1));
            set_next_prev_alloc(split, 0);
            coalesce(split);
        }
        return oldptr;
    }

    /* 尝试扩展到下一个空闲块 */
    void *next = NEXT_BLKP(oldptr);
    if (!GET_ALLOC(HDRP(next)))
    {
        size_t combined = oldsize + GET_SIZE(HDRP(next));
        if (combined >= asize)
        {
            remove_free(next);
            size_t remainder = combined - asize;
            size_t newsize = combined;
            if (remainder >= MIN_BLOCK_SIZE)
            {
                newsize = asize;
            }

            int prev_alloc = GET_PREV_ALLOC(HDRP(oldptr)) ? 1 : 0;
            PUT(HDRP(oldptr), PACK(newsize, 1, prev_alloc));

            if (remainder >= MIN_BLOCK_SIZE)
            {
                void *split = NEXT_BLKP(oldptr);
                PUT(HDRP(split), PACK(remainder, 0, 1));
                PUT(FTRP(split), PACK(remainder, 0, 1));
                insert_free(split);
                set_next_prev_alloc(split, 0);
            }
            else
            {
                set_next_prev_alloc(oldptr, 1);
            }
            return oldptr;
        }
    }

    void *newptr = malloc(size);
    if (newptr == NULL)
    {
        return NULL;
    }
    size_t copy_size = oldsize;
    copy_size = MIN(size, oldsize);
    memcpy(newptr, oldptr, copy_size);
    free(oldptr);
#ifdef DEBUG
    mm_checkheap(__LINE__);
#endif
    return newptr;
}
```

`realloc` 函数也有许多不同的实现方式，但实际上经分析 trace 中对 `realloc` 的调用并不多，因此一个相对好的实现即可。

### calloc

`calloc` 函数用于分配并清零一个数组，主要实现：

- 计算总字节数。
- 调用 `malloc` 分配内存。
- 使用 `memset` 清零内存。
- 返回指针。

```c
/*
 * calloc - 分配并清零一个数组。
 */
void *calloc(size_t nmemb, size_t size)
{
    size_t bytes = nmemb * size;
    void *bp = malloc(bytes);
    if (bp)
    {
        memset(bp, 0, bytes);
    }
#ifdef DEBUG
    mm_checkheap(__LINE__);
#endif
    return bp;
}
```

由于 `calloc` 并不计入最终的**内存利用率**和**吞吐量**评分，因此一个简单的实现即可。

### mm_checkheap

`mm_checkheap` 函数用于进行一些堆检查，确保堆的一致性和正确性，主要用于调试，可以根据具体的需要来进行个性化的调整。但注意频繁地调用可能会拖垮运行速度，以及在评分时请务必注释掉调试宏。

下面是笔者的实现，供参考。

```c
/*
 * mm_checkheap - 堆一致性检查，正常时静默，异常时报错退出。
 */
void mm_checkheap(int lineno)
{
    if (heap_listp == NULL)
    {
        return;
    }

    check_prologue_epilogue(lineno);
    int list_count = check_free_lists(lineno);
    int heap_count = check_heap_linear(lineno);

    if (list_count != heap_count) {
        heap_error(lineno, "Free List Error: Free block count mismatch between free list and heap traversal");
    }
}
```

#### heap_error

```c
/* 堆检查失败时打印并退出 */
static inline void heap_error(int lineno, const char *msg)
{
    dbg_printf("[line %d] %s\n", lineno, msg);
    exit(1);
}
```

#### check_prologue_epilogue

```c
/* 检查前言与结尾块结构 */
static inline void check_prologue_epilogue(int lineno)
{
    if (GET_SIZE(HDRP(heap_listp)) != DSIZE || !GET_ALLOC(HDRP(heap_listp)))
    {
        heap_error(lineno, "Prologue Error: bad prologue header");
    }

    char *bp = heap_listp;
    while ((void*)bp < mem_heap_hi())
    {
        bp = NEXT_BLKP(bp);
    }

    if (GET_SIZE(HDRP(bp)) != 0) {
        heap_error(lineno, "Epilogue Error: epilogue block size is invalid");
    }
    if (GET_ALLOC(HDRP(bp)) != 1) {
        heap_error(lineno, "Epilogue Error: epilogue block is not allocated");
    }
}
```

#### check_free_lists

```c
/* 遍历所有空闲链表，校验一致性 */
static inline int check_free_lists(int lineno)
{
    int count = 0;
    for (int i = 0; i < LIST_MAX; i++)
    {
        for (char *bp = OFF_TO_PTR(GET(list_array + i * WSIZE)); bp != NULL; bp = NEXT_FREEP(bp))
        {
            count++;
            if (!in_heap_region(bp))
            {
                heap_error(lineno, "Free List Error: free list pointer outside heap");
            }
            if (GET_ALLOC(HDRP(bp)))
            {
                heap_error(lineno, "Free List Error: allocated block found in free list");
            }
            size_t size = GET_SIZE(HDRP(bp));
            if (list_index(size) != i)
            {
                heap_error(lineno, "Free List Error: free block in wrong size class");
            }
            if (NEXT_FREEP(bp) && PREV_FREEP(NEXT_FREEP(bp)) != bp)
            {
                heap_error(lineno, "Free List Error: free list forward link broken");
            }
            if (PREV_FREEP(bp) && NEXT_FREEP(PREV_FREEP(bp)) != bp)
            {
                heap_error(lineno, "Free List Error: free list backward link broken");
            }
        }
    }
    return count;
}
```

#### in_heap_region

```c
/*
 * 判断指针是否位于堆内，调试辅助。
 */
static inline int in_heap_region(const void *p)
{
    return p >= mem_heap_lo() && p <= mem_heap_hi();
}
```

#### check_heap_linear

```c
/* 线性遍历堆，验证 prev_alloc 位与相邻关系 */
static inline int check_heap_linear(int lineno)
{
    int free_count = 0;
    int prev_alloc = 1;
    for (char *bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp))
    {
        if (!GET_ALLOC(HDRP(bp))) {
            free_count++;
        }
        int header_prev = GET_PREV_ALLOC(HDRP(bp)) ? 1 : 0;
        if (header_prev != prev_alloc)
        {
            heap_error(lineno, "Consistency Error: prev_alloc bit disagrees with previous block");
        }

        check_block(bp, lineno);

        if (!GET_ALLOC(HDRP(bp)) && !GET_ALLOC(HDRP(NEXT_BLKP(bp))))
        {
            heap_error(lineno, "Consistency Error: consecutive free blocks not coalesced");
        }

        prev_alloc = GET_ALLOC(HDRP(bp)) ? 1 : 0;
    }

    char *epilogue = heap_listp;
    while (GET_SIZE(HDRP(epilogue)) > 0)
    {
        epilogue = NEXT_BLKP(epilogue);
    }

    if ((GET_PREV_ALLOC(HDRP(epilogue)) ? 1 : 0) != prev_alloc)
    {
        heap_error(lineno, "Epilogue Error: epilogue prev_alloc bit incorrect");
    }
    return free_count;
}
```

#### check_block

```c
/* 检查单个块的基本不变量 */
static inline void check_block(void *bp, int lineno)
{
    if (!aligned_ptr(bp))
    {
        heap_error(lineno, "Block Error: payload not 8-byte aligned");
    }
    if (!in_heap_region(bp))
    {
        heap_error(lineno, "Block Error: pointer outside heap region");
    }

    size_t size = GET_SIZE(HDRP(bp));
    if (size % DSIZE != 0) {
        heap_error(lineno, "Block Error: block size not aligned to DSIZE");
    }
    if (bp != heap_listp && size < MIN_BLOCK_SIZE) {
        heap_error(lineno, "Block Error: block size smaller than MIN_BLOCK_SIZE");
    }

    if (!GET_ALLOC(HDRP(bp)) && (GET(HDRP(bp)) != GET(FTRP(bp))))
    {
        heap_error(lineno, "Block Error: header/footer mismatch");
    }
}
```

#### aligned_ptr

```c
/*
 * 判断指针是否按 8 字节对齐。
 */
static inline int aligned_ptr(const void *p)
{
    return (size_t)ALIGN(p) == (size_t)p;
}
```

### 测试结果

```bash
make && ./mdriver
```

```text
Using default tracefiles in ./traces/
Measuring performance with a cycle counter.
Processor clock rate ~= 2000.0 MHz
...................
Results for mm malloc:
  valid  util   ops    secs     Kops  trace
   yes    77%  100000  0.007791 12836 ./traces/alaska.rep
 * yes    99%    4805  0.001299  3698 ./traces/amptjp.rep
 * yes    81%    4162  0.000277 15006 ./traces/bash.rep
 * yes    80%   57716  0.003124 18473 ./traces/boat.rep
 u yes    94%      --        --    -- ./traces/binary2-bal.rep
 * yes    99%    5032  0.001136  4430 ./traces/cccp.rep
 * yes    76%   11991  0.000798 15019 ./traces/chrome.rep
 * yes    98%   20000  0.001193 16765 ./traces/coalesce-big.rep
   yes    50%   14400  0.000491 29353 ./traces/coalescing-bal.rep
   yes   100%      15  0.000009  1723 ./traces/corners.rep
 * yes    99%    5683  0.001923  2955 ./traces/cp-decl.rep
 u yes    71%      --        --    -- ./traces/exhaust.rep
 * yes    99%    5380  0.001953  2755 ./traces/expr-bal.rep
 * yes    98%   55092  0.003527 15619 ./traces/freeciv.rep
 * yes    88%     372  0.000028 13417 ./traces/ls.rep
   yes    17%      10  0.000005  1893 ./traces/malloc.rep
   yes    14%      17  0.000005  3096 ./traces/malloc-free.rep
 p yes     --    1494  0.000114 13092 ./traces/perl.rep
   yes    69%   14401  0.000605 23784 ./traces/realloc.rep
12 11     90%  171727  0.015374 11170

Perf index = 48 (util) & 29 (thru) = 77/80
```

可以根据输出定位到具体的 trace 进行调试和优化，以获得更好的性能表现。其中：

- `*` 表示该 trace 为同时计入内存利用率和吞吐量指标的测试。
- `u` 表示该 trace 仅计入内存利用率指标的测试。
- `p` 表示该 trace 仅计入吞吐量指标的测试。
- 没有标记的表示该 trace 不计入任何评分指标，仅作正确性测试。

请注意，实际测试的 KOPS 值可能会因机器性能和当前负载而有所不同。

---

[更适合北大宝宝体质的 Malloc Lab 踩坑记](https://arthals.ink/blog/malloc-lab)
