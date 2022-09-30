#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

char *strsep(char **stringp, const char *delim);
uint64_t splitBinary(uint64_t number, int leading_bits, int removed);

typedef enum
{
    dm,
    fa
} cache_map_t;
typedef enum
{
    uc,
    sc
} cache_org_t;
typedef enum
{
    instruction,
    data
} access_t;

typedef struct
{
    uint32_t address;
    access_t accesstype;
} mem_access_t;

typedef struct
{
    uint64_t accesses;
    uint64_t hits;
    uint64_t uchits;
    uint64_t scDatahits;
    uint64_t scInsthits;
    // You can declare additional statistics if
    // you like, however you are now allowed to
    // remove the accesses or hits
} cache_stat_t;

typedef struct
{
    int valid;
    int32_t tag;
    int32_t index;
    int32_t offset;
    int32_t data;
} cache;

// DECLARE CACHES AND COUNTERS FOR THE STATS HERE

uint32_t cache_size;
uint32_t block_size = 64;
cache_map_t cache_mapping;
cache_org_t cache_org;

// USE THIS FOR YOUR CACHE STATISTICS
cache_stat_t cache_statistics;

/* Reads a memory access from the trace file and returns
 * 1) access type (instruction or data access
 * 2) memory address
 */
mem_access_t read_transaction(FILE *ptr_file)
{
    char type;
    mem_access_t access;

    if (fscanf(ptr_file, "%c %x\n", &type, &access.address) == 2)
    {
        if (type != 'I' && type != 'D')
        {
            printf("Unkown access type\n");
            exit(0);
        }
        access.accesstype = (type == 'I') ? instruction : data;
        return access;
    }

    /* If there are no more entries in the file,
     * return an address 0 that will terminate the infinite loop in main
     */
    access.address = 0;
    return access;
}

