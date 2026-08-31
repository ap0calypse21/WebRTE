/* 1352_offsets.h - offsets for firmware 13.52
 * Sources:
 *  - Scene-Collective/ps4-hen kpayload/source/offsets/1352.c
 *  - ps4-payload-dev/sdk Pull Request #25
 *  - adri22235/ps4-suid-scanner 1352_offsets.txt
 *
 * These defines are the minimal offsets WebRTE's installer needs. If you
 * obtain a kernel_live.elf from a device running 13.52, verify these
 * against that dump and update as necessary.
 */

#ifndef _OFFS_1352_H
#define _OFFS_1352_H

/* Patch addresses are applied as: kernbase + OFFSET_* */
#define OFFSET_MEMCPY_STACK_CHECK   0x003B35F0
#define OFFSET_VM_MAP_PROTECT_CHECK 0x003B3610
#define OFFSET_KMEM_ALLOC_1         0x001FC561
#define OFFSET_KMEM_ALLOC_2         0x001FC569

#endif /* _OFFS_1352_H */
