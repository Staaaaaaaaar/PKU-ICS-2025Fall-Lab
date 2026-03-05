# ICS - Data Lab | 万物皆比特

在 Data Lab 中，我们将深入计算机最底层的语言——**位运算**，用限定的操作符，在各种严格限制下实现一系列函数。每个函数都是一道谜题，而我们的任务，就是用最少的操作符写出解法。

## 预备知识

在正式开始挑战位运算谜题之前，我们需要对计算机底层的数值表示与位运算逻辑有深刻的理解。

### 核心概念

- **补码**：计算机中整数的通用表示方法。最高位为符号位，权重为 。理解补码是处理正负数转换、位移溢出的基础。
- **IEEE 754 浮点数标准**：浮点数由符号位（Sign）、阶码（Exponent）和尾数（Fraction）三部分组成。在实验的浮点数部分，我们需要手动拆解这些位域并处理规格化、非规格化以及特殊值。

### 常用位运算技巧

为了在受限的操作符下实现功能，我们需要掌握一些经典的位运算技巧，包括但不限于：

- `!!x`：将任意非零值转换为 `1`，零值保持为 `0`，在需要将数值映射为布尔判定时非常有用。

- `x >> 31`：若 `x` 为负则结果为 `0xFFFFFFFF`，若为正则结果为 `0x00000000`，常用于构造掩码。

- `-x = ~x + 1`：利用补码特性，按位取反加一即可得到其相反数。

## 前置准备

### 编码规则

我们实现的所有解法必须严格遵守以下规范，否则 `dlc` 检查将无法通过：

#### 整数部分

- **限定操作符**：只能使用 `! ~ & ^ | + << >>`，具体参考每个函数的说明。
- **常量限制**：只能使用 `0` 到 `255` 之间的整数常量，不能直接使用大数如 `0xFFFFFFFF`。
- **代码禁令**：
  - 禁止使用任何控制流语句。
  - 禁止定义或使用宏。
  - 禁止调用其他函数。
  - 禁止使用其他数据类型。
  - 禁止进行类型转换。

#### 浮点数部分

- 允许使用条件判断和循环控制流语句。
- **代码禁令**：依然禁止使用 `float` 或 `double` 类型，禁止进行涉及浮点数的运算或强制转换。

### 工具链使用

实验提供了几个关键工具来帮助我们验证正确性并评估表现：

#### dlc (Data Lab Compiler)

这是一个静态分析工具，用于检查你的代码是否违反了编码规则，并统计操作符的使用数量。

```bash
./dlc bits.c
```

#### btest

用于通过测试样例来验证函数的功能正确性。

编译与运行：

```bash
make btest
./btest
```

如果你只想测试某个具体的函数（例如 `bitOr`），可以使用 `-f` 参数：

```bash
./btest -f bitOr
```

#### BDD Checker

基于二进制决策图（Binary Decision Diagrams）的形式化验证工具，它会穷举所有可能的输入来确保你的函数完全正确。

```bash
./check.pl
```

#### driver

这是最终的评分脚本，它结合了 `dlc` 和 `btest` 的结果，计算出你本实验的得分。

```bash
./driver.pl
```

## bitOr(x, y)

```c
/*
 * bitOr - x|y using only ~ and &
 *   Example: bitOr(6, 5) = 7
 *   Legal ops: ~ &
 *   Max ops: 8
 *   Rating: 1
 */
int bitOr(int x, int y) {
  return ~(~x & ~y);
}
```

## upperBits(n)

```c
/*
 * upperBits - pads n upper bits with 1's
 *  You may assume 0 <= n <= 32
 *  Example: upperBits(4) = 0xF0000000
 *  Legal ops: ! ~ & ^ | + << >>
 *  Max ops: 10
 *  Rating: 1
 */
int upperBits(int n) {
  int t = !!n;
  int m = n + ~0 + !t;
  return (t << 31) >> m;
}
```

笔者采用的思路是将 `0x80000000` 算术右移 `n-1` 位得到对应结果的思路，但是要格外处理 `n=0` 的特殊情形，笔者对于特殊情况的处理比较繁琐且有技巧型，应该还有更好的解法。

## fullAdd(x, y)

```c
/*
 * fullAdd - 4-bits add using bit-wise operations only.
 *   (0 <= x, y < 16)
 *   Example: fullAdd(12, 7) = 3,
 *            fullAdd(7, 8) = 15,
 *   Legal ops: ~ | ^ & << >>
 *   Max ops: 30
 *   Rating: 2
 */
int fullAdd(int x, int y) {
  int add_1 = x ^ y;
  int carry_1 = (x & y) << 1;

  int add_2 = add_1 ^ carry_1;
  int carry_2 = (add_1 & carry_1) << 1;

  int add_3 = add_2 ^ carry_2;
  int carry_3 = (add_2 & carry_2) << 1;

  int add_4 = add_3 ^ carry_3;
  int mask = 15;
  return add_4 & mask;
}
```

