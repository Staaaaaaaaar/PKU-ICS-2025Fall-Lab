#include "cachelab.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>

// Cache line structure
typedef struct {
    int valid;
    unsigned long long tag;
    int lru_counter;
} cache_line;

// Cache set structure
typedef struct {
    cache_line *lines;
} cache_set;

// Cache structure
typedef struct {
    int s;
    int E;
    int b;
    cache_set *sets;
} cache;

// Global variables for results
int hit_count = 0;
int miss_count = 0;
int eviction_count = 0;

// Function to initialize the cache
cache init_cache(int s, int E, int b) {
    cache new_cache;
    new_cache.s = s;
    new_cache.E = E;
    new_cache.b = b;
    int S = 1 << s; // Number of sets

    new_cache.sets = (cache_set *)malloc(S * sizeof(cache_set));
    for (int i = 0; i < S; i++) {
        new_cache.sets[i].lines = (cache_line *)malloc(E * sizeof(cache_line));
        for (int j = 0; j < E; j++) {
            new_cache.sets[i].lines[j].valid = 0;
            new_cache.sets[i].lines[j].tag = 0;
            new_cache.sets[i].lines[j].lru_counter = 0;
        }
    }
    return new_cache;
}

// Function to free the cache memory
void free_cache(cache my_cache) {
    int S = 1 << my_cache.s;
    for (int i = 0; i < S; i++) {
        free(my_cache.sets[i].lines);
    }
    free(my_cache.sets);
}

// Function to simulate a memory access
void access_cache(cache *my_cache, unsigned long long address, int verbose) {
    int s = my_cache->s;
    int E = my_cache->E;
    int b = my_cache->b;

    unsigned long long set_index_mask = (1 << s) - 1;
    unsigned long long set_index = (address >> b) & set_index_mask;
    unsigned long long tag = address >> (s + b);

    cache_set *current_set = &my_cache->sets[set_index];

    // Update LRU counters for the set
    for (int i = 0; i < E; i++) {
        if (current_set->lines[i].valid) {
            current_set->lines[i].lru_counter++;
        }
    }

    // Check for hit
    for (int i = 0; i < E; i++) {
        if (current_set->lines[i].valid && current_set->lines[i].tag == tag) {
            hit_count++;
            if (verbose) printf(" hit");
            current_set->lines[i].lru_counter = 0; // Reset LRU counter on hit
            return;
        }
    }

    // Miss
    miss_count++;
    if (verbose) printf(" miss");

    // Find an empty line
    for (int i = 0; i < E; i++) {
        if (!current_set->lines[i].valid) {
            current_set->lines[i].valid = 1;
            current_set->lines[i].tag = tag;
            current_set->lines[i].lru_counter = 0;
            return;
        }
    }

    // Eviction
    eviction_count++;
    if (verbose) printf(" eviction");
    int max_lru = -1;
    int evict_index = -1;
    for (int i = 0; i < E; i++) {
        if (current_set->lines[i].lru_counter > max_lru) {
            max_lru = current_set->lines[i].lru_counter;
            evict_index = i;
        }
    }
    current_set->lines[evict_index].tag = tag;
    current_set->lines[evict_index].lru_counter = 0;
}

void print_usage() {
    printf("Usage: ./csim [-hv] -s <num> -E <num> -b <num> -t <file>\n");
    printf("-h         Print this help message.\n");
    printf("-v         Optional verbose flag.\n");
    printf("-s <num>   Number of set index bits.\n");
    printf("-E <num>   Number of lines per set.\n");
    printf("-b <num>   Number of block offset bits.\n");
    printf("-t <file>  Trace file.\n");
}

int main(int argc, char *argv[]) {
    int s = 0, E = 0, b = 0;
    char *trace_file = NULL;
    int verbose = 0;

    int opt;
    while ((opt = getopt(argc, argv, "hvs:E:b:t:")) != -1) {
        switch (opt) {
        case 'h':
            print_usage();
            exit(0);
        case 'v':
            verbose = 1;
            break;
        case 's':
            s = atoi(optarg);
            break;
        case 'E':
            E = atoi(optarg);
            break;
        case 'b':
            b = atoi(optarg);
            break;
        case 't':
            trace_file = optarg;
            break;
        default:
            print_usage();
            exit(1);
        }
    }

    if (s == 0 || E == 0 || b == 0 || trace_file == NULL) {
        printf("Error: Missing required command-line argument\n");
        print_usage();
        exit(1);
    }

    cache my_cache = init_cache(s, E, b);

    FILE *file = fopen(trace_file, "r");
    if (file == NULL) {
        printf("Error: Cannot open trace file %s\n", trace_file);
        exit(1);
    }

    char operation;
    unsigned long long address;
    int size;
    char line[256];

    while (fgets(line, sizeof(line), file)) {
        if (line[0] == 'I') {
            continue;
        }
        // sscanf returns the number of items successfully read
        if (sscanf(line, " %c %llx,%d", &operation, &address, &size) == 3) {
            if (verbose) {
                printf("%c %llx,%d", operation, address, size);
            }
            switch (operation) {
            case 'L':
                access_cache(&my_cache, address, verbose);
                break;
            case 'S':
                access_cache(&my_cache, address, verbose);
                break;
            case 'M':
                access_cache(&my_cache, address, verbose); // Load
                access_cache(&my_cache, address, verbose); // Store
                break;
            }
            if (verbose) {
                printf("\n");
            }
        }
    }

    fclose(file);
    free_cache(my_cache);

    printSummary(hit_count, miss_count, eviction_count);

    return 0;
}
