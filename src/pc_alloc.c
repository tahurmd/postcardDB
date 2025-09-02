#include "pc_alloc.h"

pc_result_t pc_alloc_init(pc_alloc_t *a, pc_flash_t *f)
{
  if (!a || !f)
    return PC_EINVAL;
  a->f = f;
  a->seg_bytes = pc_flash_sector_bytes(f);
  a->prog_bytes = pc_flash_prog_bytes(f);
  if (a->seg_bytes == 0)
    return PC_EINVAL;
  a->sector_count = pc_flash_total(f) / a->seg_bytes;
  a->next_index = 0;
  return PC_OK;
}

pc_result_t pc_alloc_acquire(pc_alloc_t *a, size_t *out_base)
{
  if (!a || !out_base)
    return PC_EINVAL;
  if (a->sector_count == 0)
    return PC_EINVAL;

  for (size_t step = 0; step < a->sector_count; ++step)
  {
    size_t idx = (a->next_index + step) % a->sector_count;
    if (pc_flash_is_bad(a->f, idx))
      continue;

    size_t base = idx * a->seg_bytes;
    // "Free" means commit page is fully erased (no header)
    if (pc_logseg_header_erased(a->f, base))
    {
      // Choose this one and advance pointer for next time
      a->next_index = (idx + 1) % a->sector_count;
      *out_base = base;
      return PC_OK;
    }
  }
  return PC_NO_SPACE;
}
