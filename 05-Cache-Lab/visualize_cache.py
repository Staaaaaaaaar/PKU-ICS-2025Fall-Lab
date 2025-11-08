"""
visualize_cache.py

Visualize mapping of matrix elements to cache sets for a direct-mapped cache.

Assumptions (defaults):
- C int size: 4 bytes
- Cache size: 1024 bytes (1KB)
- Block size: 32 bytes
- Direct-mapped -> number of sets = cache_size / block_size

The matrix is laid out in C row-major order: index = i*M + j, where i is row (first index) and j is column (second index).
Plot: N rows by M columns grid; row 0 is at the top (origin='upper'), rows increase downward.

Usage examples:
    python visualize_cache.py -N 8 -M 16 --annotate

"""
import argparse
import numpy as np
import matplotlib.pyplot as plt
import matplotlib


def compute_set_indices(N, M, int_size=4, cache_size=1024, block_size=32):
    """Return an (N, M) array of set indices for each matrix element."""
    assert cache_size % block_size == 0, "cache_size must be multiple of block_size"
    sets_count = cache_size // block_size

    # linear index in memory for element A[i][j]
    linear_idx = np.arange(N * M, dtype=np.int64).reshape((N, M))
    # byte offset of each element
    byte_offsets = linear_idx * int_size
    # compute block index and then the cache set (direct mapped)
    block_idx = byte_offsets // block_size
    set_idx = block_idx % sets_count
    return set_idx, sets_count


def make_cmap(sets_count):
    # Build a palette of 32 visually distinct colors by sampling HSV and
    # introducing small saturation/value variations for better separation.
    base_n = 32
    hues = np.linspace(0, 1, base_n, endpoint=False)
    sats = np.full(base_n, 0.9)
    vals = np.full(base_n, 0.9)

    # Vary saturation/value pattern to increase perceptual difference
    for k in range(base_n):
        if k % 4 == 0:
            sats[k], vals[k] = 0.95, 0.85
        elif k % 4 == 1:
            sats[k], vals[k] = 0.8, 0.95
        elif k % 4 == 2:
            sats[k], vals[k] = 0.9, 0.9
        else:
            sats[k], vals[k] = 0.7, 1.0

    hsv = np.stack([hues, sats, vals], axis=1)
    rgb = matplotlib.colors.hsv_to_rgb(hsv)
    # If sets_count <= 32, return the first sets_count colors; otherwise repeat the 32-color palette
    if sets_count <= base_n:
        return matplotlib.colors.ListedColormap(rgb[:sets_count], name=f'distinct{sets_count}')
    else:
        colors = [rgb[i % base_n] for i in range(sets_count)]
        return matplotlib.colors.ListedColormap(colors, name=f'distinct{sets_count}')


def plot_sets(set_idx, sets_count, annotate=False, out_file=None, title=None):
    N, M = set_idx.shape
    cmap = make_cmap(sets_count)

    fig, ax = plt.subplots(figsize=(max(6, M * 0.4), max(4, N * 0.4)))
    im = ax.imshow(set_idx, cmap=cmap, vmin=0, vmax=sets_count - 1, origin='upper', interpolation='nearest')

    # Draw grid lines between cells
    ax.set_xticks(np.arange(M), minor=False)
    ax.set_yticks(np.arange(N), minor=False)
    ax.grid(which='minor', color='k', linestyle='-', linewidth=0.5)

    # Remove tick labels (or show indices)
    ax.set_xticks(np.arange(M))
    ax.set_yticks(np.arange(N))
    ax.set_xticklabels(np.arange(M))
    ax.set_yticklabels(np.arange(N))

    # Optionally annotate each cell with the set number
    if annotate:
        # Skip annotation if too large unless explicitly requested
        if N * M > 1000:
            print("Grid too large to annotate (N*M > 1000). Skipping annotations.")
        else:
            for i in range(N):
                for j in range(M):
                    ax.text(j, i, str(set_idx[i, j]), ha='center', va='center', color='white', fontsize=8)

    # colorbar with ticks centered in each color block
    boundaries = np.arange(sets_count + 1) - 0.5
    cbar = fig.colorbar(im, ax=ax, fraction=0.04, pad=0.04,
                        boundaries=boundaries, ticks=np.arange(sets_count))
    cbar.ax.set_ylabel('Cache set')
    cbar.set_ticklabels([str(i) for i in range(sets_count)])
    # hide tick lines so labels appear centered without end ticks
    cbar.ax.tick_params(length=0)

    ax.set_xlabel('Column')
    ax.set_ylabel('Row')
    if title:
        ax.set_title(title)

    plt.tight_layout()

    if out_file:
        plt.savefig(out_file, dpi=150)
        print(f"Saved visualization to {out_file}")
    else:
        plt.show()


def main():
    p = argparse.ArgumentParser(description='Visualize mapping of N x M int matrix elements to cache sets')
    p.add_argument('-N', type=int, required=True, help='Number of rows (first index)')
    p.add_argument('-M', type=int, required=True, help='Number of columns (second index)')
    p.add_argument('--int-size', type=int, default=4, help='Size of int in bytes (default: 4)')
    p.add_argument('--cache-size', type=int, default=1024, help='Total cache size in bytes (default: 1024)')
    p.add_argument('--block-size', type=int, default=32, help='Cache block size in bytes (default: 32)')
    p.add_argument('--annotate', action='store_true', help='Annotate each cell with its cache set number (skip if grid large)')
    p.add_argument('--out', type=str, default=None, help='Output image file (png, pdf, etc). If not provided, show interactively')
    p.add_argument('--title', type=str, default=None, help='Optional plot title')

    args = p.parse_args()

    set_idx, sets_count = compute_set_indices(args.N, args.M, args.int_size, args.cache_size, args.block_size)
    title = args.title or f"N={args.N}, M={args.M}, int={args.int_size}B, cache={args.cache_size}B, block={args.block_size}B ({sets_count} sets)"
    plot_sets(set_idx, sets_count, annotate=args.annotate, out_file=args.out, title=title)


if __name__ == '__main__':
    main()
