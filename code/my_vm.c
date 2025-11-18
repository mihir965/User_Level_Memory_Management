
#include "my_vm.h"
#include <cstddef>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h> // optional for memcpy if you later implement put/get

// -----------------------------------------------------------------------------
// Global Declarations (optional)
// -----------------------------------------------------------------------------
static uint8_t *physical_mem = NULL; // MEMSIZE bytes
static pde_t *pgdir = NULL;          // this is the top-level directory
static uint8_t *phys_bitmap = NULL;  // one bit per phys page
static uint8_t *virt_bitmap = NULL;  // one bit per virtual page
static size_t num_phys_pages = 0;
static size_t num_virt_pages = 0;
static bool vm_initialized = false;

static pthread_mutex_t vm_lock = PTHREAD_MUTEX_INITIALIZER;

/* Some bit map helper functions */
static inline void bitmap_set(uint8_t *bm, size_t idx) {
  bm[idx / 8] |= (1u << (idx % 8));
}

static inline void bitmap_clear(uint8_t *bm, size_t idx) {
  bm[idx / 8] &= ~(1u << (idx % 8));
}

static inline int bitmap_test(uint8_t *bm, size_t idx) {
  return (bm[idx / 8] >> (idx % 8)) & 1u;
}

/* Adding some functions to help with get_next_avail and n_malloc() by making the traversal through bitmap cleaner*/
static long bitmap_find_free(uint8_t *bm, size_t num_pages){
    for(size_t i=0; i<num_pages; i++){
        if(!bitmap_test(bm, i)) return i;
    }
    return -1;
}

struct tlb tlb_store[TLB_ENTRIES]; // Placeholder for your TLB structure

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
  pthread_mutex_lock(&vm_lock);
  if (vm_initialized) {
    printf("VM already initialized\n");
    pthread_mutex_unlock(&vm_lock);
    return;
  }
  num_phys_pages = MEMSIZE / PGSIZE;
  num_virt_pages = MAX_MEMSIZE / PGSIZE;

  physical_mem = malloc(MEMSIZE);
  if (!physical_mem) {
    perror("malloc physical_mem\n");
    exit(1);
  }

  memset(physical_mem, 0, MEMSIZE);

  size_t phys_bm_bytes = (num_phys_pages + 7) / 8;
  size_t virt_bm_bytes = (num_virt_pages + 7) / 8;

  phys_bitmap = calloc(1, phys_bm_bytes);
  virt_bitmap = calloc(1, virt_bm_bytes);
  if (!phys_bitmap || !virt_bitmap) {
    perror("calloc bitmaps\n");
    exit(1);
  }

  /* We use physical page 0 for the page directory */
  pgdir = (pde_t *)physical_mem;
  bitmap_set(phys_bitmap, 0);

  /* Clear the TLB */
  for (int i = 0; i < TLB_ENTRIES; i++) {
    tlb_store[i].valid = false;
  }

  vm_initialized = true;
  pthread_mutex_unlock(&vm_lock);
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
  vaddr32_t v = VA2U(va);
  uint32_t vpn = v >> PFN_SHIFT;

  paddr32_t p = (paddr32_t)(uintptr_t)pa; // we treat pa as 32 bit simulated
  uint32_t pfn = p >> PFN_SHIFT;

  uint32_t idx = vpn % TLB_ENTRIES;

  pthread_mutex_lock(&vm_lock);
  tlb_store[idx].vpn = vpn;
  tlb_store[idx].pfn = pfn;
  tlb_store[idx].valid = true;
  pthread_mutex_unlock(&vm_lock);

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
pte_t *TLB_check(void *va) {
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
void print_TLB_missrate(void) {
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
pte_t *translate(pde_t *pgdir, void *va) {
  // TODO: Extract the 32-bit virtual address and compute indices
  // for the page directory, page table, and offset.
  // Return the corresponding PTE if found.
  return NULL; // Translation unsuccessful placeholder.
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
void *get_next_avail(int num_pages) {
  // TODO: Implement virtual bitmap search for free pages.
  pthread_mutex_lock(&vm_lock);
  for(size_t start=0; start<num_virt_pages; start++){
      /* Check if starting at start, there are num_pages free */
      bool ok = true;
      for(int pg=0; pg<num_pages; pg++){
          if(bitmap_test(virt_bitmap, start+pg)){
              ok = false;
              break;
          }
      }
      if(ok){
          for(int pg=0; pg<num_pages; pg++) bitmap_set(virt_bitmap, start+pg);
          pthread_mutex_unlock(&vm_lock);
          return U2VA(start*PGSIZE);
      }
  }
  pthread_mutex_unlock(&vm_lock);
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
void n_free(void *va, int size) {
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
int put_data(void *va, void *val, int size) {
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
void get_data(void *va, void *val, int size) {
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
void mat_mult(void *mat1, void *mat2, int size, void *answer) {
  int i, j, k;
  uint32_t a, b, c;

  for (i = 0; i < size; i++) {
    for (j = 0; j < size; j++) {
      c = 0;
      for (k = 0; k < size; k++) {
        // TODO: Compute addresses for mat1[i][k] and mat2[k][j].
        // Retrieve values using get_data() and perform multiplication.
        get_data(NULL, &a, sizeof(int)); // placeholder
        get_data(NULL, &b, sizeof(int)); // placeholder
        c += (a * b);
      }
      // TODO: Store the result in answer[i][j] using put_data().
      put_data(NULL, (void *)&c, sizeof(int)); // placeholder
    }
  }
}