主要思路就是手动模拟加法的**相加**和**进位**过程。

## rotateLeft(x, n)

```c
/*
 * rotateLeft - Rotate x to the left by n
 *   Can assume that 0 <= n <= 31
 *   Examples: rotateLeft(0x87654321,4) = 0x76543218
 *   Legal ops: ~ & ^ | + << >> !
 *   Max ops: 25
 *   Rating: 3
 */
int rotateLeft(int x, int n) {
  int left = x << n;
  int mask = (1 << n) + ~0;
  int right = (x >> (32 + ~n + 1)) & mask;
  return left | right;
}
```

主要思路是分别构造左右两个片段后使用或运算拼接。

## bitParity(x)

```c
/*
 * bitParity - returns 1 if x contains an odd number of 0's
 *   Examples: bitParity(5) = 0, bitParity(7) = 1
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 20
 *   Rating: 4
 */
int bitParity(int x) {
  int a = (x >> 16) ^ x;
  int b = (a >> 8) ^ a;
  int c = (b >> 4) ^ b;
  int d = (c >> 2) ^ c;
  int e = (d >> 1) ^ d;
  return e & 1;
}
```

函数 `bitParity` 要求我们判断一个整数的二进制表示中 0 的个数是奇数还是偶数。由于对于操作符和控制流的限制，我们不能直接遍历每一位来计数。但是，我们可以采用了一种**二分折叠**的策略，通过异或操作将整数的各个位的信息逐步合并，最终得到一个单一的位，表示整个整数中 0 的奇偶性。

## negate(x)

```c
/*
 * negate - return -x
 *   Example: negate(1) = -1.
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 5
 *   Rating: 2
 */
int negate(int x) {
  return ~x + 1;
}
```

## oneMoreThan(x, y)

```c
/*
 * oneMoreThan - return 1 if y is one more than x, and 0 otherwise
 *   Examples oneMoreThan(0, 1) = 1, oneMoreThan(-1, 1) = 0
 *   Legal ops: ~ & ! ^ | + << >>
 *   Max ops: 15
 *   Rating: 2
 */
int oneMoreThan(int x, int y) {
  int z = x + 1;
  int diff = z ^ y;
  int isoverflow = !(((y << 1) ^ 0) | ((y >> 31) ^ ~0));
  return !diff & !isoverflow;
}
```

`oneMoreThan` 函数要求我们判断 `y` 是否恰好比 `x` 大 `1`，即 `y` 和 `x + 1` 是否相等，这可以由异或运算直接判断。但是需要注意 `x + 1` 发生**整数正溢出**的情况，即当 `x = INT_MAX` 时，`x + 1` 会变为 `INT_MIN`。

## ezThreeFourths(x)

```c
/*
 * ezThreeFourths - multiplies by 3/4 rounding toward 0,
 *   Should exactly duplicate effect of C expression (x*3/4),
 *   including overflow behavior.
 *   Examples: ezThreeFourths(11) = 8
 *             ezThreeFourths(-9) = -6
 *             ezThreeFourths(1073741824) = -268435456 (overflow)
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 12
 *   Rating: 3
 */
int ezThreeFourths(int x) {
  int result;
  int sign;
  result = (x << 1) + x;
  sign = result >> 31;
  result = (result + (3 & sign)) >> 2;
  return result;
}
```

`ezThreeFourths` 函数要求我们计算 `x * 3 / 4`，并且结果要向零舍入。

**思路**：先乘 3（`x + x + x` 或 `x << 1 + x`），再除 4（`>> 2`），但需处理负数的向零舍入。

## isLess(x, y)

```c
/*
 * isLess - if x < y  then return 1, else return 0
 *   Example: isLess(4,5) = 1.
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 24
 *   Rating: 3
 */
int isLess(int x, int y) {
  int z = x + ~y + 1;
  int i = (x ^ y) >> 31; // x and y have different MSB ?
  int sign_x = x >> 31;
  int sign = z >> 31;
  return ((sign & ~i) | (i & sign_x)) & 1;
}
```

**思路**：利用 `x - y` 的符号位判断，但需处理溢出。

- 若 `x` 和 `y` 符号位相同：`x - y` 符号位为 `1` 等价于 `x < y`
- 若 `x` 和 `y` 符号位不同：`x` 为负则 `x < y`

## satMul2(x)

```c
/*
 * satMul2 - multiplies by 2, saturating to Tmin or Tmax if overflow
 *   Examples: satMul2(0x30000000) = 0x60000000
 *             satMul2(0x40000000) = 0x7FFFFFFF (saturate to TMax)
 *             satMul2(0x90000000) = 0x80000000 (saturate to TMin)
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 20
 *   Rating: 3
 */
int satMul2(int x) {
  int y = x << 1;
  int i = (x ^ y) >> 31;
  int tmin = 1 << 31;
  int tmax = ~tmin;
  int result = (i & ((x >> 31) ^ tmax)) | (~i & y);
  return result;
}
```

