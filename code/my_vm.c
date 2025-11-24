
#include "my_vm.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h> // optional for memcpy if you later implement put/get
#include <sys/types.h>
#include <pthread.h>

// -----------------------------------------------------------------------------
// Global Declarations (optional)
// -----------------------------------------------------------------------------
static uint8_t *physical_memory = NULL; // A contiguous array of bytes of memory
static pde_t *pgdir = NULL; // This is the top-level page directory which will
                            // map to the Page Table
static uint8_t *physical_bitmap =
NULL; // A bit map array for mapping each physical page
static uint8_t *virtual_bitmap =
NULL; // A bit map array for mapping each virtual page - much more than the
      // number of physical pages 4x
static size_t num_physical_pages = 0; // This will be 1GB / 4KB
static size_t num_virtual_pages = 0;  // This is 4GB / 4KB
static bool vm_initialized = false;

static pthread_mutex_t vm_lock = PTHREAD_MUTEX_INITIALIZER;

static int tlb_next = 0; // Index for round-robin replacement
struct tlb tlb_store;    // Placeholder for your TLB structure

/* Some bitmap helper functions */
static inline void bitmap_set(uint8_t *bm, size_t idx) {
    bm[idx / 8] |= (1 << (idx % 8));
}

static inline int bitmap_check(uint8_t *bm, size_t idx) {
    return (bm[idx / 8] >> (idx % 8)) & 1;
}

static inline void bitmap_clear(uint8_t *bm, size_t idx) {
    bm[idx / 8] &= ~(1 << (idx % 8));
}

