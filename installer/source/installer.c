// golden
// 6/12/2018
//

#include "installer.h"

extern uint8_t kernelelf[];
extern int32_t kernelelf_size;

extern uint8_t debuggerbin[];
extern int32_t debuggerbin_size;

void ascii_art() {
    printf("\n\n");
    printf(" __          __  _     _____ _______ ______  \n");
    printf(" \\ \\        / / | |   |  __ \\__   __|  ____| \n");
    printf("  \\ \\  /\\  / /__| |__ | |__) | | |  | |__    \n");
    printf("   \\ \\/  \\/ / _ \\ '_ \\|  _  /  | |  |  __|   \n");
    printf("    \\  /\\  /  __/ |_) | | \\ \\  | |  | |____  \n");
    printf("     \\/  \\/ \\___|_.__/|_|  \\_\\ |_|  |______| \n");
    printf("                                             \n");
}

void patch_505(uint64_t kernbase) {

    // patch memcpy first
    *(uint8_t*)(kernbase + 0x1EA53D) = 0xEB;

    // patch sceSblACMgrIsAllowedSystemLevelDebugging
    memcpy((void*)(kernbase + 0x11730), "\x48\xC7\xC0\x01\x00\x00\x00\xC3", 8);

    // patch sceSblACMgrHasMmapSelfCapability
    memcpy((void*)(kernbase + 0x117B0), "\x48\xC7\xC0\x01\x00\x00\x00\xC3", 8);

    // patch sceSblACMgrIsAllowedToMmapSelf
    memcpy((void*)(kernbase + 0x117C0), "\x48\xC7\xC0\x01\x00\x00\x00\xC3", 8);

    // disable sysdump_perform_dump_on_fatal_trap
    // will continue execution and give more information on crash, such as rip
    *(uint8_t*)(kernbase + 0x7673E0) = 0xC3;

    // self patches
    memcpy((void*)(kernbase + 0x13F03F), "\x31\xC0\x90\x90\x90", 5);

    // patch vm_map_protect check
    memcpy((void*)(kernbase + 0x1A3C08), "\x90\x90\x90\x90\x90\x90", 6);

    // patch ptrace, thanks 2much4u
    *(uint8_t*)(kernbase + 0x30D9AA) = 0xEB;

    // remove all these bullshit checks from ptrace, by golden
    memcpy((void*)(kernbase + 0x30DE01), "\xE9\xD0\x00\x00\x00", 5);

    // patch ASLR, thanks 2much4u
    *(uint16_t*)(kernbase + 0x194875) = 0x9090;

    // patch kmem_alloc
    *(uint8_t*)(kernbase + 0xFCD48) = VM_PROT_ALL;
    *(uint8_t*)(kernbase + 0xFCD56) = VM_PROT_ALL;
}

void patch_672(uint64_t kernbase) {
    // patch memcpy first
    *(uint8_t*)(kernbase + 0x3C15BD) = 0xEB;

    // patch sceSblACMgrIsAllowedSystemLevelDebugging
    memcpy((void*)(kernbase + 0x233BD0), "\x48\xC7\xC0\x01\x00\x00\x00\xC3", 8);

    // patch sceSblACMgrHasMmapSelfCapability
    memcpy((void*)(kernbase + 0x233C40), "\x48\xC7\xC0\x01\x00\x00\x00\xC3", 8);

    // patch sceSblACMgrIsAllowedToMmapSelf
    memcpy((void*)(kernbase + 0x233C50), "\x48\xC7\xC0\x01\x00\x00\x00\xC3", 8);

    // disable sysdump_perform_dump_on_fatal_trap
    // will continue execution and give more information on crash, such as rip
    *(uint8_t*)(kernbase + 0x784120) = 0xC3;

    // self patches
    memcpy((void*)(kernbase + 0xAD2E4), "\x31\xC0\x90\x90\x90", 5);

    // patch vm_map_protect check
    memcpy((void*)(kernbase + 0x451DB8), "\x90\x90\x90\x90\x90\x90", 6);

    // patch ptrace, thanks 2much4u
    *(uint8_t*)(kernbase + 0x10F879) = 0xEB;

    // remove all these bullshit checks from ptrace, by golden
    memcpy((void*)(kernbase + 0x10FD22), "\xE9\xD0\x00\x00\x00", 5);

    // patch ASLR, thanks 2much4u
    *(uint16_t*)(kernbase + 0x3CECE1) = 0x9090;

    // patch kmem_alloc
    *(uint8_t*)(kernbase + 0x2507F5) = VM_PROT_ALL;
    *(uint8_t*)(kernbase + 0x250803) = VM_PROT_ALL;
}

