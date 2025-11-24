#ifndef MY_VM_H_INCLUDED
#define MY_VM_H_INCLUDED

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* ============================================================================
 *  Virtual Memory Simulation Header
 * ============================================================================
 *  This header defines constants, data types, and function prototypes
 *  for implementing a simulated 32-bit virtual memory system.
 *
 *  Students will:
 *   - Fill in missing constants and macros for address translation.
 *   - Define TLB structure and page table entry fields.
 *   - Implement all declared functions in my_vm.c.
 *
 *  Return conventions (used across functions):
 *    0   → Success
 *   -1   → Failure
 *   NULL → Translation or lookup not found
 * ============================================================================
 */

// -----------------------------------------------------------------------------
//  Memory and Paging Configuration
// -----------------------------------------------------------------------------

#define VA_BITS 32u  // Simulated virtual address width
#define PGSIZE 4096u // Page size = 4 KB

#define MAX_MEMSIZE (1ULL << 32) // Max virtual memory = 4 GB
#define MEMSIZE (1ULL << 30)     // Simulated physical memory = 1 GB

// COMPLETE HERE

// --- Constants for bit shifts and masks ---
#define PDXSHIFT                                                               \
  22 /** TODO: number of bits to shift for directory index - Ans - 4KB VA      \
        layout is [ 10 bits directory | 10 bits table | 12 bits offset ]       \
        therefore to get the directory bits which are 10 in length, we need to \
        shift 22 from the left. Basically we are right shifting away the table \
        and offset bits**/
#define PTXSHIFT                                                               \
  12 /** TODO: number of bits to shift for table index Ans - Same as above but \
        for the table bits**/
#define PXMASK                                                                 \
  0x3FF /** TODO: Ans - This is because both the directory and table bits are  \
           10 bits, meaning that we want to mask off everything other than the \
           10 LSBits  **/
#define OFFMASK 0xFFF /** TODO: - This is to get the 12LSBits directly**/

/* Also defining VPNSHIFT and VPNMASK for translate() function so that it is
 * cleaner */
#define VPNSHIFT 12

#define VPNMASK 0xFFFF // This is 20 1s in binary

// --- Macros to extract address components ---
#define PDX(va)                                                                \
  ((VA2U(va) >> PDXSHIFT) &                                                    \
   PXMASK) /** TODO: compute directory index from virtual address **/
#define PTX(va)                                                                \
  ((VA2U(va) >> PTXSHIFT) &                                                    \
   PXMASK) /** TODO: compute table index from virtual address **/
#define OFF(va)                                                                \
  ((VA2U(va)) & OFFMASK) /** TODO: compute page offset from virtual address    \
                          **/

// -----------------------------------------------------------------------------
//  Type Definitions
// -----------------------------------------------------------------------------

typedef uint32_t vaddr32_t; // Simulated 32-bit virtual address
typedef uint32_t paddr32_t; // Simulated 32-bit physical address
typedef uint32_t pte_t;     // Page table entry
typedef uint32_t pde_t;     // Page directory entry

// -----------------------------------------------------------------------------
//  Page Table Flags (Students fill as needed)
// -----------------------------------------------------------------------------

#define PFN_SHIFT                                                              \
  12 /** TODO: number of bits to shift: Ans - The format again of each page    \
        table entry is [20 bits of the PFN | 12 bits of the flags] therefore   \
        we need to right shift away the flag bits to get the PFN**/
#define VALID 0x1

// -----------------------------------------------------------------------------
//  Address Conversion Helpers (Provided)
// -----------------------------------------------------------------------------

static inline vaddr32_t VA2U(void *va) { return (vaddr32_t)(uintptr_t)va; }
static inline void *U2VA(vaddr32_t u) { return (void *)(uintptr_t)u; }

// -----------------------------------------------------------------------------
//  TLB Configuration
// -----------------------------------------------------------------------------

#define TLB_ENTRIES 512 // Default number of TLB entries

struct tlb_entry {
  uint32_t vpn; // This is the Virtual Page Number of the entry
  uint32_t pfn; // The page Frame Number of the entry
  uint8_t valid;
};

struct tlb {
  /*
   * TODO: Define the TLB structure.
   * Each entry typically includes:
   *  - Virtual Page Number (VPN)
   *  - Physical Frame Number (PFN)
   *  - Valid bit
   *  - Optional timestamp for replacement policy (e.g., LRU)
   *
   * Example:
   *   uint32_t vpn;
   *   uint32_t pfn;
   *   bool valid;
   *   uint64_t last_used;
   *
   * Ans: What I am going to be doing is making another struct tlb_entry and
   * make the tlb struct a container of TLB_ENTRIES number of tlb_entry
   */
  struct tlb_entry entries[TLB_ENTRIES];
};

extern struct tlb tlb_store;

// -----------------------------------------------------------------------------
//  Function Prototypes
// -----------------------------------------------------------------------------

/*
 * Initializes physical memory and supporting data structures.
 * Return: None.
 */
void set_physical_mem(void);

/*
 * Adds a new virtual-to-physical translation to the TLB.
 * Return: 0 on success, -1 on failure.
 */
int TLB_add(void *va, void *pa);

/*
 * Checks if a virtual address translation exists in the TLB.
 * Return: pointer to PTE on hit; NULL on miss.
 */
pte_t *TLB_check(void *va);

/*
 * Calculates and prints the TLB miss rate.
 * Return: None.
 */
void print_TLB_missrate(void);

/*
 * Translates a virtual address to a physical address.
 * Return: pointer to PTE if successful; NULL otherwise.
 */
pte_t *translate(pde_t *pgdir, void *va);

/*
 * Creates a mapping between a virtual and a physical page.
 * Return: 0 on success, -1 on failure.
 */
int map_page(pde_t *pgdir, void *va, void *pa);

/*
 * Finds the next available block of contiguous virtual pages.
 * Return: pointer to base virtual address on success; NULL if unavailable.
 */
void *get_next_avail(int num_pages);

/*
 * Allocates memory in the simulated virtual address space.
 * Return: pointer to base virtual address on success; NULL on failure.
 */
void *n_malloc(unsigned int num_bytes);

/*
 * Frees one or more pages of memory starting from the given virtual address.
 * Return: None.
 */
void n_free(void *va, int size);

/*
 * Copies data from a user buffer into simulated physical memory
 * through a virtual address.
 * Return: 0 on success, -1 on failure.
 */
int put_data(void *va, void *val, int size);

/*
 * Copies data from simulated physical memory into a user buffer.
 * Return: None.
 */
void get_data(void *va, void *val, int size);

/*
 * Performs matrix multiplication using data stored in simulated memory.
 * Each element should be accessed via get_data() and stored via put_data().
 * Return: None.
 */
void mat_mult(void *mat1, void *mat2, int size, void *answer);

#endif // MY_VM_H_INCLUDED
