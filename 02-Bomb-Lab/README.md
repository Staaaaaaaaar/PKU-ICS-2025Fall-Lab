# ICS - Bomb Lab | 拆弹！我吗？

在 Bomb Lab 中，我们将要拆解一个 “二进制炸弹” 。这种 “炸弹” 本质是一个包含多阶段关卡的程序，每个阶段都要求你输入特定字符串。若输入正确，该阶段的 “炸弹” 就会被成功拆除，程序将进入下一阶段；一旦输入错误，“炸弹” 便会立刻 “引爆” ，直接终止运行。只有所有阶段全部破解，这颗 “二进制炸弹” 才算彻底解除威胁。

## 预备知识

### GDB 工具

GDB（GNU Debugger）是一个强大的调试工具，可以帮助我们分析和调试程序。在 Bomb Lab 中，我们将使用 GDB 来逐步执行炸弹程序，观察其行为，并找到正确的输入。

### 汇编语言

了解汇编语言的基本知识对于理解炸弹程序的行为至关重要。我们需要能够阅读和理解反汇编代码，清楚程序的机器级表示，以便找到每个阶段的 “拆弹密码” 。

## 前置准备

### 熟悉 GDB 工具

建议事先阅读 CSAPP 中的 3.10.2 章节，这里给出部分指令参考：

| 指令        | 含义             | 描述                                             |
| ----------- | ---------------- | ------------------------------------------------ |
| r           | run              | 开始执行程序，直到下一个断点或程序结束           |
| b           | break            | 在指定位置设置断点                               |
| q           | quit             | 退出 GDB 调试器                                  |
| ni          | next instruction | 执行下一条指令                                   |
| si          | step instruction | 执行当前指令，若是函数调用则进入函数             |
| n           | next             | 执行下一行代码                                   |
| s           | step             | 执行下一行代码，若有函数调用则进入函数           |
| c           | continue         | 从当前位置继续执行程序，直到下一个断点或程序结束 |
| p           | print            | 打印变量的值                                     |
| p $rax      |                  | 打印 %rax 寄存器的值                             |
| p/x $rsp    |                  | 以十六进制打印 %rsp 寄存器的值                   |
| p/d $rsp    |                  | 以十进制打印 %rsp 寄存器的值                     |
| x           | examine          | 打印内存中的值                                   |
| x/2x $rsp   |                  | 以十六进制格式查看 %rsp 处的 2 个内存单元        |
| x/2c $rsp   |                  | 以字符格式查看 %rsp 处的 2 个内存单元            |
| x/s $rsp    |                  | 将 %rsp 处的内存视为 C 风格字符串查看            |
| x/b $rsp    |                  | 查看 %rsp 处的 1 个字节内存                      |
| x/h $rsp    |                  | 查看 %rsp 处的 1 个字（2 字节）内存              |
| x/w $rsp    |                  | 查看 %rsp 处的 1 个二字（4 字节）内存            |
| x/g $rsp    |                  | 查看 %rsp 处的 1 个四字（8 字节）内存            |
| j           | jump             | 跳转到程序指定位置                               |
| layout asm  | assembly layout  | 显示汇编代码视图                                 |
| layout regs | register layout  | 显示寄存器状态视图                               |

### 了解炸弹结构

首先，查看一下给出的 `bomb.c` 文件，了解炸弹的整体结构。

```c
int main(int argc, char *argv[])
{
    // ...
    input = read_line();
    phase_1(input);
    phase_defused(fp);
    printf("Phase 1 defused. How about the next one?\n");
    // ...
    return 0;
}
```

炸弹程序包含六个 `phase` ，每个 `phase` 都会读入一行输入，然后调用一个 `phase_defused` 函数来验证输入。

接着，我们反汇编 `bomb` 可执行文件，查看每个炸弹的细节。

```bash
objdump -d bomb > bomb.asm
```

阅读汇编代码，我们发现每个 `phase` 函数都包含一个 `explode_bomb` 的调用指令，形如：

```asm
27c1:	e8 1d 08 00 00       	call   2fe3 <explode_bomb>
```

显而易见，这个函数会在输入错误时引爆炸弹，这是我们不想看到的。

### 基本配置

#### 设置断点

前文提到，我们不希望程序运行 `explode_bomb` 函数，因此我们利用 `gdb` 工具给 `explode_bomb` 设置断点，防止程序继续执行到引爆炸弹的指令。

```bash
gdb bomb
(gdb) b explode_bomb
```

同时，为了方便我们对每个 `phase` 进行调试，我们也可以在每个 `phase` 函数入口处设置断点。

```bash
(gdb) b phase_1
(gdb) b phase_2
(gdb) b phase_3
(gdb) b phase_4
(gdb) b phase_5
(gdb) b phase_6
```

#### 打开视图

为了更好地观察程序每一步的执行情况以及寄存器状态，我们可以打开汇编代码视图和寄存器状态视图。

```bash
(gdb) layout asm
(gdb) layout regs
```

#### 输入重定向

查看原代码发现，`bomb.c` 程序提供了文件读入的方式。为方便调试，我们可以将输入重定向到一个文件 `psol.txt` 中。

```bash
gdb bomb psol.txt
```

#### 添加默认配置

为了避免每次启动 `gdb` 都要手动设置断点，我们可以在当前目录下创建一个 `.gdbinit` 文件来设置 `gdb` 进入时的一些默认配置。

```bash
# 创建当前目录下的 .gdbinit 文件
touch .gdbinit
# 创建 .config/gdb 文件夹
mkdir -p ~/.config/gdb
# 允许 gdb 预加载根目录下所有的文件
echo "set auto-load safe-path /" > ~/.config/gdb/gdbinit
```

在 `.gdbinit` 中添加默认配置：

```
set args psol.txt

layout asm
layout regs

b explode_bomb

b phase_1
b phase_2
b phase_3
b phase_4
b phase_5
b phase_6

r
```

完成这些之后，我们就可以直接运行 `gdb bomb` 来开始拆弹了。

## Phase 1

```asm
00000000000027a4 <phase_1>:
    27a4:	f3 0f 1e fa          	endbr64
    27a8:	48 83 ec 08          	sub    $0x8,%rsp
    27ac:	48 8d 35 05 2a 00 00 	lea    0x2a05(%rip),%rsi        # 51b8 <_IO_stdin_used+0x1b8>
    27b3:	e8 16 05 00 00       	call   2cce <strings_not_equal>
    27b8:	85 c0                	test   %eax,%eax
    27ba:	75 05                	jne    27c1 <phase_1+0x1d>
    27bc:	48 83 c4 08          	add    $0x8,%rsp
    27c0:	c3                   	ret
    27c1:	e8 1d 08 00 00       	call   2fe3 <explode_bomb>
    27c6:	eb f4                	jmp    27bc <phase_1+0x18>
```

从汇编代码中可以看出，`phase_1` 函数调用了 `strings_not_equal` 函数来比较输入字符串和某个预设字符串是否相等。因此，我们只需要在执行 `call strings_not_equal` 之前，获取到 `%rsi` 指向的字符串即可。

![](./.assets/phase-1.png)

可见 phase 1 的答案就是：

```text
We can be both of God and the devil. Since we're trying to raise the dead against the stream of time.
```

## Phase 2