void patch_70X(uint64_t kernbase) {
    // patch memcpy first
    *(uint8_t*)(kernbase + 0x2F04D) = 0xEB;

    // patch sceSblACMgrIsAllowedSystemLevelDebugging
    memcpy((void*)(kernbase + 0x1CB880), "\x48\xC7\xC0\x01\x00\x00\x00\xC3", 8);

    // patch sceSblACMgrHasMmapSelfCapability
    memcpy((void*)(kernbase + 0x1CB8F0), "\x48\xC7\xC0\x01\x00\x00\x00\xC3", 8);

    // patch sceSblACMgrIsAllowedToMmapSelf
    memcpy((void*)(kernbase + 0x1CB910), "\x48\xC7\xC0\x01\x00\x00\x00\xC3", 8);

    // disable sysdump_perform_dump_on_fatal_trap
    // will continue execution and give more information on crash, such as rip
    *(uint8_t*)(kernbase + 0x7889E0) = 0xC3;

    // self patches
    memcpy((void*)(kernbase + 0x1D40BB), "\x31\xC0\x90\x90\x90", 5);

    // patch vm_map_protect check
    memcpy((void*)(kernbase + 0x264C08), "\x90\x90\x90\x90\x90\x90", 6);

    // patch ptrace, thanks 2much4u
    *(uint8_t*)(kernbase + 0x448D5) = 0xEB;

    // remove all these bullshit checks from ptrace, by golden
    memcpy((void*)(kernbase + 0x44DAF), "\xE9\xD0\x00\x00\x00", 5);

    // patch ASLR, thanks 2much4u
    *(uint16_t*)(kernbase + 0xC1F9A) = 0x9090;

    // patch kmem_alloc
    *(uint8_t*)(kernbase + 0x1171BE) = VM_PROT_ALL;
    *(uint8_t*)(kernbase + 0x1171C6) = VM_PROT_ALL;
}

void patch_755(uint64_t kernbase) {
    // patch memcpy first
    *(uint8_t*)(kernbase + 0x28F80D) = 0xEB;

    // patch sceSblACMgrIsAllowedSystemLevelDebugging
    memcpy((void*)(kernbase + 0x364CD0), "\x48\xC7\xC0\x01\x00\x00\x00\xC3", 8);

    // patch sceSblACMgrHasMmapSelfCapability
    memcpy((void*)(kernbase + 0x364D40), "\x48\xC7\xC0\x01\x00\x00\x00\xC3", 8);

    // patch sceSblACMgrIsAllowedToMmapSelf
    memcpy((void*)(kernbase + 0x364D60), "\x48\xC7\xC0\x01\x00\x00\x00\xC3", 8);

    // disable sysdump_perform_dump_on_fatal_trap
    // will continue execution and give more information on crash, such as rip
    *(uint8_t*)(kernbase + 0x77F9A0) = 0xC3;

    // self patches
    memcpy((void*)(kernbase + 0xDCEB1), "\x31\xC0\x90\x90\x90", 5);

    // patch vm_map_protect check
    memcpy((void*)(kernbase + 0x3014C8), "\x90\x90\x90\x90\x90\x90", 6);

    // patch ptrace, thanks 2much4u
    *(uint8_t*)(kernbase + 0x361CF5) = 0xEB;

    // remove all these bullshit checks from ptrace, by golden
    memcpy((void*)(kernbase + 0x3621CF), "\xE9\xD0\x00\x00\x00", 5);

    // patch ASLR, thanks 2much4u
    *(uint16_t*)(kernbase + 0x218AA2) = 0x9090;

    // patch kmem_alloc
    *(uint8_t*)(kernbase + 0x1754AC) = VM_PROT_ALL;
    *(uint8_t*)(kernbase + 0x1754B4) = VM_PROT_ALL;
}