`satMul2` 函数要求我们将整数 `x` 乘以 `2`，但如果结果溢出，则返回 `TMax` 或 `TMin`。

**思路**：使用左移实现乘 `2`，并通过检查符号位是否变化来判断是否溢出。

## modThree(x)

```c
/*
 * modThree - calculate x mod 3 without using %.
 *   Example: modThree(12) = 0,
 *            modThree(2147483647) = 1,
 *            modThree(-8) = -2,
 *   Legal ops: ~ ! | ^ & << >> +
 *   Max ops: 60
 *   Rating: 4
 */
int modThree(int x) {
  int a = ~3 + 1;
  int s, t, sign, iszero;
  int m1 = ~(~0 << 16);
  int m2 = ~(~0 << 8);
  int m3 = ~(~0 << 4);
  int m4 = ~(~0 << 2);
  s = (x >> 16) + (x & m1);
  s = (s >> 16) + (s & m1);
  s = (s >> 8) + (s & m2);
  s = (s >> 8) + (s & m2);
  s = (s >> 4) + (s & m3);
  s = (s >> 4) + (s & m3);
  s = (s >> 2) + (s & m4);
  s = (s >> 2) + (s & m4);

  // 0~3 -> 0~2
  t = s + a;
  sign = t >> 31;
  s = (t & ~sign) | (s & sign);

  // negative case?
  sign = x >> 31;
  iszero = !s << 31 >> 31;
  s = s + (a & sign & ~iszero);

  return s;
}
```

`modThree` 函数要求我们计算 `x mod 3` 的结果，注意 `x` 为负数时结果也为负数。

**思路**：利用 `2^k ≡ (-1)^k (mod 3)`，将整数 `x` 进行折叠，为确保最终得到一个 `0~3` 的数，我们简单地进行多次折叠处理。

- 对于 `x` 为正数的情况，我们需要将结果转换为 `0~2`
- 对于 `x` 为负数的情况，我们需要将结果转换为 `-2~0`

## float_half(uf)

```c
/*
 * float_half - Return bit-level equivalent of expression 0.5*f for
 *   floating point argument f.
 *   Both the argument and result are passed as unsigned int's, but
 *   they are to be interpreted as the bit-level representation of
 *   single-precision floating point values.
 *   When argument is NaN, return argument
 *   Legal ops: Any integer/unsigned operations incl. ||, &&. also if, while
 *   Max ops: 30
 *   Rating: 4
 */
unsigned float_half(unsigned uf) {
  int m1 = 0x80000000;
  int m2 = 0x7f800000;
  int m3 = 0x007fffff;
  int s = uf & m1;
  int exp = uf & m2;
  int frac = uf & m3;
  if (!exp)
    return ((s | (frac >> 1)) + (frac & 1 & (frac >> 1 & 1)));
  if (exp == 0x800000)
    return ((s | ((frac | 0x800000) >> 1)) + (frac & 1 & (frac >> 1 & 1)));
  if (exp == 0x7f800000)
    return uf;
  else
    return (s | (exp - (1 << 23)) | frac);
}
```

`float_half` 函数要求我们计算浮点数 `uf` 的一半，输入和输出均为浮点数的位级表示。

**思路**：我们可以根据指数位的情况进行不同的处理：

- `exp == 0`：`frac >> 1`，并处理舍入（看最低两位）
- `exp == 1`：转为非规格化数，将隐含的 `1` 加入 `frac`，再右移，并处理舍入
- `exp == 0xFF`：`NaN/Inf`，返回原值
- 剩下的情况：`exp - 1`

## float_i2f(x)

```c
/*
 * float_i2f - Return bit-level equivalent of expression (float) x
 *   Result is returned as unsigned int, but
 *   it is to be interpreted as the bit-level representation of a
 *   single-precision floating point values.
 *   Legal ops: Any integer/unsigned operations incl. ||, &&. also if, while
 *   Max ops: 30
 *   Rating: 4
 */
unsigned float_i2f(int x) {
  int s, exp, frac, absx, round, i = 0; // Declare all variables at once
  if (!x)
    return 0;
  if (x == 0x80000000)
    return 0xcf000000;
  s = x & 0x80000000;
  if (!s)
    absx = x;
  else
    absx = ~x + 1;
  while (!!(absx >> i)) {
    i = i + 1;
  }
  exp = 126 + i;
  frac = ((absx << (32 - i)) & 0x7fffffff) >> 8;
  round = (absx << (55 - i)) & 0x7fffffff;
  if (round > 0x40000000 || (round == 0x40000000 && (frac & 1))) {
    frac = frac + 1;
    if (frac >> 23) {
      frac = 0;
      exp = exp + 1;
    }
  }
  return (s | (exp << 23) | frac);
}
```