```asm
00000000000027c8 <phase_2>:
    27c8:	f3 0f 1e fa          	endbr64
    27cc:	53                   	push   %rbx
    27cd:	48 83 ec 20          	sub    $0x20,%rsp
    27d1:	64 48 8b 04 25 28 00 	mov    %fs:0x28,%rax
    27d8:	00 00
    27da:	48 89 44 24 18       	mov    %rax,0x18(%rsp)
    27df:	31 c0                	xor    %eax,%eax
    27e1:	48 89 e6             	mov    %rsp,%rsi
    27e4:	e8 80 08 00 00       	call   3069 <read_six_numbers>
    27e9:	83 3c 24 00          	cmpl   $0x0,(%rsp)
    27ed:	75 07                	jne    27f6 <phase_2+0x2e>
    27ef:	83 7c 24 04 01       	cmpl   $0x1,0x4(%rsp)
    27f4:	74 05                	je     27fb <phase_2+0x33>
    27f6:	e8 e8 07 00 00       	call   2fe3 <explode_bomb>
    27fb:	bb 02 00 00 00       	mov    $0x2,%ebx
    2800:	eb 03                	jmp    2805 <phase_2+0x3d>
    2802:	83 c3 01             	add    $0x1,%ebx
    2805:	83 fb 05             	cmp    $0x5,%ebx
    2808:	7f 24                	jg     282e <phase_2+0x66>
    280a:	48 63 c3             	movslq %ebx,%rax
    280d:	8d 53 fe             	lea    -0x2(%rbx),%edx
    2810:	48 63 d2             	movslq %edx,%rdx
    2813:	8b 0c 94             	mov    (%rsp,%rdx,4),%ecx
    2816:	8d 53 ff             	lea    -0x1(%rbx),%edx
    2819:	48 63 d2             	movslq %edx,%rdx
    281c:	8b 14 94             	mov    (%rsp,%rdx,4),%edx
    281f:	8d 14 4a             	lea    (%rdx,%rcx,2),%edx
    2822:	39 14 84             	cmp    %edx,(%rsp,%rax,4)
    2825:	74 db                	je     2802 <phase_2+0x3a>
    2827:	e8 b7 07 00 00       	call   2fe3 <explode_bomb>
    282c:	eb d4                	jmp    2802 <phase_2+0x3a>
    282e:	48 8b 44 24 18       	mov    0x18(%rsp),%rax
    2833:	64 48 2b 04 25 28 00 	sub    %fs:0x28,%rax
    283a:	00 00
    283c:	75 06                	jne    2844 <phase_2+0x7c>
    283e:	48 83 c4 20          	add    $0x20,%rsp
    2842:	5b                   	pop    %rbx
    2843:	c3                   	ret
    2844:	e8 67 fa ff ff       	call   22b0 <__stack_chk_fail@plt>
```

phase 2 相较于 phase 1 复杂了一些，我们一步一步拆解。

```asm
00000000000027c8 <phase_2>:
    ; ...
    27da:	48 89 44 24 18       	mov    %rax,0x18(%rsp)
    27df:	31 c0                	xor    %eax,%eax
    27e1:	48 89 e6             	mov    %rsp,%rsi
    27e4:	e8 80 08 00 00       	call   3069 <read_six_numbers>
    27e9:	83 3c 24 00          	cmpl   $0x0,(%rsp)
    27ed:	75 07                	jne    27f6 <phase_2+0x2e>
    ; ...
    27f6:	e8 e8 07 00 00       	call   2fe3 <explode_bomb>
```

程序首先调用 `read_six_numbers` 读取 6 个数，然后将 (%rsp) 与 0 进行比较，如果不等于 0 则引爆炸弹。我们可以先尝试性地输入 `1 2 3 4 5 6` ，在程序运行到 `cmpl $0x0,(%rsp)` 时，查看 `(%rsp)` 的内容。

![](./assets/phase-2.png)

可见输入的 6 个数被保存在以 `%rsp` 为起始地址的内存中，每个数占 4 个字节。知道了这一点，后面的分析就简单多了。

```asm
00000000000027c8 <phase_2>:
    ; ...
    27e4:	e8 80 08 00 00       	call   3069 <read_six_numbers>
    27e9:	83 3c 24 00          	cmpl   $0x0,(%rsp)  ; numbers[0] = 0
    27ed:	75 07                	jne    27f6 <phase_2+0x2e>
    27ef:	83 7c 24 04 01       	cmpl   $0x1,0x4(%rsp)  ; numbers[1] = 1
    27f4:	74 05                	je     27fb <phase_2+0x33>
    27f6:	e8 e8 07 00 00       	call   2fe3 <explode_bomb>
    27fb:	bb 02 00 00 00       	mov    $0x2,%ebx
    2800:	eb 03                	jmp    2805 <phase_2+0x3d>
    2802:	83 c3 01             	add    $0x1,%ebx
    2805:	83 fb 05             	cmp    $0x5,%ebx
    2808:	7f 24                	jg     282e <phase_2+0x66>  ; if (%ebx > 5) done
    280a:	48 63 c3             	movslq %ebx,%rax
    280d:	8d 53 fe             	lea    -0x2(%rbx),%edx
    2810:	48 63 d2             	movslq %edx,%rdx
    2813:	8b 0c 94             	mov    (%rsp,%rdx,4),%ecx  ; a = numbers[%rbx-2]
    2816:	8d 53 ff             	lea    -0x1(%rbx),%edx
    2819:	48 63 d2             	movslq %edx,%rdx
    281c:	8b 14 94             	mov    (%rsp,%rdx,4),%edx  ; b = numbers[%rbx-1]
    281f:	8d 14 4a             	lea    (%rdx,%rcx,2),%edx  ; c = 2*a + b
    2822:	39 14 84             	cmp    %edx,(%rsp,%rax,4)  ; numbers[%rbx] = c
    2825:	74 db                	je     2802 <phase_2+0x3a>
    2827:	e8 b7 07 00 00       	call   2fe3 <explode_bomb>
    282c:	eb d4                	jmp    2802 <phase_2+0x3a>
    282e:	48 8b 44 24 18       	mov    0x18(%rsp),%rax
    ; ...
```

对关键步分析后，我们可以总结出 phase 2 的要求：

1. 输入的第一个数必须为 0
2. 输入的第二个数必须为 1
3. 从第三个数开始，必须满足 `numbers[i] = 2 * numbers[i-2] + numbers[i-1]` 的关系

根据上述要求，我们可以计算出 phase 2 的答案：

```text
0 1 1 3 5 11
```

## Phase 3

```asm
0000000000002849 <phase_3>:
    2849:	f3 0f 1e fa          	endbr64
    284d:	48 83 ec 18          	sub    $0x18,%rsp
    2851:	64 48 8b 04 25 28 00 	mov    %fs:0x28,%rax
    2858:	00 00
    285a:	48 89 44 24 08       	mov    %rax,0x8(%rsp)
    285f:	31 c0                	xor    %eax,%eax
    2861:	48 8d 4c 24 04       	lea    0x4(%rsp),%rcx
    2866:	48 89 e2             	mov    %rsp,%rdx
    2869:	48 8d 35 f8 2d 00 00 	lea    0x2df8(%rip),%rsi        # 5668 <array.0+0x328>
    2870:	e8 eb fa ff ff       	call   2360 <__isoc99_sscanf@plt>
    2875:	83 f8 01             	cmp    $0x1,%eax
    2878:	7e 1a                	jle    2894 <phase_3+0x4b>
    287a:	83 3c 24 07          	cmpl   $0x7,(%rsp)
    287e:	77 65                	ja     28e5 <phase_3+0x9c>
    2880:	8b 04 24             	mov    (%rsp),%eax
    2883:	48 8d 15 96 2a 00 00 	lea    0x2a96(%rip),%rdx        # 5320 <_IO_stdin_used+0x320>
    288a:	48 63 04 82          	movslq (%rdx,%rax,4),%rax
    288e:	48 01 d0             	add    %rdx,%rax
    2891:	3e ff e0             	notrack jmp *%rax
    2894:	e8 4a 07 00 00       	call   2fe3 <explode_bomb>
    2899:	eb df                	jmp    287a <phase_3+0x31>
    289b:	b8 f5 01 00 00       	mov    $0x1f5,%eax
    28a0:	39 44 24 04          	cmp    %eax,0x4(%rsp)
    28a4:	75 52                	jne    28f8 <phase_3+0xaf>
    28a6:	48 8b 44 24 08       	mov    0x8(%rsp),%rax
    28ab:	64 48 2b 04 25 28 00 	sub    %fs:0x28,%rax
    28b2:	00 00
    28b4:	75 49                	jne    28ff <phase_3+0xb6>
    28b6:	48 83 c4 18          	add    $0x18,%rsp
    28ba:	c3                   	ret
    28bb:	b8 3b 03 00 00       	mov    $0x33b,%eax
    28c0:	eb de                	jmp    28a0 <phase_3+0x57>
    28c2:	b8 83 03 00 00       	mov    $0x383,%eax
    28c7:	eb d7                	jmp    28a0 <phase_3+0x57>
    28c9:	b8 c0 03 00 00       	mov    $0x3c0,%eax
    28ce:	eb d0                	jmp    28a0 <phase_3+0x57>
    28d0:	b8 96 02 00 00       	mov    $0x296,%eax
    28d5:	eb c9                	jmp    28a0 <phase_3+0x57>
    28d7:	b8 fe 02 00 00       	mov    $0x2fe,%eax
    28dc:	eb c2                	jmp    28a0 <phase_3+0x57>
    28de:	b8 7e 02 00 00       	mov    $0x27e,%eax
    28e3:	eb bb                	jmp    28a0 <phase_3+0x57>
    28e5:	e8 f9 06 00 00       	call   2fe3 <explode_bomb>
    28ea:	b8 00 00 00 00       	mov    $0x0,%eax
    28ef:	eb af                	jmp    28a0 <phase_3+0x57>
    28f1:	b8 11 03 00 00       	mov    $0x311,%eax
    28f6:	eb a8                	jmp    28a0 <phase_3+0x57>
    28f8:	e8 e6 06 00 00       	call   2fe3 <explode_bomb>
    28fd:	eb a7                	jmp    28a6 <phase_3+0x5d>
    28ff:	e8 ac f9 ff ff       	call   22b0 <__stack_chk_fail@plt>
```

