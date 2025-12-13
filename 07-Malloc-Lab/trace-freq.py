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