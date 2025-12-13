/* mm.c
 *
 * 策略：
 * 
 * 分离空闲链表 + 最佳适配 + 边界标记 + 空闲块立即合并+ 头部插入 + 交替放置
 * 
 * 说明：
 * 
 * 采用分离链表存储不同尺寸的空闲块，链表内采用最佳适配策略查找合适块。
 * 对于空闲块，使用边界标记法存储头尾信息以支持双向合并；已分配块仅存储头部信息以节省空间。
 * 每次释放块时立即与相邻空闲块合并。新释放的空闲块插入对应链表表头。
 * 为减少小块内碎片，放置分配块时交替地将块放置在空闲块的前部或后部。
 * 
 * 堆结构：
 * 
 *  Low Address                                                           High Address
 *  +-------------------+-------------------+-------------------+-------------------+
 *  | Segregated List   | Padding           | Prologue          | Prologue          |
 *  | Heads Array       | (4 Bytes)         | Header            | Footer            |
 *  | (LIST_MAX * 4B)   |                   | (4 Bytes)         | (4 Bytes)         |
 *  +-------------------+-------------------+-------------------+-------------------+
 *  ^                                                           ^
 *  |                                                           |
 *  list_array                                                  heap_listp
 * 
 *  +-------------------------------------------------------------------------------+
 *  |                          Regular Blocks (Allocated / Free)                    |
 *  |                                                                               |
 *  +-------------------------------------------------------------------------------+
 *  |                                                                               |
 *  v                                                                               v
 *  +-------------------+-------------------+-------------------+-------------------+
 *  | Epilogue Header   |                   |                   |                   |
 *  | (0 | alloc)       |                   |                   |                   |
 *  +-------------------+-------------------+-------------------+-------------------+
 * 
 *  Allocated Block:
 *  +-------------------+---------------------------------------+
 *  | Header (4 Bytes)  | Payload (User Data)                   |
 *  | (size | alloc)    |                                       |
 *  +-------------------+---------------------------------------+
 * 
 *  Smallest Free Block:
 *  +-------------------+-------------------+-------------------+-------------------+
 *  | Header (4 Bytes)  | NEXT_FREEP (4B)   | PREV_FREEP (4B)   | Footer (4 Bytes)  |
 *  | (size | alloc)    | (Relative Ptr)    | (Relative Ptr)    | (size | alloc)    |
 *  +-------------------+-------------------+-------------------+-------------------+
 * 
 */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mm.h"
#include "memlib.h"

/* If you want debugging output, use the following macro.  When you hand
 * in, remove the #define DEBUG line. */
// #define DEBUG
#ifdef DEBUG
#define dbg_printf(...) printf(__VA_ARGS__)
#else
#define dbg_printf(...)
#endif

/* do not change the following! */
#ifdef DRIVER
/* create aliases for driver tests */
#define malloc mm_malloc
#define free mm_free
#define realloc mm_realloc
#define calloc mm_calloc
#endif /* def DRIVER */

/* 基本常量 */
#define ALIGNMENT 8
#define WSIZE 4                               /* 单字大小为 4 字节 */
#define DSIZE 8                               /* 双字大小为 8 字节 */
#define MIN_BLOCK_SIZE ALIGN(WSIZE * 4)       /* 头+尾+前驱偏移+后继偏移 */
#define CHUNKSIZE ((1 << 13))                 /* 默认扩堆大小 */
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

/* 全局状态：堆上维护分离链表头指针数组 */
static char *heap_listp = NULL; /* 指向前言块的载荷 */
static char *list_array = NULL; /* 分离链表头数组起始地址（位于堆内） */

/* 辅助函数声明 */
static inline size_t adjust_block_size(size_t size);
static inline int list_index(size_t size);
static inline void insert_free(void *bp);
static inline void remove_free(void *bp);
static inline void *extend_heap(size_t words);
static inline void *coalesce(void *bp);
static inline void *find_fit(size_t asize);
static inline void *place(void *bp, size_t asize);
static inline void set_next_prev_alloc(void *bp, int prev_alloc);
static inline void heap_error(int lineno, const char *msg);
static inline int in_heap_region(const void *p);
static inline int aligned_ptr(const void *p);
static inline void check_block(void *bp, int lineno);
static inline int check_free_lists(int lineno);
static inline void check_prologue_epilogue(int lineno);
static inline int check_heap_linear(int lineno);

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

/* === 辅助函数 === */

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

/* === 检查函数 === */

/*
 * 判断指针是否位于堆内，调试辅助。
 */
static inline int in_heap_region(const void *p)
{
    return p >= mem_heap_lo() && p <= mem_heap_hi();
}

/*
 * 判断指针是否按 8 字节对齐。
 */
static inline int aligned_ptr(const void *p)
{
    return (size_t)ALIGN(p) == (size_t)p;
}

/* 堆检查失败时打印并退出 */
static inline void heap_error(int lineno, const char *msg)
{
    dbg_printf("[line %d] %s\n", lineno, msg);
    exit(1);
}

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