注意到 phase 3 开头调用了 `__isoc99_sscanf@plt` 函数读取输入，同时我们也知道函数调用传入的参数会被依次放在 `%rdi`、`%rsi`、`%rdx`、`%rcx` 等寄存器中。因此，我们可以在执行 `__isoc99_sscanf@plt` 的调用前，查看这些寄存器的值来推测输入的内容。

![](./assets/phase-3.png)

可见输入的格式为两个整数。参考 phase 2，猜测两个整数被存放在 `%rsp` 和 `0x4(%rsp)` 处（事实上确实如此，可通过调试验证）。

```asm
0000000000002849 <phase_3>:
    ; ...
    287a:	83 3c 24 07          	cmpl   $0x7,(%rsp)
    287e:	77 65                	ja     28e5 <phase_3+0x9c>
    ; ...
    28e5:	e8 f9 06 00 00       	call   2fe3 <explode_bomb>
    ; ...
```

输入的第一个整数必须小于等于 7，否则引爆炸弹。

```asm
0000000000002849 <phase_3>:
    ; ...
    2880:	8b 04 24             	mov    (%rsp),%eax
    2883:	48 8d 15 96 2a 00 00 	lea    0x2a96(%rip),%rdx        # 5320 <_IO_stdin_used+0x320>
    288a:	48 63 04 82          	movslq (%rdx,%rax,4),%rax
    288e:	48 01 d0             	add    %rdx,%rax
    2891:	3e ff e0             	notrack jmp *%rax
    2894:	e8 4a 07 00 00       	call   2fe3 <explode_bomb>
    ; ...
```

这段汇编代码依赖于输入的第一个整数跳转到不同的位置，类似于跳转表的结构。我们可以随机输入一个数字来看看跳转到了什么位置，进而只需满足对应分支的处理即可。我们以输入 `2` 为例，发现跳转到了 `28bb` 处。

```asm
0000000000002849 <phase_3>:
    ; ...
    28a0:	39 44 24 04          	cmp    %eax,0x4(%rsp)
    28a4:	75 52                	jne    28f8 <phase_3+0xaf>  ; if (input[1] != eax) explode_bomb()
    28a6:	48 8b 44 24 08       	mov    0x8(%rsp),%rax
    28ab:	64 48 2b 04 25 28 00 	sub    %fs:0x28,%rax
    28b2:	00 00
    28b4:	75 49                	jne    28ff <phase_3+0xb6>
    28b6:	48 83 c4 18          	add    $0x18,%rsp
    28ba:	c3                   	ret
    ; ...
    28bb:	b8 3b 03 00 00       	mov    $0x33b,%eax  ; eax = 0x33b
    28c0:	eb de                	jmp    28a0 <phase_3+0x57>
```

由此可知，输入的第二个整数必须等于 0x33b（即 827）。综上，phase 3 的答案之一为：

```text
2 827
```

## Phase 4

```asm
0000000000002904 <func4>:
    2904:	f3 0f 1e fa          	endbr64
    2908:	53                   	push   %rbx
    2909:	89 d0                	mov    %edx,%eax
    290b:	29 f0                	sub    %esi,%eax
    290d:	89 c3                	mov    %eax,%ebx
    290f:	c1 eb 1f             	shr    $0x1f,%ebx
    2912:	01 c3                	add    %eax,%ebx
    2914:	d1 fb                	sar    $1,%ebx
    2916:	01 f3                	add    %esi,%ebx
    2918:	39 fb                	cmp    %edi,%ebx
    291a:	7f 06                	jg     2922 <func4+0x1e>
    291c:	7c 10                	jl     292e <func4+0x2a>
    291e:	89 d8                	mov    %ebx,%eax
    2920:	5b                   	pop    %rbx
    2921:	c3                   	ret
    2922:	8d 53 ff             	lea    -0x1(%rbx),%edx
    2925:	e8 da ff ff ff       	call   2904 <func4>
    292a:	01 c3                	add    %eax,%ebx
    292c:	eb f0                	jmp    291e <func4+0x1a>
    292e:	8d 73 01             	lea    0x1(%rbx),%esi
    2931:	e8 ce ff ff ff       	call   2904 <func4>
    2936:	01 c3                	add    %eax,%ebx
    2938:	eb e4                	jmp    291e <func4+0x1a>

000000000000293a <phase_4>:
    293a:	f3 0f 1e fa          	endbr64
    293e:	48 83 ec 18          	sub    $0x18,%rsp
    2942:	64 48 8b 04 25 28 00 	mov    %fs:0x28,%rax
    2949:	00 00
    294b:	48 89 44 24 08       	mov    %rax,0x8(%rsp)
    2950:	31 c0                	xor    %eax,%eax
    2952:	48 8d 4c 24 04       	lea    0x4(%rsp),%rcx
    2957:	48 89 e2             	mov    %rsp,%rdx
    295a:	48 8d 35 07 2d 00 00 	lea    0x2d07(%rip),%rsi        # 5668 <array.0+0x328>
    2961:	e8 fa f9 ff ff       	call   2360 <__isoc99_sscanf@plt>
    2966:	83 f8 02             	cmp    $0x2,%eax
    2969:	75 0c                	jne    2977 <phase_4+0x3d>
    296b:	8b 04 24             	mov    (%rsp),%eax
    296e:	85 c0                	test   %eax,%eax
    2970:	78 05                	js     2977 <phase_4+0x3d>
    2972:	83 f8 0e             	cmp    $0xe,%eax
    2975:	7e 05                	jle    297c <phase_4+0x42>
    2977:	e8 67 06 00 00       	call   2fe3 <explode_bomb>
    297c:	ba 0e 00 00 00       	mov    $0xe,%edx
    2981:	be 00 00 00 00       	mov    $0x0,%esi
    2986:	8b 3c 24             	mov    (%rsp),%edi
    2989:	e8 76 ff ff ff       	call   2904 <func4>
    298e:	83 f8 0b             	cmp    $0xb,%eax
    2991:	75 07                	jne    299a <phase_4+0x60>
    2993:	83 7c 24 04 0b       	cmpl   $0xb,0x4(%rsp)
    2998:	74 05                	je     299f <phase_4+0x65>
    299a:	e8 44 06 00 00       	call   2fe3 <explode_bomb>
    299f:	48 8b 44 24 08       	mov    0x8(%rsp),%rax
    29a4:	64 48 2b 04 25 28 00 	sub    %fs:0x28,%rax
    29ab:	00 00
    29ad:	75 05                	jne    29b4 <phase_4+0x7a>
    29af:	48 83 c4 18          	add    $0x18,%rsp
    29b3:	c3                   	ret
    29b4:	e8 f7 f8 ff ff       	call   22b0 <__stack_chk_fail@plt>
```

首先，我们注意到了熟悉的函数调用 `__isoc99_sscanf@plt`，同样地，我们可以在调用前查看相关寄存器的值来推测输入格式，发现为输入是两个整数。

