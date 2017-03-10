#include "vm/swap.h"
#include "devices/block.h"
#include "threads/vaddr.h"

static struct block *swap_dev;  /* The swap device */
static const int sectors_per_page = PGSIZE / BLOCK_SECTOR_SIZE;
static int num_slots;           /* Total number of swap slots */

void swap_init(void) {
  swap_dev = block_get_role(BLOCK_SWAP);
  num_slots = block_size(swap_dev) / sectors_per_page;
}