void patch_80X(uint64_t kernbase) {
    // patch memcpy first
    *(uint8_t*)(kernbase + 0x25E1CD) = 0xEB;

    // patch sceSblACMgrIsAllowedSystemLevelDebugging
    memcpy((void*)(kernbase + 0x1D5710), "\x48\xC7\xC0\x01\x00\x00\x00\xC3", 8);

    // patch sceSblACMgrHasMmapSelfCapability
    memcpy((void*)(kernbase + 0x1D5780), "\x48\xC7\xC0\x01\x00\x00\x00\xC3", 8);

    // patch sceSblACMgrIsAllowedToMmapSelf
    memcpy((void*)(kernbase + 0x1D57A0), "\x48\xC7\xC0\x01\x00\x00\x00\xC3", 8);

    // disable sysdump_perform_dump_on_fatal_trap
    // will continue execution and give more information on crash, such as rip
    *(uint8_t*)(kernbase + 0x766DF0) = 0xC3;

    // self patches
    memcpy((void*)(kernbase + 0xFED61), "\x31\xC0\x90\x90\x90", 5);

    // patch vm_map_protect check
    memcpy((void*)(kernbase + 0x3EC68B), "\x90\x90\x90\x90\x90\x90", 6);

    // patch ptrace, thanks 2much4u
    *(uint8_t*)(kernbase + 0x174155) = 0xEB;

    // remove all these bullshit checks from ptrace, by golden
    memcpy((void*)(kernbase + 0x174173), "\xE9\xD0\x00\x00\x00", 5);

    // patch ASLR, thanks 2much4u
    *(uint16_t*)(kernbase + 0x2856F4) = 0x9090;

    // patch kmem_alloc
    *(uint8_t*)(kernbase + 0x1B4BC) = VM_PROT_ALL;
    *(uint8_t*)(kernbase + 0x1B4C4) = VM_PROT_ALL;
}

void patch_8XX(uint64_t kernbase) {
    // patch memcpy first
    *(uint8_t*)(kernbase + 0x3A40FD) = 0xEB;

    // patch sceSblACMgrIsAllowedSystemLevelDebugging
    memcpy((void*)(kernbase + 0x2935E0), "\x48\xC7\xC0\x01\x00\x00\x00\xC3", 8);

    // patch sceSblACMgrHasMmapSelfCapability
    memcpy((void*)(kernbase + 0x293650), "\x48\xC7\xC0\x01\x00\x00\x00\xC3", 8);

    // patch sceSblACMgrIsAllowedToMmapSelf
    memcpy((void*)(kernbase + 0x293670), "\x48\xC7\xC0\x01\x00\x00\x00\xC3", 8);

    // disable sysdump_perform_dump_on_fatal_trap
    // will continue execution and give more information on crash, such as rip
    *(uint8_t*)(kernbase + 0x76CEB0) = 0xC3;

    // self patches
    memcpy((void*)(kernbase + 0x84411), "\x31\xC0\x90\x90\x90", 5);

    // patch vm_map_protect check
    memcpy((void*)(kernbase + 0x14D6DB), "\x90\x90\x90\x90\x90\x90", 6);

    // patch ptrace, thanks 2much4u
    *(uint8_t*)(kernbase + 0x132535) = 0xEB;

    // remove all these bullshit checks from ptrace, by golden
    memcpy((void*)(kernbase + 0x132A0F), "\xE9\xD0\x00\x00\x00", 5);

    // patch ASLR, thanks 2much4u
    *(uint16_t*)(kernbase + 0x215154) = 0x9090;

    // patch kmem_alloc
    *(uint8_t*)(kernbase + 0x219A6C) = VM_PROT_ALL;
    *(uint8_t*)(kernbase + 0x219A74) = VM_PROT_ALL;
}