```asm
000000000000293a <phase_4>:
    ; ...
    2966:	83 f8 02             	cmp    $0x2,%eax
    2969:	75 0c                	jne    2977 <phase_4+0x3d>  ; if (num_inputs != 2) explode_bomb()
    296b:	8b 04 24             	mov    (%rsp),%eax  ; eax = input[0]
    296e:	85 c0                	test   %eax,%eax
    2970:	78 05                	js     2977 <phase_4+0x3d>  ; if (eax < 0) explode_bomb()
    2972:	83 f8 0e             	cmp    $0xe,%eax
    2975:	7e 05                	jle    297c <phase_4+0x42>  ; if (eax > 14) explode_bomb()
    2977:	e8 67 06 00 00       	call   2fe3 <explode_bomb>
    297c:	ba 0e 00 00 00       	mov    $0xe,%edx  ; edx = 14
    2981:	be 00 00 00 00       	mov    $0x0,%esi  ; esi = 0
    2986:	8b 3c 24             	mov    (%rsp),%edi  ; edi = input[0]
    2989:	e8 76 ff ff ff       	call   2904 <func4>  ; eax = func4(edi, esi, edx)
    298e:	83 f8 0b             	cmp    $0xb,%eax  ; if (eax != 11) explode_bomb()
    2991:	75 07                	jne    299a <phase_4+0x60>
    2993:	83 7c 24 04 0b       	cmpl   $0xb,0x4(%rsp)  ; if (input[1] != 11) explode_bomb()
    2998:	74 05                	je     299f <phase_4+0x65>
    299a:	e8 44 06 00 00       	call   2fe3 <explode_bomb>
    ; ...
```

分析发现，phase 4 对输入的要求如下：

1. 输入的第一个整数必须在 0 到 14 之间
2. 计算 `func4(input[0], 0, 14)` 的结果必须为 11
3. 输入的第二个整数必须为 11

接着使用伟大的枚举法，枚举 `input[0]` 从 1 到 13 的值，得到 phase 4 的答案为：

```text
0 11
```

## Phase 5

```asm
00000000000029b9 <phase_5>:
    29b9:	f3 0f 1e fa          	endbr64
    29bd:	53                   	push   %rbx
    29be:	48 89 fb             	mov    %rdi,%rbx
    29c1:	e8 f0 02 00 00       	call   2cb6 <string_length>
    29c6:	83 f8 04             	cmp    $0x4,%eax
    29c9:	75 0c                	jne    29d7 <phase_5+0x1e>
    29cb:	b9 01 00 00 00       	mov    $0x1,%ecx
    29d0:	b8 00 00 00 00       	mov    $0x0,%eax
    29d5:	eb 1f                	jmp    29f6 <phase_5+0x3d>
    29d7:	e8 07 06 00 00       	call   2fe3 <explode_bomb>
    29dc:	eb ed                	jmp    29cb <phase_5+0x12>
    29de:	48 63 d0             	movslq %eax,%rdx
    29e1:	0f b6 14 13          	movzbl (%rbx,%rdx,1),%edx
    29e5:	83 e2 07             	and    $0x7,%edx
    29e8:	48 8d 35 51 29 00 00 	lea    0x2951(%rip),%rsi        # 5340 <array.0>
    29ef:	0f af 0c 96          	imul   (%rsi,%rdx,4),%ecx
    29f3:	83 c0 01             	add    $0x1,%eax
    29f6:	83 f8 03             	cmp    $0x3,%eax
    29f9:	7e e3                	jle    29de <phase_5+0x25>
    29fb:	81 f9 c0 00 00 00    	cmp    $0xc0,%ecx
    2a01:	75 02                	jne    2a05 <phase_5+0x4c>
    2a03:	5b                   	pop    %rbx
    2a04:	c3                   	ret
    2a05:	e8 d9 05 00 00       	call   2fe3 <explode_bomb>
    2a0a:	eb f7                	jmp    2a03 <phase_5+0x4a>
```

phase 5 开头调用了 `string_length`，猜测输入要求是一个字符串。经过简单调试可以发现，输入的字符串储存在 `[%rdi]` 处，之后该值由 `%rbx` 保存，`string_length` 的返回值 `%rax` 代表字符串的长度。

```asm
00000000000029b9 <phase_5>:
    ; ...
    29bd:	53                   	push   %rbx
    29be:	48 89 fb             	mov    %rdi,%rbx ; *rbx = inputs
    29c1:	e8 f0 02 00 00       	call   2cb6 <string_length>
    29c6:	83 f8 04             	cmp    $0x4,%eax
    29c9:	75 0c                	jne    29d7 <phase_5+0x1e>  ; if (length != 4) explode_bomb()
    29cb:	b9 01 00 00 00       	mov    $0x1,%ecx  ; ecx = 1
    29d0:	b8 00 00 00 00       	mov    $0x0,%eax  ; eax = 0
    29d5:	eb 1f                	jmp    29f6 <phase_5+0x3d>
    29d7:	e8 07 06 00 00       	call   2fe3 <explode_bomb>
    29dc:	eb ed                	jmp    29cb <phase_5+0x12>
    29de:	48 63 d0             	movslq %eax,%rdx  ; loop rdx = eax
    29e1:	0f b6 14 13          	movzbl (%rbx,%rdx,1),%edx  ; edx = inputs[rdx]
    29e5:	83 e2 07             	and    $0x7,%edx  ; edx = edx & 0x7
    29e8:	48 8d 35 51 29 00 00 	lea    0x2951(%rip),%rsi        # 5340 <array.0>
    29ef:	0f af 0c 96          	imul   (%rsi,%rdx,4),%ecx  ; ecx = ecx * array[edx]
    29f3:	83 c0 01             	add    $0x1,%eax  ; eax++
    29f6:	83 f8 03             	cmp    $0x3,%eax
    29f9:	7e e3                	jle    29de <phase_5+0x25>  ; loop while (eax <= 3)
    29fb:	81 f9 c0 00 00 00    	cmp    $0xc0,%ecx
    2a01:	75 02                	jne    2a05 <phase_5+0x4c>  ; if (ecx != 0xc0) explode_bomb()
    2a03:	5b                   	pop    %rbx
    2a04:	c3                   	ret
    2a05:	e8 d9 05 00 00       	call   2fe3 <explode_bomb>
    2a0a:	eb f7                	jmp    2a03 <phase_5+0x4a>
```

梳理一下程序逻辑：

1. 输入的字符串长度必须为 4
2. 初始化 `ecx = 1`，`eax = 0`
3. 对字符串的每个字符（共 4 个），执行以下操作：
   1. 取字符的 ASCII 码与 0x7 做与运算，结果作为索引从数组 `array.0` 中取值
   2. 将取出的值乘到 `ecx` 上
   3. `eax` 自增 1
4. 最终 `ecx` 必须等于 0xc0（即 192）
   根据上述逻辑，接下来我们需要查看 `array.0` 的内容。让程序运行到 `29ef` 处，此时 `array.0` 的地址被储存在 `%rsi` 中，调用 `x/10wd $rsi` 以四字节为单位查看数组的前十个元素的十进制值。

![](./assets/phase-5.png)

可见 `array.0` 由 8 个 int 类整数构成，接下来的事情就简单了，我们只需用数组中的数凑出乘积为 192 的一种组合，倒推出满足条件的四个字符即可。经过计算，phase 5 的答案之一为：

```text
hadd
```

## Phase 6

