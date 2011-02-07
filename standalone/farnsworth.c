/* -*- Mode: C -*- */

#include <pci.h>
#include <mbi.h>
#include <util.h>
#include <elf.h>
#include <version.h>
#include <serial.h>

int
main(uint32_t magic, struct mbi *mbi)
{
  serial_init();

  if (magic != MBI_MAGIC) {
    printf("Not loaded by Multiboot-compliant loader. Bye.\n");
    return 1;
  }

  printf("\nFarnsworth %s\n", version_str);
  printf("Blame Julian Stecklina <jsteckli@os.inf.tu-dresden.de> for bugs.\n\n");

  printf("MBI Modules List:\n");
  struct module *mods = (struct module *)mbi->mods_addr;
  for (unsigned i = 0; i < mbi->mods_count; i++) {
    const char *s = (const char *)mods->string;
    printf("  %2u: start %8x end %8x '%s'\n", i, mods[i].mod_start, mods[i].mod_end, s);
  }


  printf("\nMBI Memory Map:\n");
  size_t mmap_len    = mbi->mmap_length;
  memory_map_t *mmap = (memory_map_t *)mbi->mmap_addr;
  
  while ((uint32_t)mmap < mbi->mmap_addr + mmap_len) {
    uint64_t base = (uint64_t)mmap->base_addr_low | ((uint64_t)mmap->base_addr_high<<32);
    uint64_t len  = (uint64_t)mmap->length_low | ((uint64_t)mmap->length_high<<32);

    printf(" type %2x start %8llx end %8llx len %8llx\n", mmap->type, base, base+len-1, len);

    /* Skip to next entry. */
    mmap = (memory_map_t *)(mmap->size + (uint32_t)mmap + sizeof(mmap->size));
  }

  return 0;
}