void patch_900(uint64_t kernbase) {
    // patch memcpy first
    *(uint8_t*)(kernbase + 0x2714BD) = 0xEB;

    // patch sceSblACMgrIsAllowedSystemLevelDebugging
    memcpy((void*)(kernbase + 0x8BC20), "\x48\xC7\xC0\x01\x00\x00\x00\xC3", 8);

    // patch sceSblACMgrHasMmapSelfCapability
    memcpy((void*)(kernbase + 0x8BC90), "\x48\xC7\xC0\x01\x00\x00\x00\xC3", 8);

    // patch sceSblACMgrIsAllowedToMmapSelf
    memcpy((void*)(kernbase + 0x8BCB0), "\x48\xC7\xC0\x01\x00\x00\x00\xC3", 8);

    // disable sysdump_perform_dump_on_fatal_trap
    // will continue execution and give more information on crash, such as rip
    *(uint8_t*)(kernbase + 0x767E30) = 0xC3;

    // self patches
    memcpy((void*)(kernbase + 0x168051), "\x31\xC0\x90\x90\x90", 5);

    // patch vm_map_protect check
    memcpy((void*)(kernbase + 0x80B8B), "\x90\x90\x90\x90\x90\x90", 6);

    // patch ptrace, thanks 2much4u
    *(uint8_t*)(kernbase + 0x41F4E5) = 0xEB;

    // remove all these bullshit checks from ptrace, by golden
    memcpy((void*)(kernbase + 0x41F9D1), "\xE9\xD0\x00\x00\x00", 5);

    // patch ASLR, thanks 2much4u
    *(uint16_t*)(kernbase + 0x5F824) = 0x9090;

    // patch kmem_alloc
    *(uint8_t*)(kernbase + 0x37BF3C) = VM_PROT_ALL;
    *(uint8_t*)(kernbase + 0x37BF44) = VM_PROT_ALL;
}

void patch_90X(uint64_t kernbase) {
    // patch memcpy first
    *(uint8_t*)(kernbase + 0x27113D) = 0xEB;

    // patch sceSblACMgrIsAllowedSystemLevelDebugging
    memcpy((void*)(kernbase + 0x8BC20), "\x48\xC7\xC0\x01\x00\x00\x00\xC3", 8);

    // patch sceSblACMgrHasMmapSelfCapability
    memcpy((void*)(kernbase + 0x8BC90), "\x48\xC7\xC0\x01\x00\x00\x00\xC3", 8);

    // patch sceSblACMgrIsAllowedToMmapSelf
    memcpy((void*)(kernbase + 0x8BCB0), "\x48\xC7\xC0\x01\x00\x00\x00\xC3", 8);

    // disable sysdump_perform_dump_on_fatal_trap
    // will continue execution and give more information on crash, such as rip
    *(uint8_t*)(kernbase + 0x765DF0) = 0xC3;

    // self patches
    memcpy((void*)(kernbase + 0x168001), "\x31\xC0\x90\x90\x90", 5);

    // patch vm_map_protect check
    memcpy((void*)(kernbase + 0x80B8B), "\x90\x90\x90\x90\x90\x90", 6);

    // patch ptrace, thanks 2much4u
    *(uint8_t*)(kernbase + 0x41D455) = 0xEB;

    // remove all these bullshit checks from ptrace, by golden
    memcpy((void*)(kernbase + 0x41D941), "\xE9\xD0\x00\x00\x00", 5);

    // patch ASLR, thanks 2much4u
    *(uint16_t*)(kernbase + 0x5F824) = 0x9090;

    // patch kmem_alloc
    *(uint8_t*)(kernbase + 0x37A13C) = VM_PROT_ALL;
    *(uint8_t*)(kernbase + 0x37A144) = VM_PROT_ALL;
}

void patch_9XX(uint64_t kernbase) {
    // patch memcpy first
    *(uint8_t*)(kernbase + 0x201CCD) = 0xEB;

    // patch sceSblACMgrIsAllowedSystemLevelDebugging
    memcpy((void*)(kernbase + 0x32590), "\x48\xC7\xC0\x01\x00\x00\x00\xC3", 8);

    // patch sceSblACMgrHasMmapSelfCapability
    memcpy((void*)(kernbase + 0x32600), "\x48\xC7\xC0\x01\x00\x00\x00\xC3", 8);

    // patch sceSblACMgrIsAllowedToMmapSelf
    memcpy((void*)(kernbase + 0x32620), "\x48\xC7\xC0\x01\x00\x00\x00\xC3", 8);

    // disable sysdump_perform_dump_on_fatal_trap
    // will continue execution and give more information on crash, such as rip
    *(uint8_t*)(kernbase + 0x7603C0) = 0xC3;

    // self patches
    memcpy((void*)(kernbase + 0x124AA1), "\x31\xC0\x90\x90\x90", 5);

    // patch vm_map_protect check
    memcpy((void*)(kernbase + 0x196D3B), "\x90\x90\x90\x90\x90\x90", 6);

    // patch ptrace, thanks 2much4u
    *(uint8_t*)(kernbase + 0x47A005) = 0xEB;

    // remove all these bullshit checks from ptrace, by golden
    memcpy((void*)(kernbase + 0x47A4F1), "\xE9\xD0\x00\x00\x00", 5);

    // patch ASLR, thanks 2much4u
    *(uint16_t*)(kernbase + 0x29AE74) = 0x9090;

    // patch kmem_alloc
    *(uint8_t*)(kernbase + 0x188A9C) = VM_PROT_ALL;
    *(uint8_t*)(kernbase + 0x188AA4) = VM_PROT_ALL;
}