```asm
0000000000002a0c <phase_6>:
    2a0c:	f3 0f 1e fa          	endbr64
    2a10:	41 54                	push   %r12
    2a12:	55                   	push   %rbp
    2a13:	53                   	push   %rbx
    2a14:	48 83 ec 60          	sub    $0x60,%rsp
    2a18:	64 48 8b 04 25 28 00 	mov    %fs:0x28,%rax
    2a1f:	00 00
    2a21:	48 89 44 24 58       	mov    %rax,0x58(%rsp)
    2a26:	31 c0                	xor    %eax,%eax
    2a28:	48 89 e6             	mov    %rsp,%rsi
    2a2b:	e8 39 06 00 00       	call   3069 <read_six_numbers>
    2a30:	bd 00 00 00 00       	mov    $0x0,%ebp
    2a35:	eb 27                	jmp    2a5e <phase_6+0x52>
    2a37:	e8 a7 05 00 00       	call   2fe3 <explode_bomb>
    2a3c:	eb 33                	jmp    2a71 <phase_6+0x65>
    2a3e:	83 c3 01             	add    $0x1,%ebx
    2a41:	83 fb 05             	cmp    $0x5,%ebx
    2a44:	7f 15                	jg     2a5b <phase_6+0x4f>
    2a46:	48 63 c5             	movslq %ebp,%rax
    2a49:	48 63 d3             	movslq %ebx,%rdx
    2a4c:	8b 3c 94             	mov    (%rsp,%rdx,4),%edi
    2a4f:	39 3c 84             	cmp    %edi,(%rsp,%rax,4)
    2a52:	75 ea                	jne    2a3e <phase_6+0x32>
    2a54:	e8 8a 05 00 00       	call   2fe3 <explode_bomb>
    2a59:	eb e3                	jmp    2a3e <phase_6+0x32>
    2a5b:	44 89 e5             	mov    %r12d,%ebp
    2a5e:	83 fd 05             	cmp    $0x5,%ebp
    2a61:	7f 17                	jg     2a7a <phase_6+0x6e>
    2a63:	48 63 c5             	movslq %ebp,%rax
    2a66:	8b 04 84             	mov    (%rsp,%rax,4),%eax
    2a69:	83 e8 01             	sub    $0x1,%eax
    2a6c:	83 f8 05             	cmp    $0x5,%eax
    2a6f:	77 c6                	ja     2a37 <phase_6+0x2b>
    2a71:	44 8d 65 01          	lea    0x1(%rbp),%r12d
    2a75:	44 89 e3             	mov    %r12d,%ebx
    2a78:	eb c7                	jmp    2a41 <phase_6+0x35>
    2a7a:	b8 00 00 00 00       	mov    $0x0,%eax
    2a7f:	eb 11                	jmp    2a92 <phase_6+0x86>
    2a81:	48 63 c8             	movslq %eax,%rcx
    2a84:	ba 07 00 00 00       	mov    $0x7,%edx
    2a89:	2b 14 8c             	sub    (%rsp,%rcx,4),%edx
    2a8c:	89 14 8c             	mov    %edx,(%rsp,%rcx,4)
    2a8f:	83 c0 01             	add    $0x1,%eax
    2a92:	83 f8 05             	cmp    $0x5,%eax
    2a95:	7e ea                	jle    2a81 <phase_6+0x75>
    2a97:	be 00 00 00 00       	mov    $0x0,%esi
    2a9c:	eb 17                	jmp    2ab5 <phase_6+0xa9>
    2a9e:	48 8b 52 08          	mov    0x8(%rdx),%rdx
    2aa2:	83 c0 01             	add    $0x1,%eax
    2aa5:	48 63 ce             	movslq %esi,%rcx
    2aa8:	39 04 8c             	cmp    %eax,(%rsp,%rcx,4)
    2aab:	7f f1                	jg     2a9e <phase_6+0x92>
    2aad:	48 89 54 cc 20       	mov    %rdx,0x20(%rsp,%rcx,8)
    2ab2:	83 c6 01             	add    $0x1,%esi
    2ab5:	83 fe 05             	cmp    $0x5,%esi
    2ab8:	7f 0e                	jg     2ac8 <phase_6+0xbc>
    2aba:	b8 01 00 00 00       	mov    $0x1,%eax
    2abf:	48 8d 15 4a 66 00 00 	lea    0x664a(%rip),%rdx        # 9110 <node1>
    2ac6:	eb dd                	jmp    2aa5 <phase_6+0x99>
    2ac8:	48 8b 5c 24 20       	mov    0x20(%rsp),%rbx
    2acd:	48 89 d9             	mov    %rbx,%rcx
    2ad0:	b8 01 00 00 00       	mov    $0x1,%eax
    2ad5:	eb 12                	jmp    2ae9 <phase_6+0xdd>
    2ad7:	48 63 d0             	movslq %eax,%rdx
    2ada:	48 8b 54 d4 20       	mov    0x20(%rsp,%rdx,8),%rdx
    2adf:	48 89 51 08          	mov    %rdx,0x8(%rcx)
    2ae3:	83 c0 01             	add    $0x1,%eax
    2ae6:	48 89 d1             	mov    %rdx,%rcx
    2ae9:	83 f8 05             	cmp    $0x5,%eax
    2aec:	7e e9                	jle    2ad7 <phase_6+0xcb>
    2aee:	48 c7 41 08 00 00 00 	movq   $0x0,0x8(%rcx)
    2af5:	00
    2af6:	bd 00 00 00 00       	mov    $0x0,%ebp
    2afb:	eb 07                	jmp    2b04 <phase_6+0xf8>
    2afd:	48 8b 5b 08          	mov    0x8(%rbx),%rbx
    2b01:	83 c5 01             	add    $0x1,%ebp
    2b04:	83 fd 04             	cmp    $0x4,%ebp
    2b07:	7f 11                	jg     2b1a <phase_6+0x10e>
    2b09:	48 8b 43 08          	mov    0x8(%rbx),%rax
    2b0d:	8b 00                	mov    (%rax),%eax
    2b0f:	39 03                	cmp    %eax,(%rbx)
    2b11:	7e ea                	jle    2afd <phase_6+0xf1>
    2b13:	e8 cb 04 00 00       	call   2fe3 <explode_bomb>
    2b18:	eb e3                	jmp    2afd <phase_6+0xf1>
    2b1a:	48 8b 44 24 58       	mov    0x58(%rsp),%rax
    2b1f:	64 48 2b 04 25 28 00 	sub    %fs:0x28,%rax
    2b26:	00 00
    2b28:	75 09                	jne    2b33 <phase_6+0x127>
    2b2a:	48 83 c4 60          	add    $0x60,%rsp
    2b2e:	5b                   	pop    %rbx
    2b2f:	5d                   	pop    %rbp
    2b30:	41 5c                	pop    %r12
    2b32:	c3                   	ret
    2b33:	e8 78 f7 ff ff       	call   22b0 <__stack_chk_fail@plt>
```

相较于前几个 phase，phase 6 的代码量显著增加，分析难度也有所提升。首先我们注意到 `read_six_numbers` 函数的调用，猜测输入格式为六个整数，有了前几个 phase 的经验，六个整数应该依次储存在 `[%rsp]` 的位置（调试验证确实如此）。

由于汇编代码过长，一个可选的方案是对代码的主干部分进行人工反汇编，并将一些跳转逻辑转换成条件分支或循环，呈现为伪代码形式：

```c
int main() {
    rsi = rsp;
    rbp = 0;

    // 第一阶段：检查输入合法性（无重复且1-6）
    while (rbp <= 5) {
        rax = rbp;
        rax = *(rsp + rax * 4);
        rax -= 1;

        if (0 <= rax <= 5) {  // 注意 2ab8 处的 ja
            r12 = rbp + 1;
            rbx = r12;
            while (rbx <= 5) {
                rax = rbp;
                rdx = rbx;
                rdi = *(rsp + rdx * 4);
                if (*(rsp + rax * 4) == rdi) explode_bomb();
                rbx += 1;
            }
            rbp = r12;
        }
        else explode_bomb();
    }

    // 第二阶段：转换输入值（7 - 原输入）
    rax = 0;
    while (rax <= 5) {
        rcx = rax;
        rdx = 7;
        rdx -= *(rsp + rcx * 4);  // 7 - 原输入值
        *(rsp + rcx * 4) = rdx;
        rax += 1;
    }

    // 第三阶段：存储链表节点地址（基于转换后的值，第i个节点）
    rsi = 0;
    while (rsi <= 5) {
        rax = 1;
        rdx = rip + 0x664a;  // 调试时可查看，发现是链表节点的起始地址
        rcx = rsi;  // index
        while (*(rsp + rcx * 4) > rax) {
            rdx = *(rdx + 0x8);  // 移动到下一个节点
            rax += 1;
            rcx = rsi;
        }
        *(rsp + 0x20 + rcx * 8) = rdx;  // 存储节点地址
        rsi += 1;
    }

    // 第四阶段：重新构建链表
    rbx = *(rsp + 0x20);  // 第一个储存的节点地址
    rcx = rbx;
    rax = 1;
    while (rax <= 5) {
        rdx = rax;
        rdx = *(rsp + 0x20 + rdx * 8);
        *(rcx + 0x8) = rdx;
        rax += 1;
        rcx = rdx;  // 新节点地址
    }

    // 第五阶段：校验链表升序
    rbp = 0;
    while (rbp <= 4) {
        rax = *(rbx + 0x8);  // next 节点地址
        rax = *rax;  // next 节点的值
        // 比较当前节点值与 next 节点值
        if (*rbx > eax) {  // 注意 2b0f 处，四字节比较
            explode_bomb();
        }
        rbx = *(rbx + 0x8);
        rbp += 1;
    }

    return 0;
}
```