// Optional counters for TLB statistics
static unsigned long long tlb_lookups = 0;
static unsigned long long tlb_misses = 0;

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
    // printf("[DEBUG]: set_physical_mem()\n");
    if (vm_initialized) {
        // printf("VM already initialized\n");
        return;
    }

    /* First we allocate the simulated physical ram */
    physical_memory = malloc(MEMSIZE);
    if (!physical_memory) {
        // perror("malloc physical_memory\n");
        exit(1);
    }
    /* We also set everything to 0 - initialization */
    memset(physical_memory, 0, MEMSIZE);

    /* The number of physical pages is MEMSIZE / PGSIZE same for virtual pages but
     * with MAX */
    num_physical_pages = MEMSIZE / PGSIZE;
    num_virtual_pages = MAX_MEMSIZE / PGSIZE;

    /* We then also malloc bitmaps both for physical and virtual pages */
    physical_bitmap = malloc(num_physical_pages / 8);
    if (!physical_bitmap) {
        // perror("malloc physical_bitmap\n");
        exit(1);
    }
    virtual_bitmap = malloc(num_virtual_pages / 8);
    if (!virtual_bitmap) {
        // perror("malloc virtual_bitmap");
        exit(1);
    }
    memset(physical_bitmap, 0, num_physical_pages / 8);
    memset(virtual_bitmap, 0, num_virtual_pages / 8);

    /* The page directory is also in the physical memory and is also a page
     * technically */
    pgdir = (pde_t *)physical_memory;
    memset(pgdir, 0, PGSIZE);

    /* Now, because the pgdir will take up the first page of the physical memory,
     * it is important to mark it so in the physical bitmap otherwise it will get
     * overwritten */
    bitmap_set(physical_bitmap, 0);
    bitmap_set(virtual_bitmap, 0);

    /* Let us also clear all the entries of the tlb structure */
    for (int i = 0; i < TLB_ENTRIES; i++) {
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
int TLB_add(void *va, void *pa) {
    // TODO: Implement TLB insertion logic.

    if (!vm_initialized) {
        set_physical_mem();
    }

    if (!va || !pa) {
        // perror("Nothing here\n");
        return -1;
    }

    vaddr32_t v = VA2U(va);
    paddr32_t p = VA2U(pa);

    uint32_t vpn = v >> PFN_SHIFT;
    uint32_t pfn = p >> PFN_SHIFT;

    tlb_store.entries[tlb_next].vpn = vpn;
    tlb_store.entries[tlb_next].pfn = pfn;
    tlb_store.entries[tlb_next].valid = 1;

    tlb_next = (tlb_next + 1) % TLB_ENTRIES;

    return 0; // Currently returns failure placeholder.
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
pte_t *TLB_check(void *va) {
    // TODO: Implement TLB lookup.
    // printf("[DEBUG]: TLB_check()\n");
    if(!vm_initialized){
        return NULL;
    }

    vaddr32_t virtual_address = VA2U(va);
    uint32_t vpn = virtual_address >> VPNSHIFT;

    tlb_lookups++;

    for(int i=0; i<TLB_ENTRIES; i++){
        if(tlb_store.entries[i].valid && tlb_store.entries[i].vpn == vpn){
            /* Then we reconstruct the physical address */
            uint32_t pfn = tlb_store.entries[i].pfn;
            uint32_t offset = OFF(va);
            paddr32_t pa = (pfn << PFN_SHIFT) | offset;

            /* We need to return a pte_t* so get it from the page table */
            pde_t pde = pgdir[PDX(va)];
            if(pde==0){
                // fprintf(stderr, "No page Directory Entry mapped\n");
                return NULL;
            }
            uint32_t pt_pfn = pde >> PFN_SHIFT;
            pte_t* pt = (pte_t *)(physical_memory + pt_pfn*PGSIZE);
            return &pt[PTX(va)];
        }
    }

    tlb_misses++;
    return NULL; // Currently returns TLB miss.
}

/*
 * print_TLB_missrate()
 * --------------------
 * Calculates and prints the TLB miss rate.
 *
 * Return value: None.
 */
void print_TLB_missrate(void) {
    double miss_rate = 0.0;
    // TODO: Calculate miss rate as (tlb_misses / tlb_lookups).
    if(tlb_lookups>0){
        miss_rate = (double)tlb_misses / (double)tlb_lookups;
    }
    fprintf(stderr, "TLB miss rate %lf \n", miss_rate);
    fprintf(stderr, "Totaal lookups: %llu, Total misses: %llu\n", tlb_lookups, tlb_misses);
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
pte_t *translate(pde_t *pgdir, void *va) {
    // TODO: Extract the 32-bit virtual address and compute indices
    // for the page directory, page table, and offset.
    // Return the corresponding PTE if found.

    /* We need to make sure that the virtual machine is initialized */ 
    // printf("[DEBUG]: translate()\n");
    if (!vm_initialized)
        set_physical_mem();

    /* First we will look into the TLB and check whether the VA is currently
     * mapped */
    pte_t* pte_from_table = TLB_check(va);
    /* Basically if we find the pte from the TLB, we don't need to make all the address translations of the virtual address */
    if(pte_from_table){
        // printf("[DEBUG]: Found the Page Table intry from TLB");
        return pte_from_table;
    }

    /* This means that it is a TLB miss */
    uint32_t pdx = PDX(va);
    uint32_t ptx = PTX(va);

    pde_t pde = pgdir[pdx];
    if(pde==0){
        // fprintf(stderr, "No Page Directory Entry mapped\n");
        return NULL;
    }

    uint32_t pt_pfn = pde >> PFN_SHIFT;
    /* This will basically give us the starting address of the page in the page directory, from which we will find the exact page table entry */
    pte_t *pt = (pte_t*)(physical_memory+pt_pfn*PGSIZE);

    pte_t pte = pt[ptx];
    if(pte==0){
        // fprintf(stderr, "No Page Table Entry mapped\n");
        return NULL;
    }

    /* Add to the TLB for future lookups */
    vaddr32_t v = VA2U(va);
    uint32_t vpn = v >> VPNSHIFT;
    uint32_t pfn = pte >> PFN_SHIFT;
    void *pa = (void*)(uintptr_t)((pfn<<PFN_SHIFT)|OFF(va));
    TLB_add(va, pa);
    // printf("[DEBUG]: returning from translate\n");
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
int map_page(pde_t *pgdir, void *va, void *pa) {
    // TODO: Map virtual address to physical address in the page tables.

    /* First let's store the addresses as 32 bit ins */

    // printf("[DEBUG]: map_page()\n");

    vaddr32_t virtual_address = VA2U(va);
    paddr32_t physical_address = VA2U(pa);

    /* We also need the Page directory index and the table index that the virtual
     * address is trying to map into */
    uint32_t pde_index = PDX(va);
    uint32_t ptx = PTX(va);

    /* And the actual physical frame nymber of the data */
    uint32_t data_pfn = physical_address >> PFN_SHIFT;

    /* Now we look at the index in the page directory specified by the pde_index
    */
    pde_t pde = pgdir[pde_index];

    if (pde != 0) {
        // printf("The page table already exists\n");
    } else {
        /* We need to allocate a new physical page_table in the pgdir */

        /* So first we need to go over all the physical_memory array to find the
         * first page that is unallocated, we do this using the bitmap */
        /*This will be the page table physical frame number */
        // printf("[DEBUG]: Mapping page table entry\n");
        uint32_t free_pfn;
        bool page_found = false;
        for (int pfn = 0; pfn < num_physical_pages; pfn++) {
            if (!bitmap_check(physical_bitmap, pfn)) {
                /* This means that the chunk is free */
                free_pfn = pfn;
                bitmap_set(physical_bitmap, pfn);
                page_found = true;
                break;
            }
        }
        /* This means that no available chunk was found */
        if (!page_found) {
            // printf("memory has been exhausted\n");
            return -1;
        }
        void *pt_addr = physical_memory + free_pfn * PGSIZE;
        memset(pt_addr, 0, PGSIZE);

        /* Now we create the page_drectory entry */
        pde = (free_pfn << PFN_SHIFT) | VALID;
        pgdir[pde_index] = pde;
        // printf("[DEBUG]: Created PDE at index %u: PDE=0x%x, pt_pfn=%u\n", pde_index, pde, free_pfn);
    }
    /* Now we have a nonzero PDE, and want to map the PTE */

    /* Let's extract the Page Frame Number of the Page Table its that we created
    */
    uint32_t pt_pfn = pde >> PFN_SHIFT; // Again we just got the PFN by right
                                        // shifting the 12 bits fo the Flags

    /* This will be an array of 1024 contiguous page table entries in the physical
     * ememory */
    pte_t *pt = (pte_t *)(physical_memory + pt_pfn * PGSIZE);

    /* Now we set the PTE for this virtual page as well, the same way we did for
     * the page directory entry */
    pt[ptx] = (data_pfn << PFN_SHIFT) | VALID;

    // printf("[DEBUG]: Set PTE: pgdir[%u]->pt[%u] = 0x%x (data_pfn=%u)\n", pde_index, ptx, pt[ptx], data_pfn);

    /* We should also change the virtual bitmap accoridngly */
    /* The virtual page number will be  */
    uint32_t vpn = virtual_address >> PFN_SHIFT;
    bitmap_set(virtual_bitmap, vpn);

    /* Update the TLB */
    if (TLB_add(va, pa)<0) {
        // fprintf(stderr, "TLB add failed\n");
        return -1;
    }
    return 0;
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
void *get_next_avail(int num_pages) {
    // TODO: Implement virtual bitmap search for free pages.

    // printf("[DEBUG]: get_next_avail\n");

    /* We basically want to scan through the virtual_bitmap and check if we have
     * num_pages amount of pages available to be allocated */
    for (uint32_t vpn = 0; vpn <= num_virtual_pages - num_pages; vpn++) {
        bool all_free = true;
        for (int j = 0; j < num_pages; j++) {
            if (bitmap_check(virtual_bitmap, vpn + j)) {
                // printf("bitmap_check failed, all_free false\n");
                all_free = false;
                break;
            }
        }
        if (all_free){
            // printf("[DEBUG]: exiting get_next_avail\n");
            // printf("%p: \n", U2VA(vpn*PGSIZE));
            return U2VA(vpn * PGSIZE);
        }
    }
    // printf("No available block\n");
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
void *n_malloc(unsigned int num_bytes) {
    // TODO: Determine required pages, allocate them, and map them.
    // printf("[DEBUG]: n_malloc\n");
    pthread_mutex_lock(&vm_lock);

    if(!vm_initialized){
        // printf("[DEBUG]: vm not initialized, running set_physical_mem\n");
        set_physical_mem();
    }

    int num_pages = (num_bytes + PGSIZE - 1) / PGSIZE;

    /* We find the next bit of virtual memory from the 4GB that we can use for the
     * n_malloc function */
    void *base_va = get_next_avail(num_pages);
    if (!base_va){
        // printf("[DEBUG]: base_va was null\n");
        pthread_mutex_unlock(&vm_lock);
        return NULL;
    }

    uint32_t base = VA2U(base_va);

    for (int i = 0; i < num_pages; i++) {
        void *curr_va = U2VA(base + i * PGSIZE);

        /* Allocate the physical page */
        uint32_t data_pfn;
        bool found = false;

        for (uint32_t pfn = 0; pfn < num_physical_pages; pfn++) {
            if (!bitmap_check(physical_bitmap, pfn)) {
                data_pfn = pfn;
                bitmap_set(physical_bitmap, pfn);
                found = true;
                break;
            }
        }
        if (!found){
            pthread_mutex_unlock(&vm_lock);
            return NULL;
        }

        void *pa = U2VA(data_pfn*PGSIZE);

        /* Map the VA to the physial page */
        if (map_page(pgdir, curr_va, pa) < 0) {
            // printf("[DEBUG]: map_page failed\n");
            pthread_mutex_unlock(&vm_lock);
            return NULL;
        }
    }
    // printf("[DEBUG]: exiting n_malloc\n");
    pthread_mutex_unlock(&vm_lock);
    return base_va;
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
void n_free(void *va, int size) {
    // TODO: Clear page table entries, update bitmaps, and invalidate TLB.

    /* Similar to n_malloc we first determine how many pages we want to free - and
     * there fore how many page table entries */
    pthread_mutex_lock(&vm_lock);
    int num_pages = (size + PGSIZE - 1) / PGSIZE;

    vaddr32_t base = VA2U(va);
    for (int i = 0; i < num_pages; i++) {
        void *curr_va = U2VA(base + i * PGSIZE);
        pte_t *pte = translate(pgdir, curr_va);
        if (pte == NULL || *pte == 0) {
            // printf("This virtual_address is either not mapped or is already freed\n");
            continue;
        } else {
            uint32_t entry = *pte;
            uint32_t data_pfn = entry >> PFN_SHIFT;

            *pte = 0;
            bitmap_clear(physical_bitmap, data_pfn);

            vaddr32_t curr = VA2U(curr_va);
            uint32_t vpn = curr >> PFN_SHIFT;
            bitmap_clear(virtual_bitmap, vpn);

            for (int j = 0; j < TLB_ENTRIES; j++) {
                if (tlb_store.entries[j].valid && tlb_store.entries[j].vpn == vpn) {
                    tlb_store.entries[j].valid = 0;
                }
            }
        }
    }
    pthread_mutex_unlock(&vm_lock);
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
int put_data(void *va, void *val, int size) {
    // TODO: Walk virtual pages, translate to physical addresses,
    // and copy data into simulated memory.

    /* The write may cross page boundaries meaning that the data may have to be
     * written across different pages in the simulated physical memory, therefore
     * we cannot just choose one page by doing translate, we have to keep going
     * untill all the size has been exhauseted */

    pthread_mutex_lock(&vm_lock);
    int remaining = size;

    vaddr32_t curr = VA2U(va);
    uint8_t *src = (uint8_t *)val;
    void *curr_va = va;

    while (remaining > 0) {
        pte_t *pte = translate(pgdir, curr_va);
        if (!pte || *pte == 0) {
            /* This means that there was no mapping for this pte and the translate
             * failed */
            // perror("translate: VA not mapped\n");
            pthread_mutex_unlock(&vm_lock);
            return -1;
        }
        uint32_t entry = *pte;
        uint32_t pfn = entry >> PFN_SHIFT;

        /* We get to the actual page in the simulated physical memory, now after
         * this we have to find the offset where we want to put this in the page */
        uint8_t *page_base = physical_memory + pfn * PGSIZE;

        /* offset */
        uint32_t offset = OFF(curr_va);
        uint8_t *dst = page_base + offset;

        int capacity = PGSIZE - offset;
        int to_cpy = (remaining < capacity) ? remaining : capacity;

        memcpy(dst, src, to_cpy);

        remaining -= to_cpy;
        src += to_cpy;
        curr += to_cpy;
        curr_va = U2VA(curr);
    }
    pthread_mutex_unlock(&vm_lock);
    return 0;
}

/*
 * get_data()
 * -----------
 * Copies data from simulated physical memory (accessed via virtual address)
 * into a user buffer.
 *
 * Return value: None.
 */
void get_data(void *va, void *val, int size) {
    // TODO: Perform reverse operation of put_data().
    pthread_mutex_lock(&vm_lock);
    int remaining = size;
    vaddr32_t curr = VA2U(va);
    uint8_t *dst = (uint8_t *)val;
    void *curr_va = va;

    while (remaining > 0) {
        pte_t *pte = translate(pgdir, curr_va);
        if (!pte || *pte == 0) {
            // perror("No mapping in translate\n");
            pthread_mutex_unlock(&vm_lock);
            return;
        }
        uint32_t entry = *pte;
        uint32_t pfn = entry >> PFN_SHIFT;
        uint8_t *page_base = physical_memory + pfn * PGSIZE;

        uint32_t offset = OFF(curr_va);
        uint8_t *src = page_base + offset;

        int capacity = PGSIZE - offset;
        int to_cpy = (remaining < capacity) ? remaining : capacity;

        memcpy(dst, src, to_cpy);

        remaining -= to_cpy;
        dst += to_cpy;
        curr += to_cpy;
        curr_va = U2VA(curr);
    }
    pthread_mutex_unlock(&vm_lock);
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
void mat_mult(void *mat1, void *mat2, int size, void *answer) {
    int i, j, k;
    uint32_t a, b, c;

    for (i = 0; i < size; i++) {
        for (j = 0; j < size; j++) {
            c = 0;
            for (k = 0; k < size; k++) {
                // TODO: Compute addresses for mat1[i][k] and mat2[k][j].
                // Retrieve values using get_data() and perform multiplication.

                /* Compute VA of mat1[i][k] */
                void *addr_A =
                    (void *)((uintptr_t)mat1 + ((i * size) + k) * sizeof(uint32_t));

                /* Compute VA of mat2[k][j] */
                void *addr_B =
                    (void *)((uintptr_t)mat2 + ((k * size) + j) * sizeof(uint32_t));

                get_data(addr_A, &a, sizeof(int)); // placeholder
                get_data(addr_B, &b, sizeof(int)); // placeholder
                c += (a * b);
            }
            // TODO: Store the result in answer[i][j] using put_data().
            void *addr_C =
                (void *)((uintptr_t)answer + ((i * size) + j) * sizeof(uint32_t));

            put_data(addr_C, (void *)&c, sizeof(int)); // placeholder
        }
    }
}