void patch_100X(uint64_t kernbase) {
    // patch memcpy first
    *(uint8_t*)(kernbase + 0x472D2D) = 0xEB;

    // patch sceSblACMgrIsAllowedSystemLevelDebugging
    memcpy((void*)(kernbase + 0xA5C60), "\x48\xC7\xC0\x01\x00\x00\x00\xC3", 8);

    // patch sceSblACMgrHasMmapSelfCapability
    memcpy((void*)(kernbase + 0xA5CD0), "\x48\xC7\xC0\x01\x00\x00\x00\xC3", 8);

    // patch sceSblACMgrIsAllowedToMmapSelf
    memcpy((void*)(kernbase + 0xA5CF0), "\x48\xC7\xC0\x01\x00\x00\x00\xC3", 8);

    // disable sysdump_perform_dump_on_fatal_trap
    // will continue execution and give more information on crash, such as rip
    *(uint8_t*)(kernbase + 0x765620) = 0xC3;

    // self patches
    memcpy((void*)(kernbase + 0xEF2C1), "\x31\xC0\x90\x90\x90", 5);

    // patch vm_map_protect check
    memcpy((void*)(kernbase + 0x39207B), "\x90\x90\x90\x90\x90\x90", 6);

    // patch ptrace, thanks 2much4u
    *(uint8_t*)(kernbase + 0x44E625) = 0xEB;

    // remove all these bullshit checks from ptrace, by golden
    memcpy((void*)(kernbase + 0x44EB11), "\xE9\xD0\x00\x00\x00", 5);

    // patch ASLR, thanks 2much4u
    *(uint16_t*)(kernbase + 0x3BF3A4) = 0x9090;

    // patch kmem_alloc
    *(uint8_t*)(kernbase + 0x33B10C) = VM_PROT_ALL;
    *(uint8_t*)(kernbase + 0x33B114) = VM_PROT_ALL;
}

void patch_10XX(uint64_t kernbase) {
    // patch memcpy first
    *(uint8_t*)(kernbase + 0xD737D) = 0xEB;

    // patch sceSblACMgrIsAllowedSystemLevelDebugging
    memcpy((void*)(kernbase + 0x1F4470), "\x48\xC7\xC0\x01\x00\x00\x00\xC3", 8);

    // patch sceSblACMgrHasMmapSelfCapability
    memcpy((void*)(kernbase + 0x1F44E0), "\x48\xC7\xC0\x01\x00\x00\x00\xC3", 8);

    // patch sceSblACMgrIsAllowedToMmapSelf
    memcpy((void*)(kernbase + 0x1F4500), "\x48\xC7\xC0\x01\x00\x00\x00\xC3", 8);

    // disable sysdump_perform_dump_on_fatal_trap
    // will continue execution and give more information on crash, such as rip
    *(uint8_t*)(kernbase + 0x7673D0) = 0xC3;

    // self patches
    memcpy((void*)(kernbase + 0x19E151), "\x31\xC0\x90\x90\x90", 5);

    // patch vm_map_protect check
    memcpy((void*)(kernbase + 0x47B2EC), "\x90\x90\x90\x90\x90\x90", 6);

    // patch ptrace, thanks 2much4u
    *(uint8_t*)(kernbase + 0x424E85) = 0xEB;

    // remove all these bullshit checks from ptrace, by golden
    memcpy((void*)(kernbase + 0x425371), "\xE9\xD0\x00\x00\x00", 5);

    // patch ASLR, thanks 2much4u
    *(uint16_t*)(kernbase + 0x345E04) = 0x9090;

    // patch kmem_alloc
    *(uint8_t*)(kernbase + 0x428A2C) = VM_PROT_ALL;
    *(uint8_t*)(kernbase + 0x428A34) = VM_PROT_ALL;
}