为了能够理解代码逻辑，我们需要事先知道 `[%rip + 0x664a]` 处的内容（虽然由 node 可猜测为链表节点）。

```bash
# 在 lea 0x657b (%rip), %rdx 的下一行设置断点，从而查看更新后的 %rdx
b *(phase_6+186)
c
x/24wx $rdx
```

得到输出：

```bash
(gdb)x/24wx $rdx
0x55555555d110 <node1>: 0x0000004e  0x00000001  0x5555d120  0x00005555
0x55555555d120 <node2>: 0x000002e8  0x00000002  0x5555d130  0x00005555
0x55555555d130 <node3>: 0x0000033b  0x00000003  0x5555d140  0x00005555
0x55555555d140 <node4>: 0x000003bb  0x00000004  0x5555d150  0x00005555
0x55555555d150 <node5>: 0x00000283  0x00000005  0x5555c080  0x00005555
0x55555555d160 <host_table>: 0x555596b2 0x00005555 0x555596bb 0x00005555
(gdb)x/4wx 0x000055555555c080
0x55555555c080 <node6>: 0x00000326  0x00000006  0x00000000  0x00000000
```

结合小端法和汇编代码，推测 node 的结构如下：

```c
struct node {
    int value;          // 节点值
    int index;          // 节点索引
    struct node *next;  // 指向下一个节点的指针
};
```

现在我们知道了 node 的值，结合代码逻辑，可以反推 phase 6 的答案了：

1. 将 node 按照值从小到大排序，得到顺序为：`node1` -> `node5` -> `node2` -> `node6` -> `node3` -> `node4`
2. 取出对应的索引，得到索引顺序为：`1 5 2 6 3 4`
3. 进行 `7 - i` 操作，得到 phase 6 的答案为：

```text
6 2 5 1 4 3
```

## Secret Phase

如果仔细查看汇编代码 `phase_defused` 的部分，会发现其中有三个函数的调用：

- `abracadabra`
- `alohomora`
- `secret_phase`

想要破解 secret phase，我们需要先从 `phase_defused` 入手。

### phase_defused

1. 函数起始，执行`endbr64`安全指令，随后将`%rbx`寄存器入栈保存
2. 将第一个参数（`%rdi`）的值存入`%rbx`寄存器，保存该参数指针
3. 向`%rdi`指向的内存地址写入`0x0`（初始化该地址的值为 0）
4. 设置参数：将`%rdi`的值赋给`%rsi`（第二个参数），将`0x1`赋给`%edi`（第一个参数），调用`send_msg`函数发送消息

```asm
00000000000031ec <phase_defused>:
    31ec:	f3 0f 1e fa          	endbr64
    31f0:	53                   	push   %rbx
    31f1:	48 89 fb             	mov    %rdi,%rbx
    31f4:	c7 07 00 00 00 00    	movl   $0x0,(%rdi)
    31fa:	48 89 fe             	mov    %rdi,%rsi
    31fd:	bf 01 00 00 00       	mov    $0x1,%edi
    3202:	e8 7a fc ff ff       	call   2e81 <send_msg>
    ; ...
```

5. 检查`%rbx`指向的内存地址的值是否等于`0x1`：
   - 若不等于（`jne`），跳转到步骤 6
   - 若等于，继续步骤 7
6. 打印一些字符串，并调用`exit`函数退出程序
7. 检查`num_input_strings`（内存地址`0x63c5(%rip)`）的值是否等于`0x6`：
   - 若不等于，执行`pop %rbx`和`ret`指令，函数返回
   - 若等于（`je`），跳转到步骤 8

```asm
00000000000031ec <phase_defused>:
    ; ...
    3207:	83 3b 01             	cmpl   $0x1,(%rbx)
    320a:	75 0b                	jne    3217 <phase_defused+0x2b>
    320c:	83 3d c5 63 00 00 06 	cmpl   $0x6,0x63c5(%rip)        # 95d8 <num_input_strings>
    3213:	74 22                	je     3237 <phase_defused+0x4b>
    3215:	5b                   	pop    %rbx
    3216:	c3                   	ret
    3217:	48 8d 35 da 21 00 00 	lea    0x21da(%rip),%rsi        # 53f8 <array.0+0xb8>
    321e:	bf 01 00 00 00       	mov    $0x1,%edi
    3223:	b8 00 00 00 00       	mov    $0x0,%eax
    3228:	e8 53 f1 ff ff       	call   2380 <__printf_chk@plt>
    322d:	bf 08 00 00 00       	mov    $0x8,%edi
    3232:	e8 79 f1 ff ff       	call   23b0 <exit@plt>
    ; ...
```

8. 调用`abracadabra`函数，检查其返回值（`%eax`）：
   - 若返回值为 0，打印一些字符串，随后跳转到步骤 7 中"不等于时"的返回逻辑
   - 若返回值不为 0（`jne`），跳转到步骤 9
9. 调用`alohomora`函数，检查其返回值（`%eax`）：
   - 若返回值为 0（`je`），跳转到步骤 10
   - 若返回值不为 0，打印一些字符串，随后调用`secret_phase`函数，再跳转到步骤 8 中"返回值为 0 时"的逻辑
10. 打印一些字符串，随后跳转到步骤 8 中"返回值为 0 时"的打印逻辑

```asm
00000000000031ec <phase_defused>:
    ; ...
    3237:	e8 48 f4 ff ff       	call   2684 <abracadabra>
    323c:	85 c0                	test   %eax,%eax
    323e:	75 1a                	jne    325a <phase_defused+0x6e>
    3240:	48 8d 3d 11 23 00 00 	lea    0x2311(%rip),%rdi        # 5558 <array.0+0x218>
    3247:	e8 34 f0 ff ff       	call   2280 <puts@plt>
    324c:	48 8d 3d 4d 23 00 00 	lea    0x234d(%rip),%rdi        # 55a0 <array.0+0x260>
    3253:	e8 28 f0 ff ff       	call   2280 <puts@plt>
    3258:	eb bb                	jmp    3215 <phase_defused+0x29>
    325a:	e8 b2 f4 ff ff       	call   2711 <alohomora>
    325f:	85 c0                	test   %eax,%eax
    3261:	74 30                	je     3293 <phase_defused+0xa7>
    3263:	48 8d 3d fe 21 00 00 	lea    0x21fe(%rip),%rdi        # 5468 <array.0+0x128>
    326a:	e8 11 f0 ff ff       	call   2280 <puts@plt>
    326f:	48 8d 3d 1a 22 00 00 	lea    0x221a(%rip),%rdi        # 5490 <array.0+0x150>
    3276:	e8 05 f0 ff ff       	call   2280 <puts@plt>
    327b:	48 8d 3d 46 22 00 00 	lea    0x2246(%rip),%rdi        # 54c8 <array.0+0x188>
    3282:	e8 f9 ef ff ff       	call   2280 <puts@plt>
    3287:	b8 00 00 00 00       	mov    $0x0,%eax
    328c:	e8 f7 f8 ff ff       	call   2b88 <secret_phase>
    3291:	eb ad                	jmp    3240 <phase_defused+0x54>
    3293:	48 8d 3d 7e 22 00 00 	lea    0x227e(%rip),%rdi        # 5518 <array.0+0x1d8>
    329a:	e8 e1 ef ff ff       	call   2280 <puts@plt>
    329f:	48 8d 3d 22 22 00 00 	lea    0x2222(%rip),%rdi        # 54c8 <array.0+0x188>
    32a6:	e8 d5 ef ff ff       	call   2280 <puts@plt>
    32ab:	eb 93                	jmp    3240 <phase_defused+0x54>
```

由于我们的前六个 phase 都已顺利破解，并没有调用 `exit`，因此开启 secret_phase 的关键在于步骤 7。步骤 7 要求 `num_input_strings` 的值为 6，而这个值正是我们在前面的 phase 2 和 phase 6 中输入的数字数量。可见 secret phase 的只有可能在 phase 2 和 phase 6 之后触发。

### abracadabra

1. 函数起始，执行`endbr64`安全指令，随后在栈上分配`0x98`字节空间
2. 从`%fs:0x28`（栈金丝雀存储位置）读取值到`%rax`，并将其保存到栈上`0x88(%rsp)`位置
3. 将`%eax`清零，准备后续操作

