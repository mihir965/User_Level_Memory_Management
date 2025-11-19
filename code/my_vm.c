
#include "my_vm.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>   // optional for memcpy if you later implement put/get

// -----------------------------------------------------------------------------
// Global Declarations (optional)
// -----------------------------------------------------------------------------
static uint8_t* physical_memory = NULL; //A contiguous array of bytes of memory
static pde_t* pgdir = NULL; //This is the top-level page directory which will map to the Page Table 
static uint8_t *physical_bitmap = NULL; // A bit map array for mapping each physical page
static uint8_t *virtual_bitmap = NULL; //A bit map array for mapping each virtual page - much more than the number of physical pages 4x
static size_t num_physical_pages = 0; // This will be 1GB / 4KB
static size_t num_virtual_pages = 0; // This is 4GB / 4KB
static bool vm_initialized = false;

struct tlb tlb_store; // Placeholder for your TLB structure

static inline void bitmap_set(uint8_t *bm, size_t idx){
    bm[idx/8] |= (1 << (idx % 8));
}

// Optional counters for TLB statistics
static unsigned long long tlb_lookups = 0;
static unsigned long long tlb_misses  = 0;

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------
/*
 * set_physical_mem()
 * ------------------
 * Allocates and initializes simulated physical memory and any required
 * data structures (e.g., bitmaps for tracking page use).
 *
 * Return value: None.
 * Errors should be handled internally (e.g., failed allocation).
 */
void set_physical_mem(void) {
    // TODO: Implement memory allocation for simulated physical memory.
    // Use 32-bit values for sizes, page counts, and offsets.

    /* Checking if the VM is initialized */
    if(vm_initialized){
        printf("VM already initialized\n");
        return;
    }

    /* First we allocate the simulated physical ram */
    physical_memory = malloc(MEMSIZE);
    if(!physical_memory){
        perror("malloc physical_memory\n");
        exit(1);
    }
    /* We also set everything to 0 - initialization */
    memset(physical_memory, 0, MEMSIZE);

    /* The number of physical pages is MEMSIZE / PGSIZE same for virtual pages but with MAX */
    num_physical_pages = MEMSIZE / PGSIZE;
    num_virtual_pages = MAX_MEMSIZE / PGSIZE;

    /* We then also malloc bitmaps both for physical and virtual pages */
    physical_bitmap = malloc(num_physical_pages/8);
    if(!physical_bitmap){
        perror("malloc physical_bitmap\n");
        exit(1);
    }
    virtual_bitmap = malloc(num_virtual_pages/8);
    if(!virtual_bitmap){
        perror("malloc virtual_bitmap");
   exit(1);
    }
    memset(physical_bitmap, 0, num_physical_pages/8);
    memset(virtual_bitmap, 0, num_virtual_pages/8);

    /* The page directory is also in the physical memory and is also a page technically */
    pgdir = (pde_t*) physical_memory;
    memset(pgdir, 0, PGSIZE);

    /* Now, because the pgdir will take up the first page of the physical memory, it is important to mark it so in the physical bitmap otherwise it will get overwritten */
    bitmap_set(physical_bitmap, 0);

    /* Let us also clear all the entries of the tlb structure */
    for(int i=0; i<TLB_ENTRIES; i++){
        tlb_store.entries[i].valid = 0;
    }

    vm_initialized = true;
}

// -----------------------------------------------------------------------------
// TLB
// -----------------------------------------------------------------------------

/*
 * TLB_add()
 * ---------
 * Adds a new virtual-to-physical translation to the TLB.
 * Ensure thread safety when updating shared TLB data.
 *
 * Return:
 *   0  -> Success (translation successfully added)
 *  -1  -> Failure (e.g., TLB full or invalid input)
 */
int TLB_add(void *va, void *pa)
{
   // TODO: Implement TLB insertion logic.
    return -1; // Currently returns failure placeholder.
}