void patch_1100(uint64_t kernbase) {
    // patch memcpy first
    *(uint8_t*)(kernbase + 0x2DDDFD) = 0xEB;

    // patch sceSblACMgrIsAllowedSystemLevelDebugging
    memcpy((void*)(kernbase + 0x3D0DE0), "\x48\xC7\xC0\x01\x00\x00\x00\xC3", 8);

    // patch sceSblACMgrHasMmapSelfCapability
    memcpy((void*)(kernbase + 0x3D0E50), "\x48\xC7\xC0\x01\x00\x00\x00\xC3", 8);

    // patch sceSblACMgrIsAllowedToMmapSelf
    memcpy((void*)(kernbase + 0x3D0E70), "\x48\xC7\xC0\x01\x00\x00\x00\xC3", 8);

    // disable sysdump_perform_dump_on_fatal_trap
    // will continue execution and give more information on crash, such as rip
    *(uint8_t*)(kernbase + 0x76D210) = 0xC3;

    // self patches
    memcpy((void*)(kernbase + 0x157F91), "\x31\xC0\x90\x90\x90", 5);

    // patch vm_map_protect check
    memcpy((void*)(kernbase + 0x35C8EC), "\x90\x90\x90\x90\x90\x90", 6);

    // patch ptrace, thanks 2much4u
    *(uint8_t*)(kernbase + 0x384285) = 0xEB;

    // remove all these bullshit checks from ptrace, by golden
    memcpy((void*)(kernbase + 0x384771), "\xE9\xD0\x00\x00\x00", 5);

    // patch ASLR, thanks 2much4u
    *(uint16_t*)(kernbase + 0x3B11A4) = 0x9090;

    // patch kmem_alloc
    *(uint8_t*)(kernbase + 0x245EDC) = VM_PROT_ALL;
    *(uint8_t*)(kernbase + 0x245EE4) = VM_PROT_ALL;
}


void patch_1300(uint64_t kernbase) {
    // GoldHEN is already loaded when this runs, and it installs its own kernel
    // patches and FakeSelf hooks over the same paths. Re-patching the SELF
    // authentication call, the sceSblACMgr checks, ptrace and ASLR on top of
    // that is what takes the console down the moment an app is launched: both
    // crashes so far landed in process creation with the payload idle.
    //
    // Only the patches WebRTE's own machinery depends on are applied here:
    // kmem_alloc has to hand back RWX for the kdebugger to run from, the
    // vm_map_protect check has to allow it, and memcpy's stack guard gets in
    // the way of the kernel-side copies. Everything else was inherited from
    // ps4debug, is not needed by the HTTP API, and overlaps GoldHEN.

    // patch memcpy stack check
    *(uint8_t*)(kernbase + 0x2BD4ED) = 0xEB;

    // patch vm_map_protect check
    memcpy((void*)(kernbase + 0x2FC14C), "\x90\x90\x90\x90\x90\x90", 6);

    // patch kmem_alloc so the kdebugger's allocation comes back executable
    *(uint8_t*)(kernbase + 0x465B0C) = VM_PROT_ALL;
    *(uint8_t*)(kernbase + 0x465B14) = VM_PROT_ALL;

    // Deliberately not applied on 13.00, all of them GoldHEN territory:
    //   0x3B2D30 sceSblACMgrIsAllowedSystemLevelDebugging
    //   0x3B2DA0 sceSblACMgrHasMmapSelfCapability
    //   0x3B2DC0 sceSblACMgrIsAllowedToMmapSelf
    //   0x76BA30 sysdump_perform_dump_on_fatal_trap
    //   0x1FC4A1 self patches
    //   0x3669E5 and 0x366ED1 ptrace
    //   0x477CB4 ASLR
}