```asm
0000000000002684 <abracadabra>:
    2684:	f3 0f 1e fa          	endbr64
    2688:	48 81 ec 98 00 00 00 	sub    $0x98,%rsp
    268f:	64 48 8b 04 25 28 00 	mov    %fs:0x28,%rax
    2696:	00 00
    2698:	48 89 84 24 88 00 00 	mov    %rax,0x88(%rsp)
    269f:	00
    26a0:	31 c0                	xor    %eax,%eax
    ; ...
```

4. 加载地址到寄存器，设置`sscanf`参数：
   - `%rcx` = `0xc(%rsp)`（第三个输出参数地址）
   - `%rdx` = `0x8(%rsp)`（第二个输出参数地址）
   - `%r8` = `0x10(%rsp)`（第一个输出参数地址）
   - `%rsi` = 格式字符串地址（`0x2aa3(%rip)`，即`515b`）
   - `%rdi` = 输入字符串地址（`0x7089(%rip)`，即`9748`，属于`input_strings`）
5. 调用`__isoc99_sscanf`函数，解析输入字符串并将结果存入栈上`0x8`、`0xc`、`0x10`位置

   > [!TIP]
   > 通过调试发现，调用 `__isoc99_sscanf` 前，`%rsi` 处储存的格式字符串为`"%d %d %s"`，而 `%rdi` 处储存的输入字符串为正是 phase 4 的输入。

```asm
0000000000002684 <abracadabra>:
    ; ...
    26a2:	48 8d 4c 24 0c       	lea    0xc(%rsp),%rcx
    26a7:	48 8d 54 24 08       	lea    0x8(%rsp),%rdx
    26ac:	4c 8d 44 24 10       	lea    0x10(%rsp),%r8
    26b1:	48 8d 35 a3 2a 00 00 	lea    0x2aa3(%rip),%rsi        # 515b <_IO_stdin_used+0x15b>
    26b8:	48 8d 3d 89 70 00 00 	lea    0x7089(%rip),%rdi        # 9748 <input_strings+0x168>
    26bf:	e8 9c fc ff ff       	call   2360 <__isoc99_sscanf@plt>
    ; ...
```

6. 检查`sscanf`返回值（`%eax`）是否等于`3`（成功匹配 3 个值）：
   - 若等于`3`（`je`），跳至步骤 7
   - 若不等于`3`，设置`%eax`为`0`，跳至步骤 9
7. 设置参数调用`strings_not_equal`函数：

   - `%rdi` = 栈上`0x10`位置（第三个解析结果）
   - `%rsi` = 目标字符串地址（`0x2a73(%rip)`，即`5168`）

   > [!TIP]
   > 调试可知，`0x2a73(%rip)` 处的字符串为`"...VeniVidiViciTwoThousandYearsAgo?"`。

8. 检查`strings_not_equal`返回值（`%eax`）：
   - 若返回`0`（字符串相等，`je`），设置`%eax`为`1`，跳至步骤 9
   - 若返回非`0`（字符串不相等），设置`%eax`为`0`，跳至步骤 9
9. 进行栈溢出检查：
   - 从栈上`0x88(%rsp)`取出之前保存的栈金丝雀，与`%fs:0x28`原值比较
   - 若不等（`jne`），调用`__stack_chk_fail`处理栈溢出
   - 若相等，恢复栈空间（`add $0x98,%rsp`）并返回（`ret`）

```asm
0000000000002684 <abracadabra>:
    ; ...
    26c4:	83 f8 03             	cmp    $0x3,%eax
    26c7:	74 20                	je     26e9 <abracadabra+0x65>
    26c9:	b8 00 00 00 00       	mov    $0x0,%eax
    26ce:	48 8b 94 24 88 00 00 	mov    0x88(%rsp),%rdx
    26d5:	00
    26d6:	64 48 2b 14 25 28 00 	sub    %fs:0x28,%rdx
    26dd:	00 00
    26df:	75 2b                	jne    270c <abracadabra+0x88>
    26e1:	48 81 c4 98 00 00 00 	add    $0x98,%rsp
    26e8:	c3                   	ret
    26e9:	48 8d 7c 24 10       	lea    0x10(%rsp),%rdi
    26ee:	48 8d 35 73 2a 00 00 	lea    0x2a73(%rip),%rsi        # 5168 <_IO_stdin_used+0x168>
    26f5:	e8 d4 05 00 00       	call   2cce <strings_not_equal>
    26fa:	85 c0                	test   %eax,%eax
    26fc:	74 07                	je     2705 <abracadabra+0x81>
    26fe:	b8 00 00 00 00       	mov    $0x0,%eax
    2703:	eb c9                	jmp    26ce <abracadabra+0x4a>
    2705:	b8 01 00 00 00       	mov    $0x1,%eax
    270a:	eb c2                	jmp    26ce <abracadabra+0x4a>
    270c:	e8 9f fb ff ff       	call   22b0 <__stack_chk_fail@plt>
```

因此，我们需要在 phase 4 的输入的两个数字后，添加一个字符串：

```text
...VeniVidiViciTwoThousandYearsAgo?
```

### alohomora

1. 函数起始，执行`endbr64`安全指令，随后在栈上分配`0x88`字节空间
2. 从`%fs:0x28`（栈金丝雀存储位置）读取值到`%rax`，并将其保存到栈上`0x78(%rsp)`位置
3. 将`%eax`清零（`xor %eax,%eax`），准备后续操作

```asm
0000000000002711 <alohomora>:
    2711:	f3 0f 1e fa          	endbr64
    2715:	48 81 ec 88 00 00 00 	sub    $0x88,%rsp
    271c:	64 48 8b 04 25 28 00 	mov    %fs:0x28,%rax
    2723:	00 00
    2725:	48 89 44 24 78       	mov    %rax,0x78(%rsp)
    272a:	31 c0                	xor    %eax,%eax
    ; ...
```

4. 计算输入字符串地址：通过`lea 0x6f25(%rip),%rax`获取`input_strings+0x78`，并跳转至步骤 5

   > [!TIP]
   > 通过调试发现，`%rax` 处储存的字符串为正是 phase 2 的输入。

5. 遍历字符串寻找结束符：
   - 检查`%rax`指向的字节是否为`0x0`（字符串结束符）
   - 若不是（`jne`），将`%rax`加`1`（移动到下一个字符），重复检查
   - 若是，将`%rax`减`1`（回退到最后一个有效字符）

```asm
0000000000002711 <alohomora>:
    ; ...
    272c:	48 8d 05 25 6f 00 00 	lea    0x6f25(%rip),%rax        # 9658 <input_strings+0x78>
    2733:	eb 04                	jmp    2739 <alohomora+0x28>
    2735:	48 83 c0 01          	add    $0x1,%rax
    2739:	80 38 00             	cmpb   $0x0,(%rax)
    273c:	75 f7                	jne    2735 <alohomora+0x24>
    273e:	48 83 e8 01          	sub    $0x1,%rax
    ; ...
```

6. 初始化目标缓冲区指针：将栈顶地址（`%rsp`）赋给`%rdx`（作为新字符串的存储起始位置），跳转至步骤 7
7. 反向复制字符串（从原字符串末尾向起始位置）：
   - 读取`%rax`指向的字符到`%ecx`（`movzbl (%rax),%ecx`）
   - 检查该字符是否为空格（`0x20`）：
     - 若是（`je`），跳转至步骤 8（结束复制）
     - 若不是，检查`%rax`是否未到达原字符串起始地址（`%rsi`）：
       - 若未到达（`jne`），将字符存入`%rdx`指向的栈缓冲区，`%rdx`加`1`（移动缓冲区指针），`%rax`减`1`（移动原字符串指针到前一个字符），重复步骤 7
       - 若已到达，跳转至步骤 8
8. 为新字符串添加结束符：在`%rdx`指向的位置写入`0x0`（`movb $0x0,(%rdx)`）

```asm
0000000000002711 <alohomora>:
    ; ...
    2742:	48 89 e2             	mov    %rsp,%rdx
    2745:	eb 0a                	jmp    2751 <alohomora+0x40>
    2747:	88 0a                	mov    %cl,(%rdx)
    2749:	48 83 c2 01          	add    $0x1,%rdx
    274d:	48 83 e8 01          	sub    $0x1,%rax
    2751:	0f b6 08             	movzbl (%rax),%ecx
    2754:	80 f9 20             	cmp    $0x20,%cl
    2757:	74 0c                	je     2765 <alohomora+0x54>
    2759:	48 8d 35 f8 6e 00 00 	lea    0x6ef8(%rip),%rsi        # 9658 <input_strings+0x78>
    2760:	48 39 f0             	cmp    %rsi,%rax
    2763:	75 e2                	jne    2747 <alohomora+0x36>
    2765:	c6 02 00             	movb   $0x0,(%rdx)
    ; ...
```

