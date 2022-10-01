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
    uint64_t address;
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

// This is the struct which will hold one line of the cahce. It includes a valid tag and a data tag, though these are not used in the final program.
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
    ptr_file = fopen("mem_trace.txt", "r");
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

    // creates the sizes of the various cache parameters
    uint32_t offset_size = log(block_size) / log(2);
    uint32_t index_size = log(blocks) / log(2);
    uint32_t tag_size = 32 - offset_size - index_size;

    // useful initially for debugging, but now useful as another statistic
    printf("\nInput: size = %d, mapping = %x, Org = %x\n", cache_size, cache_mapping, cache_org);
    printf("Number of blocks: %d\n", blocks);
    if (cache_mapping == fa) // If the cache is fully associative, add the index to the tag
    {
        tag_size = tag_size + index_size;
        index_size = 0;
    }

    printf("Tag (%d) + index (%d) + offset (%d) = 32 bits of address\n", tag_size, index_size, offset_size);

    // printf("%I64u\n", splitBinary(4294967294, 31, 0));
    // i could declare the caches in an if statement, but that caused compiler errors, so the unused arrays are freed later on
    cache *Unified_cache = malloc(blocks * sizeof(cache));
    cache *Split_cache_data = malloc(blocks * sizeof(cache));
    cache *Split_cache_inst = malloc(blocks * sizeof(cache));

    // freeing unused arrays
    if (cache_org == uc)
    {
        free(Split_cache_data);
        free(Split_cache_inst);
    }

    if (cache_org == sc)
    {
        free(Unified_cache);
    }

    cache current_item; // use a temporary cache line to reduce operations done inside the cache
    cache_statistics.accesses = 0;

    int uc_fifo_middle_index = 0; // needed an index to show the middle of a fully associative cache to make FIFO work
    int sc_fifo_data_middle_index = 0;
    int sc_fifo_inst_middle_index = 0;
    int check_for_change = 0; // this is needed cause C doesnt have a break twice / continue twice operation. Decided not to use GOTO labels

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

        current_item.tag = splitBinary(access.address, tag_size, 0); // the splitBinary function converts the number to binary, then uses only a select range of bits to create the tag and index. More info by the function below
        current_item.index = splitBinary(access.address, index_size, tag_size);
        // printf("Type: %x, Address: %I32u, Tag: %d, Index: %d\n", access.accesstype, access.address, current_item.tag, current_item.index);
        // current_item.offset = splitBinary(access.address, offset_size, tag_size+index_size);

        if (cache_mapping == dm) // because i made different caches for each kind of cache organization, and because of the way treat the different cache mappings, i need to use these inefficient if statements
        {
            if (cache_org == uc)
            {
                if (Unified_cache[current_item.index].tag == current_item.tag) // compare the current tag and the tag at the cache's current item index
                {
                    // Can store data here, but wont
                    cache_statistics.uchits++;
                    cache_statistics.hits++;
                    continue;
                }
                Unified_cache[current_item.index].tag = current_item.tag; // If there was a hit, this would never run, so this just stores to the cache if it was a miss
            }
            else // This does the exact same as for unified cache, but once for each cache access type
            {
                if (access.accesstype == data) // check the data cache if the access is of data type
                {
                    if (Split_cache_data[current_item.index].tag == current_item.tag)
                    {
                        cache_statistics.scDatahits++;
                        cache_statistics.hits++;
                        continue;
                    }
                    Split_cache_data[current_item.index].tag = current_item.tag;
                }

                if (access.accesstype == instruction) // Check the instruction cache if the access is of instruction type
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

        if (cache_mapping == fa) // this is similar to before, but instead of using an index created from the access address, i use a middle index which defines the position of the "oldest" cache item, so a new one can be put in. For FIFO
        {
            if (cache_org == uc)
            {
                for (int i = 0; i < blocks; i++)
                {
                    if (current_item.tag == Unified_cache[i].tag) // check if tas is already in index
                    {
                        cache_statistics.uchits++;
                        cache_statistics.hits++;
                        check_for_change = 1; // because c cant break twice
                        break;
                    }
                }
                if (check_for_change == 0) // if no hit, replace oldest cache item with current item
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
                if (access.accesstype == data) // same as before, just twice
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
    printf("Accesses: %ld\n", cache_statistics.accesses);
    printf("Hits:     %ld\n", cache_statistics.hits);
    printf("Hit Rate: %.4f\n",
           (double)cache_statistics.hits / cache_statistics.accesses);
    // DO NOT CHANGE UNTIL HERE
    // You can extend the memory statistic printing if you like!


    if (cache_org == sc)
    {
        printf("Total hits:     %I64u\n", cache_statistics.hits);
        printf("Total hit rate: %.4f\n\n", (double)cache_statistics.hits / cache_statistics.accesses);
        printf("Data Hits:     %I64u\n", cache_statistics.scDatahits);
        printf("Data Hit Rate of total accesses: %.4f\n", (double)cache_statistics.scDatahits / cache_statistics.accesses);
        printf("Instruction Hits:     %I64u\n", cache_statistics.scInsthits);
        printf("Instruction Hit Rate of total accesses: %.4f\n\n", (double)cache_statistics.scInsthits / cache_statistics.accesses);
    }

    /* Close the trace file */
    fclose(ptr_file);
}

/*
This function accepts a number, leading bits, and removed bits to create index, tag, offset, and whatever else you can think of
The leading bits defines how many of the bits should be used to create the output number, while the removed number defines how many bits have been removed from the binary number already
For example:
    To create a 20 bit index (the first 20 bits of the binary number) you would use splitBinary(number, 20, 0). 20 for the size of the index, and 0 removed as the index is the first
    To create a five bit tag (the five bits following the 20 removed for the index) you would used splitBinary(number, 5, 20). 5 for the size of the tag, and 20 as you have used the first 20 bits for the index.
    finally if you want to create a seven bit offset (the last 7 bits of a 32 bit address), you would use splitBinary(number, 7, 20+5), 7 for the size of the offset, and 20+5 for the bits you have removed for the index and tag.
    I do realize its inefficient to do this operation 2/3 times per address access. But as the program can still handle ~1.5mil address accesses in around a second (on my machine), I didnt see need for it to be much improved
*/
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