void main(int argc, char **argv)
{
    // Reset statistics:
    memset(&cache_statistics, 0, sizeof(cache_stat_t));

    /* Read command-line parameters and initialize:
     * cache_size, cache_mapping and cache_org variables
     */
    /* IMPORTANT: *IF* YOU ADD COMMAND LINE PARAMETERS (you really don't need to),
     * MAKE SURE TO ADD THEM IN THE END AND CHOOSE SENSIBLE DEFAULTS SUCH THAT WE
     * CAN RUN THE RESULTING BINARY WITHOUT HAVING TO SUPPLY MORE PARAMETERS THAN
     * SPECIFIED IN THE UNMODIFIED FILE (cache_size, cache_mapping and cache_org)
     */
    if (argc != 4)
    { /* argc should be 2 for correct execution */
        printf(
            "Usage: ./cache_sim [cache size: 128-4096] [cache mapping: dm|fa] "
            "[cache organization: uc|sc]\n");
        exit(0);
    }

    else
    {
        /* argv[0] is program name, parameters start with argv[1] */
        /* Set cache size */
        cache_size = atoi(argv[1]);

        /* Set Cache Mapping */
        if (strcmp(argv[2], "dm") == 0)
        {
            cache_mapping = dm;
        }
        else if (strcmp(argv[2], "fa") == 0)
        {
            cache_mapping = fa;
        }
        else
        {
            printf("Unknown cache mapping\n");
            exit(0);
        }

        /* Set Cache Organization */
        if (strcmp(argv[3], "uc") == 0)
        {
            cache_org = uc;
        }
        else if (strcmp(argv[3], "sc") == 0)
        {
            cache_org = sc;
        }
        else
        {
            printf("Unknown cache organization\n");
            exit(0);
        }
    }

    /* Open the file mem_trace.txt to read memory accesses */
    FILE *ptr_file;
    ptr_file = fopen("Large.txt", "r");
    if (!ptr_file)
    {
        printf("Unable to open the trace file\n");
        exit(1);
    }

    /* Loop until whole trace file has been read */
    mem_access_t access;

    // create number of blocks, which is total cache size by the block size
    unsigned int blocks = cache_size / (block_size);
    if (cache_org == sc)
    {
        blocks = blocks / 2;
    }

    uint32_t offset_size = log(block_size) / log(2);
    uint32_t index_size = log(blocks) / log(2);
    uint32_t tag_size = 32 - offset_size - index_size;

    printf("\nInput: size = %d, mapping = %x, Org = %x\n", cache_size, cache_mapping, cache_org);
    printf("Number of blocks: %d\n", blocks);
    if (cache_mapping == fa)
    {
        tag_size = tag_size + index_size;
        index_size = 0;
    }

    printf("Tag (%d) + index (%d) + offset (%d) = 32 bits of address\n", tag_size, index_size, offset_size);

    // printf("%I64u\n", splitBinary(4294967294, 31, 0));
    cache *Unified_cache = malloc(blocks * sizeof(cache));
    cache *Split_cache_data = malloc(blocks * sizeof(cache));
    cache *Split_cache_inst = malloc(blocks * sizeof(cache));

    if (cache_org == uc)
    {
        free(Split_cache_data);
        free(Split_cache_inst);
    }

    if (cache_org == sc)
    {
        free(Unified_cache);
    }

    cache current_item;
    cache_statistics.accesses = 0;

    int uc_fifo_middle_index = 0;
    int sc_fifo_data_middle_index = 0;
    int sc_fifo_inst_middle_index = 0;
    int check_for_change = 0;

    while (1)
    {
        access = read_transaction(ptr_file);
        check_for_change = 0;
        // If no transactiongits left, break out of loop
        if (access.address == 0)
        {
            break;
        }
        cache_statistics.accesses += 1;

        // printf("Access: %I64u\n", cache_statistics.accesses);
        //  printf("%d %I32u\n", access.accesstype, access.address);
        /* Do a cache access */
        // ADD YOUR CODE HERE

        current_item.tag = splitBinary(access.address, tag_size, 0);
        current_item.index = splitBinary(access.address, index_size, tag_size);
        // printf("Type: %x, Address: %I32u, Tag: %d, Index: %d\n", access.accesstype, access.address, current_item.tag, current_item.index);
        // current_item.offset = splitBinary(access.address, offset_size, tag_size+index_size);

        if (cache_mapping == dm)
        {
            if (cache_org == uc)
            {
                if (Unified_cache[current_item.index].tag == current_item.tag)
                {
                    cache_statistics.uchits++;
                    cache_statistics.hits++;
                    continue;
                }
                Unified_cache[current_item.index].tag = current_item.tag;
            }
            else
            {
                if (access.accesstype == data)
                {
                    if (Split_cache_data[current_item.index].tag == current_item.tag)
                    {
                        cache_statistics.scDatahits++;
                        cache_statistics.hits++;
                        continue;
                    }
                    Split_cache_data[current_item.index].tag = current_item.tag;
                }

                if (access.accesstype == instruction)
                {
                    if (Split_cache_inst[current_item.index].tag == current_item.tag)
                    {
                        cache_statistics.hits++;
                        cache_statistics.scInsthits++;
                        continue;
                    }
                    Split_cache_inst[current_item.index].tag = current_item.tag;
                }
            }
        }

        if (cache_mapping == fa)
        {
            if (cache_org == uc)
            {
                for (int i = 0; i < blocks; i++)
                {
                    if (current_item.tag == Unified_cache[i].tag)
                    {
                        cache_statistics.uchits++;
                        check_for_change = 1;
                        break;
                    }
                }
                if (check_for_change == 0)
                {
                    Unified_cache[uc_fifo_middle_index].tag = current_item.tag;
                    uc_fifo_middle_index++;
                    if (uc_fifo_middle_index == blocks)
                    {
                        uc_fifo_middle_index = 0;
                    }
                }
            }

            else
            {
                if (access.accesstype == data)
                {
                    for (int i = 0; i < blocks; i++)
                    {
                        if (current_item.tag == Split_cache_data[i].tag)
                        {
                            cache_statistics.scDatahits++;
                            cache_statistics.hits++;
                            check_for_change = 1;
                            break;
                        }
                    }
                    if (check_for_change == 0)
                    {
                        Split_cache_data[uc_fifo_middle_index].tag = current_item.tag;
                        sc_fifo_data_middle_index++;
                        if (sc_fifo_data_middle_index == blocks)
                        {
                            sc_fifo_data_middle_index = 0;
                        }
                    }
                }

                if (access.accesstype == instruction)
                {
                    for (int i = 0; i < blocks; i++)
                    {
                        if (current_item.tag == Split_cache_inst[i].tag)
                        {
                            cache_statistics.scInsthits++;
                            cache_statistics.hits++;
                            check_for_change = 1;
                            break;
                        }
                    }
                    if (check_for_change == 0)
                    {
                        Split_cache_inst[uc_fifo_middle_index].tag = current_item.tag;
                        sc_fifo_inst_middle_index++;
                        if (sc_fifo_inst_middle_index == blocks)
                        {
                            sc_fifo_inst_middle_index = 0;
                        }
                    }
                }
            }
        }
    }

    /* Print the statistics */
    // DO NOT CHANGE THE FOLLOWING LINES!
    printf("\nCache Statistics\n");
    printf("-----------------\n\n");
    printf("Accesses: %I64u\n", cache_statistics.accesses);
    if (cache_org == uc)
    {
        printf("Hits:     %I64u\n", cache_statistics.uchits);
        printf("Hit Rate: %.4f\n", (double)cache_statistics.uchits / cache_statistics.accesses);
    }
    if (cache_org == sc)
    {
        printf("Total hits:     %I64u\n", cache_statistics.hits);
        printf("Total hit rate: %.4f\n\n", (double)cache_statistics.hits / cache_statistics.accesses);
        printf("Data Hits:     %I64u\n", cache_statistics.scDatahits);
        printf("Data Hit Rate of total accesses: %.4f\n", (double)cache_statistics.scDatahits / cache_statistics.accesses);
        printf("Instruction Hits:     %I64u\n", cache_statistics.scInsthits);
        printf("Instruction Hit Rate of total accesses: %.4f\n", (double)cache_statistics.scInsthits / cache_statistics.accesses);
    }
    // DO NOT CHANGE UNTIL HERE
    // You can extend the memory statistic printing if you like!

    /* Close the trace file */
    fclose(ptr_file);
}

uint64_t splitBinary(uint64_t number, int num_leading_bits, int removed)
{
    int bits[32] = {0};
    if (num_leading_bits > 32 - removed)
    {
        printf("You are going beyond the binary value of the number");
        return (0);
    }

    for (int i = 0; i < 32; i++)
    {
        bits[i] = number % 2;
        number = (number - bits[i]) / 2;
        // printf("%d", bits[i]);
        if (number == 0)
        {
            break;
        }
    }
    // printf("\n");
    double newNum = 0;
    for (int i = 31 - removed; i > 31 - removed - num_leading_bits; i--)
    {
        newNum = newNum + bits[i] * pow(2, i - 32 + num_leading_bits + removed);
    }
    // printf("%f\n", newNum);
    uint64_t returnVal = newNum;
    // printf("%I64u\n", returnVal);
    return (returnVal);
}