/*
 * TLB_check()
 * -----------
 * Looks up a virtual address in the TLB.
 *
 * Return:
 *   Pointer to the corresponding page table entry (PTE) if found.
 *   NULL if the translation is not found (TLB miss).
 */
pte_t *TLB_check(void *va)
{
    // TODO: Implement TLB lookup.
    return NULL; // Currently returns TLB miss.
}

/*
 * print_TLB_missrate()
 * --------------------
 * Calculates and prints the TLB miss rate.
 *
 * Return value: None.
 */
void print_TLB_missrate(void)
{
    double miss_rate = 0.0;
    // TODO: Calculate miss rate as (tlb_misses / tlb_lookups).
    fprintf(stderr, "TLB miss rate %lf \n", miss_rate);
}

// -----------------------------------------------------------------------------
// Page Table
// -----------------------------------------------------------------------------

/*
 * translate()
 * -----------
 * Translates a virtual address to a physical address.
 * Perform a TLB lookup first; if not found, walk the page directory
 * and page tables using a two-level lookup.
 *
 * Return:
 *   Pointer to the PTE structure if translation succeeds.
 *   NULL if translation fails (e.g., page not mapped).
 */
pte_t *translate(pde_t *pgdir, void *va)
{
    // TODO: Extract the 32-bit virtual address and compute indices
    // for the page directory, page table, and offset.
    // Return the corresponding PTE if found.


    /* We need to make sure that the virtual machine is initialized */
    if(!vm_initialized) set_physical_mem();
    
    /* First we will look into the TLB and check whether the VA is currently mapped */

    /* Now we want to take the va* and translate it to the virtual address, which can be  */
    vaddr32_t virtual_address = VA2U(va);
    /* This is just the virtual address, but we are interested in the Virtual Page Number */
    uint32_t vpn = (virtual_address >> VPNSHIFT) & VPNMASK;

    /* Now we have the VPN that we want to search for in the TLB, we will just iterate through it since it is small */
    tlb_lookups++;
    for(int i=0; i<TLB_ENTRIES; i++){
        if(tlb_store.entries[i].vpn == vpn && tlb_store.entries[i].valid){
            /* This means that we found the entry in the TLB, no need to walk the entire translate pipeline */
            /* Since the function needs to return a PTE we need to convert it */
            pde_t pde = pgdir[PDX(va)];
            if(pde==0){
                printf("This VA does not have a page table\n");
                return NULL;
            }
            uint32_t pt_pfn = pde >> PFN_SHIFT;
            pte_t *pt = (pte_t*)(physical_memory + pt_pfn*PGSIZE);
            return &pt[PTX(va)];
        }
    }

    /* If we got here, that means that the tlb does not have the lookup */
    tlb_misses++;
    /* Now we walk through the mapping to get the correct page */
    uint32_t pdx = PDX(va);
    uint32_t ptx = PTX(va);

    /* Now we find the page_directory entry */
    pde_t pde = pgdir[pdx];
    if(pde==0){
        printf("This VA does not have a page table\n");
        return NULL;
    }

    uint32_t pt_pfn = pde >> PFN_SHIFT;
    pte_t *pt = (pte_t*) (physical_memory + pt_pfn*PGSIZE);

    /* Now that we have the page table entry we need, we will use the Page Table index to get the final physical address of the data */
    pte_t pte = pt[ptx];
    if(pte==0){
        printf("There is no mapping for this VA\n");
        return NULL;
    }
    /* We need to udnerstand what the page table endty looks like 20 bits of PFN*/
    return &pt[ptx];
}

/*
 * map_page()
 * -----------
 * Establishes a mapping between a virtual and a physical page.
 * Creates intermediate page tables if necessary.
 *
 * Return:
 *   0  -> Success (mapping created)
 *  -1  -> Failure (e.g., no space or invalid address)
 */