void *rwx_alloc(uint64_t size) {
    uint64_t alignedSize = (size + 0x3FFFull) & ~0x3FFFull;
    return (void *)kmem_alloc(*kernel_map, alignedSize);
}

int load_kdebugger() {
    uint64_t mapsize;
    void *kmemory;
    int (*payload_entry)(void *p);

    // calculate mapped size
    if (elf_mapped_size(kernelelf, &mapsize)) {
        printf("[webrte] invalid kdebugger elf!\n");
        return 1;
    }
    
    // allocate memory
    kmemory = rwx_alloc(mapsize);
    if(!kmemory) {
        printf("[webrte] could not allocate memory for kdebugger!\n");
        return 1;
    }

    // load the elf
    if (load_elf(kernelelf, kernelelf_size, kmemory, mapsize, (void **)&payload_entry)) {
        printf("[webrte] could not load kdebugger elf!\n");
        return 1;
    }

    // call entry
    if (payload_entry(NULL)) {
        return 1;
    }

    return 0;
}

int load_debugger() {
    struct proc *p;
    struct vmspace *vm;
    struct vm_map *map;
    int r;

    p = proc_find_by_name("SceShellCore");
    if(!p) {
        printf("[webrte] could not find SceShellCore process!\n");
        return 1;
    }

    vm = p->p_vmspace;
    map = &vm->vm_map;

    // allocate some memory
    vm_map_lock(map);
    r = vm_map_insert(map, NULL, NULL, PAYLOAD_BASE, PAYLOAD_BASE + PAYLOAD_SIZE, VM_PROT_ALL, VM_PROT_ALL, 0);
    vm_map_unlock(map);
    if(r) {
        printf("[webrte] failed to allocate payload memory!\n");
        return r;
    }

    // write the payload
    r = proc_write_mem(p, (void *)PAYLOAD_BASE, debuggerbin_size, debuggerbin, NULL);
    if(r) {
        printf("[webrte] failed to write payload!\n");
        return r;
    }

    // create a thread
    r = proc_create_thread(p, PAYLOAD_BASE);
    if(r) {
        printf("[webrte] failed to create payload thread!\n");
        return r;
    }

    return 0;
}

void patch_kernel() {
    uint64_t kernbase = get_kbase();

    cpu_disable_wp();

    switch (cachedFirmware) {
    case 505:
        patch_505(kernbase);
        break;
    case 672:
        patch_672(kernbase);
        break;
    case 700:
        patch_70X(kernbase);
        break;
    case 701:
        patch_70X(kernbase);
        break;
    case 702:
        patch_70X(kernbase);
        break;
    case 800:
        patch_80X(kernbase);
        break;
    case 801:
        patch_80X(kernbase);
        break;
    case 803:
        patch_80X(kernbase);
        break;
    case 850:
        patch_8XX(kernbase);
        break;
    case 852:
        patch_8XX(kernbase);
        break;
    case 900:
        patch_900(kernbase);
        break;
    case 903:
        patch_90X(kernbase);
        break;
    case 904:
        patch_90X(kernbase);
        break;
    case 950:
        patch_9XX(kernbase);
        break;
    case 951:
        patch_9XX(kernbase);
        break;
    case 960:
        patch_9XX(kernbase);
        break;
    case 1000:
        patch_100X(kernbase);
        break;
    case 1001:
        patch_100X(kernbase);
        break;
    case 1050:
        patch_10XX(kernbase);
        break;
    case 1070:
        patch_10XX(kernbase);
        break;
    case 1071:
        patch_10XX(kernbase);
        break;
    case 1100:
        patch_1100(kernbase);
        break;
    case 1300:
        patch_1300(kernbase);
        break;
    }

    cpu_enable_wp();
}

int runinstaller() {
    init_ksdk();

    // enable uart
    *disable_console_output = 0;

    ascii_art();

    printf("[webrte] installer running\n");

    // patch the kernel
    printf("[webrte] patching kernel...\n");
    patch_kernel();

    printf("[webrte] loading kdebugger...\n");

    if(load_kdebugger()) {
        return 1;
    }

    printf("[webrte] loading webrte...\n");

    if(load_debugger()) {
        return 1;
    }

    printf("[webrte] webrte created by golden\n");
    
    return 0;
}