`float_i2f` 函数要求我们将整数 `x` 转换为单精度浮点数的位级表示。

**思路**：把 `x` 按 IEEE754 单精度的格式拆成**符号位 s**、**指数位 exp**、**尾数 frac**，再做偶数舍入，最后拼接。

## float64_f2i(uf1, uf2)

```c
/*
 * float64_f2i - Return bit-level equivalent of expression (int) f
 *   for 64 bit floating point argument f.
 *   Argument is passed as two unsigned int, but
 *   it is to be interpreted as the bit-level representation of a
 *   double-precision floating point value.
 *   Notice: uf1 contains the lower part of the f64 f
 *   Anything out of range (including NaN and infinity) should return
 *   0x80000000u.
 *   Legal ops: Any integer/unsigned operations incl. ||, &&. also if, while
 *   Max ops: 20
 *   Rating: 4
 */
int float64_f2i(unsigned uf1, unsigned uf2) {
  int s = uf2 >> 31;
  int exp = ((uf2 & 0x7ff00000) >> 20) - 1023;
  unsigned frac1 = uf2 & 0x000fffff;
  unsigned frac2 = uf1;
  unsigned result;
  if (exp < 0) return 0;
  if (exp > 31 || (exp == 31 && (!s || frac1 | frac2))) return 0x80000000u;
  result = (frac1 << 11) | 0x80000000u | (frac2 >> 21);
  result = result >> (31 - exp);
  if (s) return -result;
  else return result;
}
```

`float64_f2i` 函数要求我们将双精度浮点数转换为整数。

**思路**：把 `uf2:uf1` 视为一个 64 位 double 的原始比特，先拆字段再按指数把尾数对齐到整数位，最后处理溢出与符号。

- **字段拆分**：
  - 符号位 `s = uf2 >> 31`
  - 指数域 `e = (uf2 >> 20) & 0x7ff`，无偏指数 `exp = e - 1023`
  - 52 位尾数由 `frac1 = uf2 & 0x000fffff`（高 20 位）与 `frac2 = uf1`（低 32 位）组成
- **范围判断**：
  - `exp < 0`：绝对值小于 1，向 0 取整结果为 0
  - `exp > 31`：一定超出 32 位 int 可表示范围，返回 `0x80000000u`
  - `exp == 31`：只有 `-2^31` 这一种情况合法（符号为负且尾数必须刚好为 1.0），否则也算溢出
- **构造有效数并移位**：
  - `result = (frac1 << 11) | 0x80000000u | (frac2 >> 21)` 得到形如 `1xxxxxxxx...` 的 32 位片段
  - 然后右移 `31 - exp`，相当于把二进制小数点移动到正确位置，得到整数部分
- **符号恢复**：若 `s` 为 1，则返回 `-result`，否则返回 `result`。

## float_pwr2(x)

```c
/*
 * float_pwr2 - Return bit-level equivalent of the expression 2.0^x
 *   (2.0 raised to the power x) for any 32-bit integer x.
 *
 *   The unsigned value that is returned should have the identical bit
 *   representation as the single-precision floating-point number 2.0^x.
 *   If the result is too small to be represented as a denorm, return
 *   0. If too large, return +INF.
 *
 *   Legal ops: Any integer/unsigned operations incl. ||, &&. Also if, while
 *   Max ops: 30
 *   Rating: 4
 */
unsigned float_pwr2(int x) {
  if (x < -149) return 0;
  if (x > 127) return 0x7f800000;
  if (x >= -126) return (x + 127) << 23;
  return 1 << (x + 149);
}
```

`float_pwr2` 函数要求我们计算 `2.0^x` 的单精度浮点数位级表示。

**思路**：`2.0^x` 的规格化形式就是 `1.0 * 2^x`，因此尾数固定为 0，核心在于根据 `x` 落在哪个区间决定它是 **规格化数**、**非规格化数** 还是 **溢出/下溢**。

- **下溢到 0**：最小的非规格化数是 $2^{-149}$，因此 `x < -149` 直接返回 `0`
- **上溢到 +INF**：最大的规格化指数是 127，因此 `x > 127` 返回 `+INF`（`0x7f800000`）
- **规格化区间**（`-126 <= x <= 127`）：指数域为 `x + 127`，尾数为 0，所以结果是 `(x + 127) << 23`
- **非规格化区间**（`-149 <= x < -126`）：指数域为 0，数值大小通过尾数字段表示，因此 `frac = 1 << (x + 149)`

---

[更适合北大宝宝体质的 Data Lab 踩坑记](https://arthals.ink/blog/data-lab)