int map_page(pde_t *pgdir, void *va, void *pa)
{
    // TODO: Map virtual address to physical address in the page tables.

    /* First let's store the addresses as 32 bit ins */
    vaddr32_t virtual_address = VA2U(va);
    paddr32_t physical_address = VA2U(pa);

    /* We also need the Page directory index and the table index that the virtual address is trying to map into */
    uint32_t pde_index = PDX(va);
    uint32_t ptx = PTX(va);

    /* And the actual physical frame nymber of the data */
    uint32_t data_pfn = physical_address >> PFN_SHIFT;

    /* Now we look at the index in the page directory specified by the pde_index */
    pde_t pde = pgdir[pde_index];

    if(pde!=){
        printf("The page table already exists");
    }

    return -1; // Failure placeholder.
}

// -----------------------------------------------------------------------------
// Allocation
// -----------------------------------------------------------------------------

/*
 * get_next_avail()
 * ----------------
 * Finds and returns the base virtual address of the next available
 * block of contiguous free pages.
 *
 * Return:
 *   Pointer to the base virtual address if available.
 *   NULL if there are no sufficient free pages.
 */
void *get_next_avail(int num_pages)
{
    // TODO: Implement virtual bitmap search for free pages.
    return NULL; // No available block placeholder.
}

/*
 * n_malloc()
 * -----------
 * Allocates a given number of bytes in virtual memory.
 * Initializes physical memory and page directories if not already done.
 *
 * Return:
 *   Pointer to the starting virtual address of allocated memory (success).
 *   NULL if allocation fails.
 */
void *n_malloc(unsigned int num_bytes)
{
    // TODO: Determine required pages, allocate them, and map them.
    return NULL; // Allocation failure placeholder.
}

/*
 * n_free()
 * ---------
 * Frees one or more pages of memory starting at the given virtual address.
 * Marks the corresponding virtual and physical pages as free.
 * Removes the translation from the TLB.
 *
 * Return value: None.
 */
void n_free(void *va, int size)
{
    // TODO: Clear page table entries, update bitmaps, and invalidate TLB.
    


}

// -----------------------------------------------------------------------------
// Data Movement
// -----------------------------------------------------------------------------

/*
 * put_data()
 * ----------
 * Copies data from a user buffer into simulated physical memory using
 * the virtual address. Handle page boundaries properly.
 *
 * Return:
 *   0  -> Success (data written successfully)
 *  -1  -> Failure (e.g., translation failure)
 */
int put_data(void *va, void *val, int size)
{
    // TODO: Walk virtual pages, translate to physical addresses,
    // and copy data into simulated memory.
    



    return -1; // Failure placeholder.
}

/*
 * get_data()
 * -----------
 * Copies data from simulated physical memory (accessed via virtual address)
 * into a user buffer.
 *
 * Return value: None.
 */
void get_data(void *va, void *val, int size)
{
    // TODO: Perform reverse operation of put_data().
    //
}

// -----------------------------------------------------------------------------
// Matrix Multiplication
// -----------------------------------------------------------------------------

/*
 * mat_mult()
 * ----------
 * Performs matrix multiplication of two matrices stored in virtual memory.
 * Each element is accessed and stored using get_data() and put_data().
 *
 * Return value: None.
 */
void mat_mult(void *mat1, void *mat2, int size, void *answer)
{
    int i, j, k;
    uint32_t a, b, c;

    for (i = 0; i < size; i++) {
        for (j = 0; j < size; j++) {
            c = 0;
            for (k = 0; k < size; k++) {
                // TODO: Compute addresses for mat1[i][k] and mat2[k][j].
                // Retrieve values using get_data() and perform multiplication.
                get_data(NULL, &a, sizeof(int));  // placeholder
                get_data(NULL, &b, sizeof(int));  // placeholder
                c += (a * b);
            }
            // TODO: Store the result in answer[i][j] using put_data().
            put_data(NULL, (void *)&c, sizeof(int)); // placeholder
        }
    }
}