9. 比较字符串：调用`strings_not_equal`函数，比较栈上的新字符串（`%rdi=%rsp`）与目标字符串（`%rsi=0x2a1e(%rip)`）
   > [!TIP]
   > 调试可知，`0x2a1e(%rip)` 处的目标字符串为`"...diaSecnOraseaCsuiluJsuiaGtahTwonKUoD"`。
10. 根据比较结果设置返回值：
    - 若字符串相等（`strings_not_equal`返回`0`，`je`），设置`%eax`为`1`
    - 若字符串不相等，设置`%eax`为`0`
11. 进行栈溢出检查：
    - 从栈上`0x78(%rsp)`取出之前保存的栈金丝雀，与`%fs:0x28`原值比较
    - 若不等（`jne`），调用`__stack_chk_fail`处理栈溢出
    - 若相等，恢复栈空间（`add $0x88,%rsp`）并返回（`ret`）

```asm
0000000000002711 <alohomora>:
    ; ...
    2768:	48 89 e7             	mov    %rsp,%rdi
    276b:	48 8d 35 1e 2a 00 00 	lea    0x2a1e(%rip),%rsi        # 5190 <_IO_stdin_used+0x190>
    2772:	e8 57 05 00 00       	call   2cce <strings_not_equal>
    2777:	85 c0                	test   %eax,%eax
    2779:	74 1d                	je     2798 <alohomora+0x87>
    277b:	b8 00 00 00 00       	mov    $0x0,%eax
    2780:	48 8b 54 24 78       	mov    0x78(%rsp),%rdx
    2785:	64 48 2b 14 25 28 00 	sub    %fs:0x28,%rdx
    278c:	00 00
    278e:	75 0f                	jne    279f <alohomora+0x8e>
    2790:	48 81 c4 88 00 00 00 	add    $0x88,%rsp
    2797:	c3                   	ret
    2798:	b8 01 00 00 00       	mov    $0x1,%eax
    279d:	eb e1                	jmp    2780 <alohomora+0x6f>
    279f:	e8 0c fb ff ff       	call   22b0 <__stack_chk_fail@plt>
```

因此，我们需要在 phase 2 的输入中，添加一个字符串：

```text
DoUKnowThatGaiusJuliusCaesarOnceSaid...
```

### secret_phase

1. 函数起始，执行`endbr64`安全指令，随后将`%rbx`寄存器入栈保存，并在栈上分配`0x10`字节空间
2. 从`%fs:0x28`（栈金丝雀存储位置）读取值到`%rax`，并将其保存到栈上`0x8(%rsp)`位置
3. 将`%eax`清零（`xor %eax,%eax`），准备后续操作

```asm
0000000000002b88 <secret_phase>:
    2b88:	f3 0f 1e fa          	endbr64
    2b8c:	53                   	push   %rbx
    2b8d:	48 83 ec 10          	sub    $0x10,%rsp
    2b91:	64 48 8b 04 25 28 00 	mov    %fs:0x28,%rax
    2b98:	00 00
    2b9a:	48 89 44 24 08       	mov    %rax,0x8(%rsp)
    2b9f:	31 c0                	xor    %eax,%eax
    ; ...
```

4. 调用`read_line`函数读取输入行，返回的输入字符串地址存入`%rax`
5. 设置`strtol`参数：
   - `%rdi` = 输入字符串地址（`%rax`）
   - `%edx` = 基数`0xa`（十进制）
   - `%esi` = `0x0`（不保存结束指针）
     调用`strtol@plt`将输入字符串以十进制转换为长整数，结果存入`%rbx`
6. 加载`n1`的地址（`0x64ee(%rip)`）到`%rdi`，调用`get_sum`函数，其返回值存入`%eax`
   > [!TIP]
   > 我们无需关心 `n1` 的具体内容，也无需知道知道 `get_sum` 的逻辑。通过调试可得其返回值 `%eax` 为 62。
7. 比较`get_sum`的返回值（`%eax`）与`strtol`转换的整数（`%ebx`）：
   - 若不等（`jne`），跳至步骤 8
   - 若相等，继续步骤 9
8. 调用`explode_bomb`函数（触发炸弹爆炸），随后跳至步骤 9 中"打印信息"的起始位置

```asm
0000000000002b88 <secret_phase>:
    ; ...
    2ba1:	e8 08 05 00 00       	call   30ae <read_line>
    2ba6:	48 89 c7             	mov    %rax,%rdi
    2ba9:	ba 0a 00 00 00       	mov    $0xa,%edx
    2bae:	be 00 00 00 00       	mov    $0x0,%esi
    2bb3:	e8 68 f7 ff ff       	call   2320 <strtol@plt>
    2bb8:	48 89 c3             	mov    %rax,%rbx
    2bbb:	48 8d 3d ee 64 00 00 	lea    0x64ee(%rip),%rdi        # 90b0 <n1>
    2bc2:	e8 71 ff ff ff       	call   2b38 <get_sum>
    2bc7:	39 d8                	cmp    %ebx,%eax
    2bc9:	75 50                	jne    2c1b <secret_phase+0x93>
    ; ...
    2c1b:	e8 c3 03 00 00       	call   2fe3 <explode_bomb>
    ; ...
```

9. 依次加载 4 个特定字符串地址到`%rdi`，并调用`puts`打印这些字符串
10. 加载栈上`0x4(%rsp)`地址到`%rdi`，调用`phase_defused`函数
11. 进行栈溢出检查：
    - 从栈上`0x8(%rsp)`取出之前保存的栈金丝雀，与`%fs:0x28`原值比较
    - 若不等（`jne`），调用`__stack_chk_fail`处理栈溢出
    - 若相等，恢复栈空间（`add $0x10,%rsp`），弹出`%rbx`寄存器并返回（`ret`）

```asm
0000000000002b88 <secret_phase>:
    ; ...
    2bcb:	48 8d 3d 4e 26 00 00 	lea    0x264e(%rip),%rdi        # 5220 <_IO_stdin_used+0x220>
    2bd2:	e8 a9 f6 ff ff       	call   2280 <puts@plt>
    2bd7:	48 8d 3d 6a 26 00 00 	lea    0x266a(%rip),%rdi        # 5248 <_IO_stdin_used+0x248>
    2bde:	e8 9d f6 ff ff       	call   2280 <puts@plt>
    2be3:	48 8d 3d a6 26 00 00 	lea    0x26a6(%rip),%rdi        # 5290 <_IO_stdin_used+0x290>
    2bea:	e8 91 f6 ff ff       	call   2280 <puts@plt>
    2bef:	48 8d 3d d2 26 00 00 	lea    0x26d2(%rip),%rdi        # 52c8 <_IO_stdin_used+0x2c8>
    2bf6:	e8 85 f6 ff ff       	call   2280 <puts@plt>
    2bfb:	48 8d 7c 24 04       	lea    0x4(%rsp),%rdi
    2c00:	e8 e7 05 00 00       	call   31ec <phase_defused>
    2c05:	48 8b 44 24 08       	mov    0x8(%rsp),%rax
    2c0a:	64 48 2b 04 25 28 00 	sub    %fs:0x28,%rax
    2c11:	00 00
    2c13:	75 0d                	jne    2c22 <secret_phase+0x9a>
    2c15:	48 83 c4 10          	add    $0x10,%rsp
    2c19:	5b                   	pop    %rbx
    2c1a:	c3                   	ret
    2c1b:	e8 c3 03 00 00       	call   2fe3 <explode_bomb>
    2c20:	eb a9                	jmp    2bcb <secret_phase+0x43>
    2c22:	e8 89 f6 ff ff       	call   22b0 <__stack_chk_fail@plt>
```

因此，我们还需要在所有输入中新增一行，填入 secret phase 的答案：

```text
62
```

---

[更适合北大宝宝体质的 Bomb Lab 踩坑记](https://arthals.ink/blog/bomb-lab)
