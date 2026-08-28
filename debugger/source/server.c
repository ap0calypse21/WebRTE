// golden
// 6/12/2018
//

#include "server.h"
#include "paramdict.h"
#include "b64.h"

//char *(*strtok)(char *str, const char *delimiters);
unsigned long long int (*strtoull)(const char *str, char **endptr, int base);

// Confirmed by disassembling libkernel_sys 13.00 at 0x23290: rdi is the
// sensor index and rsi the output, and it reaches the sensor through
// ioctl(0xC008A502) on /dev/sbi.
int (*sceKernelGetSocSensorTemperature)(int index, int *out);

struct api_operation {
    char name[32];
    int (*handler)(int sock, struct paramdict *);
};

// Four lines per request drown the klog, and the klog is the one place a crash
// explains itself -- a panic scrolls away behind "accepted a new client" while
// anything is polling. Off by default; GET /verbose?on=1 brings it back.
static int http_verbose = 0;

#define vprintf(fmt, ...) do { if(http_verbose) { uprintf(fmt, ##__VA_ARGS__); } } while(0)

const char *status_to_str(int status) {
    switch(status) {
        case 200:
            return "OK.";
        case 404:
            return "Not Found.";
        case 405:
            return "Method Not Allowed.";
    }

    return "(null)";
}

void send_response(int sock, int status, char *body) {
    char header[1024];
    char *resp;
    int size;

    snprintf(header, sizeof(header), "HTTP/1.1 %i %s\r\nAccess-Control-Allow-Origin: *\r\nContent-Type: application/json\r\nContent-Length: %i\r\n", status, status_to_str(status), body ? strlen(body) : 0);
    
    if(body) {
        size = strlen(header) + 2 + strlen(body);
    } else {
        size = strlen(header) + 2;
    }
    
    resp = (char *)pfmalloc(size + 1); // plus 1 for null terminator
    strcpy(resp, header);
    strcat(resp, "\r\n");
    
    if(body) {
        strcat(resp, body);
    }

    sceNetSend(sock, resp, size, 0);

    vprintf("sent response %i content-length %i", status, size);

    free(resp);
}

int handle_list(int sock, struct paramdict *params) {
    int i;

    struct proc_list_entry *plist;
    uint64_t numprocs = 0;

    if(sys_proc_list(NULL, &numprocs)) {
        return 1;
    }

    if(!numprocs) {
        return 1;
    }

    plist = (struct proc_list_entry *)pfmalloc(sizeof(struct proc_list_entry) * numprocs);
    memset(plist, 0, sizeof(struct proc_list_entry) * numprocs);
    if(sys_proc_list(plist, &numprocs)) {
        return 1;
    }

    char scratch[1024];
    int size = 8192;
    int cursize = 0;
    char *json = (char *)pfmalloc(size);
    memset(json, 0, size);
    strcat(json, "[ ");

    for(i = 0; i < numprocs; i++) {
        // build json for entry
        snprintf(scratch, sizeof(scratch), "{ \"name\": \"%s\", \"pid\": %i }%s", plist[i].p_comm, plist[i].pid, (i == (numprocs - 1)) ? "" : ",");

        cursize += strlen(scratch) + 1;
        if(cursize >= size - 1) {
            size += 4096;
            json = realloc(json, size);
        }

        strcat(json, scratch);
        memset(scratch, 0, sizeof(scratch));
    }

    char *end = " ]";
    cursize += strlen(end) + 1;
    if(cursize >= size - 1) {
        size += 4096;
        json = realloc(json, size);
    }
    strcat(json, end);

    send_response(sock, 200, json);
    
    free(plist);
    free(json);
   
    return 0;
}

int handle_info(int sock, struct paramdict *params) {
    char *spid = paramdict_search(params, "pid");
    if(!spid) {
        return 1;
    }

    int pid = strtoull(spid, NULL, 0);
    if(errno) {
        return 1;
    }
    
    struct sys_proc_info_args args;
    memset(&args, 0, sizeof(args));
    if(sys_proc_cmd(pid, SYS_PROC_INFO, &args)) {
        return 1;
    }

    // just allocate a scratch amount
    char *json = (char *)malloc(4096);

    char *version = "";
    

    snprintf(json, 4096, "{ \"name\": \"%s\", \"version\": \"%s\", \"path\": \"%s\", \"titleid\": \"%s\", \"contentid\": \"%s\" }", args.name, version, args.path, args.titleid, args.contentid);   

    send_response(sock, 200, json);

    free(json);

    return 0;
}

int handle_mapping(int sock, struct paramdict *params) {
    int i;

    char *spid = paramdict_search(params, "pid");
    if(!spid) {
        return 1;
    }

    int pid = strtoull(spid, NULL, 0);
    if(errno) {
        return 1;
    }

    struct sys_proc_vm_map_args args;
    memset(&args, 0, sizeof(args));

    if(sys_proc_cmd(pid, SYS_PROC_VM_MAP, &args)) {
        return 1;
    }

    args.maps = (struct proc_vm_map_entry *)pfmalloc(sizeof(struct proc_vm_map_entry) * args.num);
    memset(args.maps, 0, sizeof(struct proc_vm_map_entry) * args.num);
    if(sys_proc_cmd(pid, SYS_PROC_VM_MAP, &args)) {
        free(args.maps);
        return 1;
    }

    char scratch[1024];
    int size = 8192;
    int cursize = 0;
    char *json = (char *)pfmalloc(size);
    memset(json, 0, size);
    strcat(json, "[ ");

    for(i = 0; i < args.num; i++) {
        // build json for entry
        struct proc_vm_map_entry *p = &args.maps[i];
        snprintf(scratch, sizeof(scratch), "{ \"name\": \"%s\", \"start\": %lli, \"end\": %lli, \"offset\": %lli, \"prot\": %i }%s", p->name, p->start, p->end, p->offset, p->prot, (i == (args.num - 1)) ? "" : ",");

        cursize += strlen(scratch) + 1;
        if(cursize >= size - 1) {
            size += 4096;
            json = realloc(json, size);
        }

        strcat(json, scratch);
        memset(scratch, 0, sizeof(scratch));
    }

    char *end = " ]";
    cursize += strlen(end) + 1;
    if(cursize >= size - 1) {
        size += 4096;
        json = realloc(json, size);
    }
    strcat(json, end);

    send_response(sock, 200, json);

    free(args.maps);
    free(json);

    return 0;
}

int handle_write(int sock, struct paramdict *params) {
    char *spid = paramdict_search(params, "pid");
    if(!spid) {
        return 1;
    }

    int pid = strtoull(spid, NULL, 0);
    if(errno) {
        return 1;
    }

    char *saddress = paramdict_search(params, "address");
    if(!saddress) {
        return 1;
    }

    uint64_t address = strtoull(saddress, NULL, 0);
    if(errno) {
        return 1;
    }
    
    char *slength = paramdict_search(params, "length");
    if(!slength) {
        return 1;
    }

    uint64_t length = strtoull(slength, NULL, 0);
    if(errno) {
        return 1;
    }

    char *data = paramdict_search(params, "data");
    if(!data) {
        return 1;
    }

    unsigned char *rawdata = b64_decode(data, strlen(data));

    if(sys_proc_rw(pid, address, rawdata, length, 1)) {
        free(rawdata);
        return 1;
    }

    send_response(sock, 200, NULL);

    free(rawdata);

    return 0;
}

int handle_read(int sock, struct paramdict *params) {
    char *spid = paramdict_search(params, "pid");
    if(!spid) {
        return 1;
    }

    int pid = strtoull(spid, NULL, 0);
    if(errno) {
        return 1;
    }

    char *saddress = paramdict_search(params, "address");
    if(!saddress) {
        return 1;
    }

    uint64_t address = strtoull(saddress, NULL, 0);
    if(errno) {
        return 1;
    }
    
    char *slength = paramdict_search(params, "length");
    if(!slength) {
        return 1;
    }

    uint64_t length = strtoull(slength, NULL, 0);
    if(errno) {
        return 1;
    }

    unsigned char *data = (unsigned char *)pfmalloc(length);
    
    if(sys_proc_rw(pid, address, data, length, 0)) {
        free(data);
        return 1;
    }

    char *b64data = b64_encode(data, length);

    send_response(sock, 200, b64data);

    free(data);
    free(b64data);

    return 0;
}

int handle_alloc(int sock, struct paramdict *params) {
    char *spid = paramdict_search(params, "pid");
    if(!spid) {
        return 1;
    }

    int pid = strtoull(spid, NULL, 0);
    if(errno) {
        return 1;
    }
    
    char *slength = paramdict_search(params, "length");
    if(!slength) {
        return 1;
    }

    uint64_t length = strtoull(slength, NULL, 0);
    if(errno) {
        return 1;
    }

    struct sys_proc_alloc_args args;
    args.address = 0;
    args.length = length;

    if(sys_proc_cmd(pid, SYS_PROC_ALLOC, &args)) {
        return 1;
    }

    char scratch[512];
    snprintf(scratch, sizeof(scratch), "{ \"address\": %i }", args.address);
    send_response(sock, 200, scratch);

    return 0;
}

int handle_free(int sock, struct paramdict *params) {
    char *spid = paramdict_search(params, "pid");
    if(!spid) {
        return 1;
    }

    int pid = strtoull(spid, NULL, 0);
    if(errno) {
        return 1;
    }

    char *saddress = paramdict_search(params, "address");
    if(!saddress) {
        return 1;
    }

    uint64_t address = strtoull(saddress, NULL, 0);
    if(errno) {
        return 1;
    }
    
    char *slength = paramdict_search(params, "length");
    if(!slength) {
        return 1;
    }

    uint64_t length = strtoull(slength, NULL, 0);
    if(errno) {
        return 1;
    }

    struct sys_proc_free_args args;
    args.address = address;
    args.length = length;

    if(sys_proc_cmd(pid, SYS_PROC_FREE, &args)) {
        return 1;
    }

    send_response(sock, 200, NULL);

    return 0;
}

int handle_pause(int sock, struct paramdict *params) {
    char *spid = paramdict_search(params, "pid");
    if(!spid) {
        return 1;
    }

    int pid = strtoull(spid, NULL, 0);
    if(errno) {
        return 1;
    }

    kill(pid, 17); // SIGSTOP 17

    send_response(sock, 200, NULL);

    return 0;
}

int handle_resume(int sock, struct paramdict *params) {
    char *spid = paramdict_search(params, "pid");
    if(!spid) {
        return 1;
    }

    int pid = strtoull(spid, NULL, 0);
    if(errno) {
        return 1;
    }

    kill(pid, 19); // SIGCONT 19

    send_response(sock, 200, NULL);

    return 0;
}


// -------------------------------------------------------------------------
// Telemetry. Everything below reads only; nothing here writes to the console.
//
// sysctlbyname and sceKernelGetCpuTemperature are already resolved by libPS4's
// initKernel(), which _main() calls before the server starts. Most of what a
// devkit shows is a sysctl underneath anyway -- sceKernelGetCpuFrequency is a
// wrapper over "dev.cpu.0.freq" and sceKernelGetSocPowerConsumption over
// "machdep.liverpool.telemetry" -- so one generic reader covers the bulk of it
// without resolving a single extra symbol.
// -------------------------------------------------------------------------

#define SYSCTL_MAX 65536

// Two-phase read: ask for the size first, then fetch. Returns 0 on success and
// leaves the byte count in *outlen.
static int sysctl_read(const char *name, unsigned char **out, size_t *outlen) {
    size_t len = 0;

    if(sysctlbyname((char *)name, NULL, &len, NULL, 0)) {
        return 1;
    }

    if(!len || len > SYSCTL_MAX) {
        return 1;
    }

    unsigned char *buf = (unsigned char *)pfmalloc(len);
    if(!buf) {
        return 1;
    }

    if(sysctlbyname((char *)name, (char *)buf, &len, NULL, 0)) {
        free(buf);
        return 1;
    }

    *out = buf;
    *outlen = len;
    return 0;
}

// Appends `"key": <value>` for a sysctl, or `"key": null` when it is missing.
// 4- and 8-byte nodes are emitted as numbers, anything else as a hex string.
static void json_sysctl(char *json, int size, const char *key, const char *name) {
    char scratch[512];
    unsigned char *buf = NULL;
    size_t len = 0;

    if(sysctl_read(name, &buf, &len)) {
        snprintf(scratch, sizeof(scratch), "\"%s\": null, ", key);
        strcat(json, scratch);
        return;
    }

    if(len == 4) {
        snprintf(scratch, sizeof(scratch), "\"%s\": %u, ", key, *(uint32_t *)buf);
    } else if(len == 8) {
        snprintf(scratch, sizeof(scratch), "\"%s\": %llu, ", key, *(uint64_t *)buf);
    } else {
        // A NUL-terminated printable run is far more useful as text than as
        // hex -- hw.model, kern.version and dev.cpu.0.freq_levels are strings.
        int printable = (len > 1 && buf[len - 1] == 0);
        for(size_t i = 0; printable && i < len - 1; i++) {
            if((buf[i] < 0x20 || buf[i] > 0x7e) && buf[i] != 0) {
                printable = 0;
            }
        }
        if(printable) {
            snprintf(scratch, sizeof(scratch), "\"%s\": \"%s\", ", key, (char *)buf);
            strcat(json, scratch);
        } else {
            int n = (int)(len > 96 ? 96 : len);
            snprintf(scratch, sizeof(scratch), "\"%s\": \"", key);
            strcat(json, scratch);
            for(int i = 0; i < n; i++) {
                snprintf(scratch, sizeof(scratch), "%02x", buf[i]);
                strcat(json, scratch);
            }
            strcat(json, "\", ");
        }
        free(buf);
        return;
    }

    strcat(json, scratch);
    free(buf);
}

int handle_sysctl(int sock, struct paramdict *params) {
    char *name = paramdict_search(params, "name");
    if(!name) {
        return 1;
    }

    unsigned char *buf = NULL;
    size_t len = 0;
    if(sysctl_read(name, &buf, &len)) {
        return 1;
    }

    int size = (int)(len * 2 + 1024);
    char *json = (char *)pfmalloc(size);
    memset(json, 0, size);

    char scratch[512];
    snprintf(scratch, sizeof(scratch), "{ \"name\": \"%s\", \"size\": %i, ", name, (int)len);
    strcat(json, scratch);

    if(len == 4) {
        snprintf(scratch, sizeof(scratch), "\"int\": %u, ", *(uint32_t *)buf);
        strcat(json, scratch);
    } else if(len == 8) {
        snprintf(scratch, sizeof(scratch), "\"int\": %llu, ", *(uint64_t *)buf);
        strcat(json, scratch);
    }

    // A NUL-terminated run of printable bytes is worth showing as text too.
    int printable = (len > 1 && buf[len - 1] == 0);
    for(size_t i = 0; printable && i < len - 1; i++) {
        if(buf[i] < 0x20 || buf[i] > 0x7e) {
            printable = 0;
        }
    }
    if(printable) {
        snprintf(scratch, sizeof(scratch), "\"str\": \"%s\", ", (char *)buf);
        strcat(json, scratch);
    }

    strcat(json, "\"hex\": \"");
    for(size_t i = 0; i < len; i++) {
        snprintf(scratch, sizeof(scratch), "%02x", buf[i]);
        strcat(json, scratch);
    }
    strcat(json, "\" }");

    send_response(sock, 200, json);

    free(json);
    free(buf);

    return 0;
}

int handle_sensors(int sock, struct paramdict *params) {
    int size = 8192;
    char *json = (char *)pfmalloc(size);
    memset(json, 0, size);
    char scratch[512];

    strcat(json, "{ ");

    // /dev/sbi rather than a sysctl, and already resolved by libPS4.
    uint32_t cpu_temp = 0;
    if(sceKernelGetCpuTemperature(&cpu_temp) == 0) {
        snprintf(scratch, sizeof(scratch), "\"cpu_temp_c\": %u, ", cpu_temp);
    } else {
        snprintf(scratch, sizeof(scratch), "\"cpu_temp_c\": null, ");
    }
    strcat(json, scratch);

    // Second SBI sensor. Index 0 is the SoC die; higher indices return an
    // error on this hardware and are simply reported as null.
    int soc_temp = 0;
    if(sceKernelGetSocSensorTemperature && sceKernelGetSocSensorTemperature(0, &soc_temp) == 0) {
        snprintf(scratch, sizeof(scratch), "\"soc_temp_c\": %i, ", soc_temp);
    } else {
        snprintf(scratch, sizeof(scratch), "\"soc_temp_c\": null, ");
    }
    strcat(json, scratch);

    // Cached at load time by libkernel; takes no arguments.
    snprintf(scratch, sizeof(scratch), "\"direct_mem_total\": %llu, ",
             (unsigned long long)sceKernelGetDirectMemorySize());
    strcat(json, scratch);

    // Only nodes that were confirmed present on a 13.00 console. Sony strips
    // most of the stock FreeBSD tree: hw.physmem, vm.stats.*, kern.cp_time and
    // the whole hw.acpi.thermal branch all return ENOENT here.
    json_sysctl(json, size, "cpu_freq_mhz",   "dev.cpu.0.freq");
    json_sysctl(json, size, "freq_levels",    "dev.cpu.0.freq_levels");
    json_sysctl(json, size, "ncpu",           "hw.ncpu");
    json_sysctl(json, size, "pagesize",       "hw.pagesize");
    json_sysctl(json, size, "availpages",     "hw.availpages");
    json_sysctl(json, size, "model",          "hw.model");
    json_sysctl(json, size, "mlock_avail",    "vm.budgets.mlock_avail");
    json_sysctl(json, size, "mlock_total",    "vm.budgets.mlock_total");
    json_sysctl(json, size, "swap_total",     "vm.swap_total");
    json_sysctl(json, size, "swap_reserved",  "vm.swap_reserved");
    json_sysctl(json, size, "tsc_freq",       "machdep.tsc_freq");
    json_sysctl(json, size, "icc_max",        "machdep.liverpool.icc_max");
    json_sysctl(json, size, "telemetry",      "machdep.liverpool.telemetry");
    json_sysctl(json, size, "osreldate",      "kern.osreldate");
    json_sysctl(json, size, "osrelease",      "kern.osrelease");
    json_sysctl(json, size, "version",        "kern.version");

    // drop the trailing ", "
    int l = (int)strlen(json);
    if(l >= 2 && json[l - 2] == ',') {
        json[l - 2] = 0;
    }
    strcat(json, " }");

    send_response(sock, 200, json);
    free(json);

    return 0;
}


// -------------------------------------------------------------------------
// Process control and kernel inspection.
//
// The kdebugger already exports more than the HTTP layer used: signals go
// through the same kill() that pause/resume use, and sys_kern_base /
// sys_kern_rw / SYS_PROC_THRINFO were implemented but never exposed.
// -------------------------------------------------------------------------

int handle_signal(int sock, struct paramdict *params) {
    char *spid = paramdict_search(params, "pid");
    char *ssig = paramdict_search(params, "sig");
    if(!spid || !ssig) {
        return 1;
    }

    int pid = strtoull(spid, NULL, 0);
    int sig = strtoull(ssig, NULL, 0);

    // Refuse anything outside the normal signal range so a stray request
    // cannot turn into an arbitrary syscall argument.
    if(sig < 1 || sig > 31 || pid < 1) {
        return 1;
    }

    kill(pid, sig);

    char scratch[128];
    snprintf(scratch, sizeof(scratch), "{ \"pid\": %i, \"sig\": %i }", pid, sig);
    send_response(sock, 200, scratch);

    return 0;
}

int handle_thrinfo(int sock, struct paramdict *params) {
    char *spid = paramdict_search(params, "pid");
    if(!spid) {
        return 1;
    }

    int pid = strtoull(spid, NULL, 0);

    struct sys_proc_thrinfo_args args;
    memset(&args, 0, sizeof(args));

    if(sys_proc_cmd(pid, SYS_PROC_THRINFO, &args)) {
        return 1;
    }

    char scratch[512];
    snprintf(scratch, sizeof(scratch),
             "{ \"pid\": %i, \"lwpid\": %u, \"priority\": %u, \"name\": \"%s\" }",
             pid, args.lwpid, args.priority, args.name);
    send_response(sock, 200, scratch);

    return 0;
}

int handle_kernbase(int sock, struct paramdict *params) {
    uint64_t kbase = 0;

    if(sys_kern_base(&kbase)) {
        return 1;
    }

    char scratch[128];
    snprintf(scratch, sizeof(scratch), "{ \"kernbase\": %llu }", (unsigned long long)kbase);
    send_response(sock, 200, scratch);

    return 0;
}

int handle_kread(int sock, struct paramdict *params) {
    char *saddress = paramdict_search(params, "address");
    char *slength = paramdict_search(params, "length");
    if(!saddress || !slength) {
        return 1;
    }

    uint64_t address = strtoull(saddress, NULL, 0);
    uint64_t length = strtoull(slength, NULL, 0);

    // A bad kernel read faults the whole console, so keep the window small.
    if(!length || length > 4096) {
        return 1;
    }

    unsigned char *data = (unsigned char *)pfmalloc(length);
    if(!data) {
        return 1;
    }

    if(sys_kern_rw(address, data, length, 0)) {
        free(data);
        return 1;
    }

    char *b64data = b64_encode(data, length);
    send_response(sock, 200, b64data);

    free(data);
    free(b64data);

    return 0;
}

// -------------------------------------------------------------------------
// Fan. /dev/icc_fan is a real character device -- its cdevsw sits at
// kernbase+0xC91348 on 13.00 with d_name "icc_fan", and this ioctl handler at
// kernbase+0x3F6B70. Every command is forwarded to ICC service 0x0A with the
// opcode in the message header, and one of them carries the kernel's own
// string "icc_fan_get_fan_manual_duty", which is what names the pair below.
//
// Group 0x8F, num = opcode + 1, all _IOWR:
//
//   op  ioctl        len  in                    out
//   0   0xC0168F01   22   u8 idx @0             u8 @4, 16 bytes @6  (status @2)
//   1   0xC0148F02   20   -                     u16 @2
//   2   0xC0048F03    4   u8 @2, u8 @3          -
//   3   0xC0048F04    4   u8 @2                 u8 @3
//   4   0xC0068F05    6   u8 @2, u16 @4         -         set manual duty
//   5   0xC0068F06    6   u8 @2                 u16 @4    get manual duty
//   6   0xC01C8F07   28   u8 @2, u8 @3, 6x u32  -         set table
//   7   0xC01C8F08   28   u8 @2                 6x u32 @4 get table
//   8   0xC0148F09   20   u8 @2                 3x u32 @4, u16 @16
//
// Each buffer opens with a u16 status the driver fills in from the SYSCON
// reply -- 0 means accepted. Op 0 is the exception: its status is at byte 2.
//
// GET /fan            reads only: ops 5, 3, 1, 7, 8 and 0.
// GET /fan?op=N&...   issues exactly one command, the setters included.
//                     Nothing here writes unless it was asked for by number.
// -------------------------------------------------------------------------

#define FAN_DEV "/dev/icc_fan"
#define FAN_IOC(num, len) \
    ((unsigned long)(0xC0000000UL | ((unsigned long)((len) & 0x1FFF) << 16) \
                     | (0x8FUL << 8) | (unsigned long)(num)))

static const unsigned long fan_ioc[9] = {
    FAN_IOC(1, 22), FAN_IOC(2, 20), FAN_IOC(3, 4),
    FAN_IOC(4, 4),  FAN_IOC(5, 6),  FAN_IOC(6, 6),
    FAN_IOC(7, 28), FAN_IOC(8, 28), FAN_IOC(9, 20)
};

static const int fan_len[9] = { 22, 20, 4, 4, 6, 6, 28, 28, 20 };

// Returns 0 only when the ioctl succeeded and the SYSCON accepted it too.
// Op 0 keeps its status word at byte 2, every other command at byte 0.
static int fan_op(int fd, int op, unsigned char *buf) {
    if(ioctl(fd, fan_ioc[op], buf)) {
        return 1;
    }
    return *(unsigned short *)(buf + (op == 0 ? 2 : 0)) != 0;
}

int handle_fan(int sock, struct paramdict *params) {
    char json[4096];
    char scratch[256];
    unsigned char buf[32];
    int i;

    int fd = open(FAN_DEV, O_RDWR, 0);
    if(fd < 0) {
        // Worth reporting rather than 404ing: a permission failure and a
        // missing device look identical from the dashboard otherwise.
        snprintf(json, sizeof(json),
                 "{ \"error\": \"open %s failed\", \"fd\": %i }", FAN_DEV, fd);
        send_response(sock, 200, json);
        return 0;
    }

    char *sidx = paramdict_search(params, "idx");
    int idx = sidx ? (int)strtoull(sidx, NULL, 0) : 0;

    char *sop = paramdict_search(params, "op");
    if(sop) {
        int op = (int)strtoull(sop, NULL, 0);
        if(op < 0 || op > 8) {
            close(fd);
            return 1;
        }

        memset(buf, 0, sizeof(buf));

        // Op 0 takes its index at byte 0, every other command at byte 2.
        if(op == 0) {
            buf[0] = (unsigned char)idx;
        } else {
            buf[2] = (unsigned char)idx;
        }

        char *sb = paramdict_search(params, "b");
        if(sb) {
            buf[3] = (unsigned char)strtoull(sb, NULL, 0);
        }

        char *sduty = paramdict_search(params, "duty");
        if(sduty) {
            *(unsigned short *)(buf + 4) = (unsigned short)strtoull(sduty, NULL, 0);
        }

        for(i = 0; i < 6; i++) {
            char key[3];
            key[0] = 'd';
            key[1] = (char)('0' + i);
            key[2] = 0;
            char *sd = paramdict_search(params, key);
            if(sd) {
                *(unsigned int *)(buf + 4 + i * 4) = (unsigned int)strtoull(sd, NULL, 0);
            }
        }

        int r = ioctl(fd, fan_ioc[op], buf);
        close(fd);

        snprintf(json, sizeof(json),
                 "{ \"op\": %i, \"ioctl\": \"0x%08X\", \"ret\": %i, \"status\": %u, \"hex\": \"",
                 op, (unsigned int)fan_ioc[op], r,
                 *(unsigned short *)(buf + (op == 0 ? 2 : 0)));
        for(i = 0; i < fan_len[op]; i++) {
            snprintf(scratch, sizeof(scratch), "%02x", buf[i]);
            strcat(json, scratch);
        }
        strcat(json, "\" }");
        send_response(sock, 200, json);
        return 0;
    }

    memset(json, 0, sizeof(json));
    strcat(json, "{ ");

    memset(buf, 0, sizeof(buf));
    buf[2] = (unsigned char)idx;
    if(!fan_op(fd, 5, buf)) {
        snprintf(scratch, sizeof(scratch), "\"duty\": %u, ", *(unsigned short *)(buf + 4));
    } else {
        snprintf(scratch, sizeof(scratch), "\"duty\": null, ");
    }
    strcat(json, scratch);

    memset(buf, 0, sizeof(buf));
    buf[2] = (unsigned char)idx;
    if(!fan_op(fd, 3, buf)) {
        snprintf(scratch, sizeof(scratch), "\"mode\": %u, ", buf[3]);
    } else {
        snprintf(scratch, sizeof(scratch), "\"mode\": null, ");
    }
    strcat(json, scratch);

    memset(buf, 0, sizeof(buf));
    if(!fan_op(fd, 1, buf)) {
        snprintf(scratch, sizeof(scratch), "\"op1\": %u, ", *(unsigned short *)(buf + 2));
    } else {
        snprintf(scratch, sizeof(scratch), "\"op1\": null, ");
    }
    strcat(json, scratch);

    // Six dwords. That is the shape a thermal table would have, but nothing
    // here proves it -- they are reported raw until the values say otherwise.
    memset(buf, 0, sizeof(buf));
    buf[2] = (unsigned char)idx;
    if(!fan_op(fd, 7, buf)) {
        strcat(json, "\"table\": [");
        for(i = 0; i < 6; i++) {
            snprintf(scratch, sizeof(scratch), "%s%u",
                     i ? ", " : "", *(unsigned int *)(buf + 4 + i * 4));
            strcat(json, scratch);
        }
        strcat(json, "], ");
    } else {
        strcat(json, "\"table\": null, ");
    }

    memset(buf, 0, sizeof(buf));
    buf[2] = (unsigned char)idx;
    if(!fan_op(fd, 8, buf)) {
        snprintf(scratch, sizeof(scratch), "\"op8\": [%u, %u, %u, %u], ",
                 *(unsigned int *)(buf + 4), *(unsigned int *)(buf + 8),
                 *(unsigned int *)(buf + 12), *(unsigned short *)(buf + 16));
    } else {
        snprintf(scratch, sizeof(scratch), "\"op8\": null, ");
    }
    strcat(json, scratch);

    memset(buf, 0, sizeof(buf));
    buf[0] = (unsigned char)idx;
    if(!fan_op(fd, 0, buf)) {
        snprintf(scratch, sizeof(scratch), "\"op0_byte\": %u, \"op0\": \"", buf[4]);
        strcat(json, scratch);
        for(i = 0; i < 16; i++) {
            snprintf(scratch, sizeof(scratch), "%02x", buf[6 + i]);
            strcat(json, scratch);
        }
        strcat(json, "\", ");
    } else {
        strcat(json, "\"op0_byte\": null, \"op0\": null, ");
    }

    snprintf(scratch, sizeof(scratch), "\"idx\": %i }", idx);
    strcat(json, scratch);

    close(fd);
    send_response(sock, 200, json);

    return 0;
}

// -------------------------------------------------------------------------
// Storage. Sony strips kern.disks, hw.physmem and the whole vm.stats tree, so
// there is no sysctl route to disk usage on 13.00 -- every one of those came
// back absent when probed against the live console. getfsstat does survive
// though: reading the sysent table live shows 395 with sy_narg 3 and a real
// sy_call, alongside statfs (396) and fstatfs (397).
//
// One call returns every mount at once, so nothing has to guess device names.
// Called with a NULL buffer it just reports how many mounts there are.
// -------------------------------------------------------------------------

#define SYS_GETFSSTAT   395
#define MNT_NOWAIT      2       // never block waiting on a filesystem
#define STATFS_SIZE     0x1D8   // FreeBSD 9 struct statfs, MNAMELEN 88

// Byte offsets into struct statfs. Derived from the FreeBSD 9 layout and
// checked against the mount names the console actually returns -- if the
// strings did not land here the response would be visibly garbled.
#define SF_BSIZE        0x10
#define SF_BLOCKS       0x20
#define SF_BFREE        0x28
#define SF_BAVAIL       0x30
#define SF_FILES        0x38
#define SF_FFREE        0x40
#define SF_FLAGS        0x08
#define SF_FSTYPENAME   0x118
#define SF_MNTFROMNAME  0x128
#define SF_MNTONNAME    0x180

// Copies a fixed-width field out as a NUL-terminated string with the quotes
// and backslashes JSON cannot carry escaped.
static void sf_str(char *dst, int dstsize, const unsigned char *base, int off, int max) {
    int i, n = 0;
    for(i = 0; i < max && n < dstsize - 2; i++) {
        char c = (char)base[off + i];
        if(!c) {
            break;
        }
        if(c == '"' || c == '\\') {
            dst[n++] = '\\';
        }
        dst[n++] = (c < 0x20) ? ' ' : c;
    }
    dst[n] = 0;
}

int handle_storage(int sock, struct paramdict *params) {
    int count = (int)syscall(SYS_GETFSSTAT, (void *)0, (long)0, MNT_NOWAIT);
    if(count <= 0) {
        char err[160];
        snprintf(err, sizeof(err),
                 "{ \"error\": \"getfsstat returned %i\", \"mounts\": [] }", count);
        send_response(sock, 200, err);
        return 0;
    }

    if(count > 128) {
        count = 128;
    }

    long bufsize = (long)count * STATFS_SIZE;
    unsigned char *buf = (unsigned char *)pfmalloc(bufsize);
    if(!buf) {
        return 1;
    }
    memset(buf, 0, bufsize);

    int got = (int)syscall(SYS_GETFSSTAT, buf, bufsize, MNT_NOWAIT);
    if(got <= 0) {
        free(buf);
        return 1;
    }
    if(got > count) {
        got = count;
    }

    int size = got * 512 + 1024;
    char *json = (char *)pfmalloc(size);
    if(!json) {
        free(buf);
        return 1;
    }
    memset(json, 0, size);
    strcat(json, "[ ");

    char scratch[640];
    char on[96], from[96], type[24];
    int i;

    for(i = 0; i < got; i++) {
        const unsigned char *e = buf + (size_t)i * STATFS_SIZE;

        sf_str(on,   sizeof(on),   e, SF_MNTONNAME,   88);
        sf_str(from, sizeof(from), e, SF_MNTFROMNAME, 88);
        sf_str(type, sizeof(type), e, SF_FSTYPENAME,  16);

        unsigned long long bsize  = *(unsigned long long *)(e + SF_BSIZE);
        unsigned long long blocks = *(unsigned long long *)(e + SF_BLOCKS);
        unsigned long long bfree  = *(unsigned long long *)(e + SF_BFREE);
        long long          bavail = *(long long *)(e + SF_BAVAIL);

        // Bytes rather than blocks: the block size differs per mount, and
        // comparing raw block counts across them would be meaningless.
        snprintf(scratch, sizeof(scratch),
                 "{ \"on\": \"%s\", \"from\": \"%s\", \"type\": \"%s\", "
                 "\"bsize\": %llu, \"total\": %llu, \"free\": %llu, \"avail\": %lld, "
                 "\"files\": %llu, \"ffree\": %lld, \"flags\": %llu }%s",
                 on, from, type, bsize,
                 blocks * bsize, bfree * bsize, (long long)(bavail * (long long)bsize),
                 *(unsigned long long *)(e + SF_FILES),
                 *(long long *)(e + SF_FFREE),
                 *(unsigned long long *)(e + SF_FLAGS),
                 (i == got - 1) ? "" : ",");
        strcat(json, scratch);
    }

    strcat(json, " ]");
    send_response(sock, 200, json);

    free(json);
    free(buf);

    return 0;
}

// -------------------------------------------------------------------------
// Files and bulk reads.
//
// send_response cannot carry any of this: it measures the body with strlen, so
// the first zero byte would truncate a binary transfer. Everything below sends
// its own header and then streams, which also keeps the payload from having to
// allocate a whole file at once.
// -------------------------------------------------------------------------

#define XFER_CHUNK   0x10000     // 64 KB per send, so nothing large is allocated
#define LS_JSON_MAX  0x40000     // 256 KB of listing, about 3000 entries
#define LS_ENTRIES   4096
#define DIRENT_BUF   0x8000

// sceNetSend is free to accept less than it was handed. Ignoring the return,
// the way send_response does, silently drops bytes once a body gets large.
static int send_all(int sock, const void *buf, int len) {
    const char *p = (const char *)buf;
    int sent = 0;

    while(sent < len) {
        int n = sceNetSend(sock, p + sent, len - sent, 0);
        if(n <= 0) {
            return 1;
        }
        sent += n;
    }

    return 0;
}

static int send_raw_header(int sock, int len, const char *ctype) {
    char header[512];
    int n = snprintf(header, sizeof(header),
                     "HTTP/1.1 200 OK.\r\nAccess-Control-Allow-Origin: *\r\n"
                     "Content-Type: %s\r\nContent-Length: %i\r\n"
                     "Accept-Ranges: bytes\r\n\r\n", ctype, len);
    return send_all(sock, header, n);
}

static void send_err(int sock, const char *what, int code) {
    char json[256];
    snprintf(json, sizeof(json), "{ \"error\": \"%s\", \"ret\": %i }", what, code);
    send_response(sock, 200, json);
}

// Escapes the characters JSON cannot carry raw. Filenames on this console are
// ordinary, but a listing endpoint should not be the thing that trusts that.
static void json_escape(char *dst, int dstsize, const char *src, int max) {
    int i, n = 0;

    for(i = 0; i < max && src[i] && n < dstsize - 7; i++) {
        unsigned char c = (unsigned char)src[i];
        if(c == '"' || c == '\\') {
            dst[n++] = '\\';
            dst[n++] = c;
        } else if(c < 0x20) {
            n += snprintf(dst + n, dstsize - n, "\\u%04x", c);
        } else {
            dst[n++] = c;
        }
    }

    dst[n] = 0;
}

int handle_ls(int sock, struct paramdict *params) {
    char *path = paramdict_search(params, "path");
    if(!path) {
        return 1;
    }

    int fd = open(path, O_RDONLY, 0);
    if(fd < 0) {
        send_err(sock, "cannot open path", fd);
        return 0;
    }

    char *dbuf = (char *)pfmalloc(DIRENT_BUF);
    char *json = (char *)pfmalloc(LS_JSON_MAX);
    if(!dbuf || !json) {
        close(fd);
        return 1;
    }
    memset(json, 0, LS_JSON_MAX);

    int len = 0;
    len += snprintf(json + len, LS_JSON_MAX - len, "{ \"path\": \"");
    json_escape(json + len, LS_JSON_MAX - len, path, 1024);
    len = strlen(json);
    len += snprintf(json + len, LS_JSON_MAX - len, "\", \"entries\": [ ");

    char name[512];
    char full[1024];
    struct stat sb;
    int count = 0, truncated = 0, n;
    int first = 1;

    while((n = getdents(fd, dbuf, DIRENT_BUF)) > 0) {
        int off = 0;

        while(off < n) {
            struct dirent *de = (struct dirent *)(dbuf + off);
            if(!de->d_reclen) {
                break;
            }
            off += de->d_reclen;

            if(!strcmp(de->d_name, ".") || !strcmp(de->d_name, "..")) {
                continue;
            }
            if(count >= LS_ENTRIES || len > LS_JSON_MAX - 1024) {
                truncated = 1;
                break;
            }

            // getdents gives the type but not the size, so each entry costs one
            // stat. A failed stat is not fatal -- the name is still worth having.
            snprintf(full, sizeof(full), "%s%s%s", path,
                     (path[0] && path[strlen(path) - 1] == '/') ? "" : "/", de->d_name);
            memset(&sb, 0, sizeof(sb));
            int statok = (stat(full, &sb) == 0);

            json_escape(name, sizeof(name), de->d_name, de->d_namlen);
            len += snprintf(json + len, LS_JSON_MAX - len,
                            "%s{ \"name\": \"%s\", \"dir\": %i, \"type\": %i, "
                            "\"size\": %llu, \"mode\": %u, \"mtime\": %lli }",
                            first ? "" : ", ", name,
                            (de->d_type == 4) ? 1 : 0, de->d_type,
                            statok ? (unsigned long long)sb.st_size : 0ULL,
                            statok ? (unsigned int)sb.st_mode : 0u,
                            statok ? (long long)sb.st_mtim.tv_sec : 0LL);
            first = 0;
            count++;
        }

        if(truncated) {
            break;
        }
    }

    close(fd);

    len += snprintf(json + len, LS_JSON_MAX - len,
                    " ], \"count\": %i, \"truncated\": %i }", count, truncated);

    send_response(sock, 200, json);

    free(json);
    free(dbuf);

    return 0;
}

int handle_fstat(int sock, struct paramdict *params) {
    char *path = paramdict_search(params, "path");
    if(!path) {
        return 1;
    }

    struct stat sb;
    memset(&sb, 0, sizeof(sb));
    int r = stat(path, &sb);
    if(r) {
        send_err(sock, "stat failed", r);
        return 0;
    }

    char json[512];
    snprintf(json, sizeof(json),
             "{ \"size\": %llu, \"mode\": %u, \"uid\": %u, \"gid\": %u, "
             "\"mtime\": %lli, \"blocks\": %llu, \"blksize\": %u, \"dir\": %i }",
             (unsigned long long)sb.st_size, (unsigned int)sb.st_mode,
             (unsigned int)sb.st_uid, (unsigned int)sb.st_gid,
             (long long)sb.st_mtim.tv_sec, (unsigned long long)sb.st_blocks,
             (unsigned int)sb.st_blksize, S_ISDIR(sb.st_mode) ? 1 : 0);
    send_response(sock, 200, json);

    return 0;
}

// Raw file bytes. offset and length are optional; without them the whole file
// comes back. The client is expected to loop for anything large.
int handle_dl(int sock, struct paramdict *params) {
    char *path = paramdict_search(params, "path");
    if(!path) {
        return 1;
    }

    char *soff = paramdict_search(params, "offset");
    char *slen = paramdict_search(params, "length");

    struct stat sb;
    memset(&sb, 0, sizeof(sb));
    if(stat(path, &sb)) {
        send_err(sock, "stat failed", -1);
        return 0;
    }

    int fd = open(path, O_RDONLY, 0);
    if(fd < 0) {
        send_err(sock, "cannot open file", fd);
        return 0;
    }

    uint64_t off = soff ? strtoull(soff, NULL, 0) : 0;
    uint64_t size = (uint64_t)sb.st_size;
    if(off > size) {
        off = size;
    }

    uint64_t want = slen ? strtoull(slen, NULL, 0) : (size - off);
    if(want > size - off) {
        want = size - off;
    }

    if(off && lseek(fd, (off_t)off, 0) < 0) {
        close(fd);
        send_err(sock, "seek failed", -1);
        return 0;
    }

    if(send_raw_header(sock, (int)want, "application/octet-stream")) {
        close(fd);
        return 0;
    }

    unsigned char *buf = (unsigned char *)pfmalloc(XFER_CHUNK);
    if(!buf) {
        close(fd);
        return 1;
    }

    uint64_t done = 0;
    while(done < want) {
        int chunk = (int)((want - done > XFER_CHUNK) ? XFER_CHUNK : (want - done));
        int got = (int)read(fd, buf, chunk);
        if(got <= 0) {
            break;
        }
        if(send_all(sock, buf, got)) {
            break;
        }
        done += got;
    }

    free(buf);
    close(fd);

    uprintf("dl %s: %llu of %llu bytes", path, done, want);

    return 0;
}

// -------------------------------------------------------------------------
// Kernel dump.
//
// sys_kern_rw is a bare memcpy with no fault handler, so reading an address
// that is not mapped kills the console outright rather than returning an
// error. The 13.00 kernel maps exactly two windows with an 0x8218A8 hole
// between them -- taken from the PT_LOAD headers of the kernel image -- and
// this endpoint refuses anything that is not wholly inside one of them.
// -------------------------------------------------------------------------

struct kwindow {
    uint64_t start;
    uint64_t end;
};

static const struct kwindow kwin_1300[] = {
    { 0x0000000, 0x0CFE758 },   // text, r-x
    { 0x1520000, 0x2834AF0 },   // data and bss, rw-
};

int handle_kdump(int sock, struct paramdict *params) {
    char *saddr = paramdict_search(params, "address");
    char *slen = paramdict_search(params, "length");

    uint64_t kbase = 0;
    if(sys_kern_base(&kbase) || !kbase) {
        send_err(sock, "cannot read kernel base", -1);
        return 0;
    }

    // Without an address, report the windows instead of guessing one. That is
    // what a client needs in order to ask for anything at all.
    if(!saddr || !slen) {
        char json[384];
        snprintf(json, sizeof(json),
                 "{ \"kernbase\": %llu, \"windows\": ["
                 "{ \"offset\": %llu, \"length\": %llu, \"name\": \"text\" }, "
                 "{ \"offset\": %llu, \"length\": %llu, \"name\": \"data\" } ] }",
                 (unsigned long long)kbase,
                 (unsigned long long)kwin_1300[0].start,
                 (unsigned long long)(kwin_1300[0].end - kwin_1300[0].start),
                 (unsigned long long)kwin_1300[1].start,
                 (unsigned long long)(kwin_1300[1].end - kwin_1300[1].start));
        send_response(sock, 200, json);
        return 0;
    }

    uint64_t addr = strtoull(saddr, NULL, 0);
    uint64_t want = strtoull(slen, NULL, 0);

    // Accept either an absolute kernel address or a kbase-relative offset.
    uint64_t off = (addr >= kbase) ? (addr - kbase) : addr;

    if(!want || want > 0x800000) {
        send_err(sock, "length must be 1..0x800000", 0);
        return 0;
    }

    int ok = 0;
    unsigned int i;
    for(i = 0; i < sizeof(kwin_1300) / sizeof(kwin_1300[0]); i++) {
        if(off >= kwin_1300[i].start && off + want <= kwin_1300[i].end) {
            ok = 1;
            break;
        }
    }

    if(!ok) {
        send_err(sock, "range is outside the mapped kernel windows", 0);
        return 0;
    }

    if(send_raw_header(sock, (int)want, "application/octet-stream")) {
        return 0;
    }

    unsigned char *buf = (unsigned char *)pfmalloc(XFER_CHUNK);
    if(!buf) {
        return 1;
    }

    uint64_t done = 0;
    while(done < want) {
        uint64_t chunk = (want - done > XFER_CHUNK) ? XFER_CHUNK : (want - done);
        if(sys_kern_rw(kbase + off + done, buf, chunk, 0)) {
            break;
        }
        if(send_all(sock, buf, (int)chunk)) {
            break;
        }
        done += chunk;
    }

    free(buf);

    uprintf("kdump +0x%llx: %llu of %llu bytes", off, done, want);

    return 0;
}

int handle_verbose(int sock, struct paramdict *params) {
    char *on = paramdict_search(params, "on");
    if(on) {
        http_verbose = (int)strtoull(on, NULL, 0) ? 1 : 0;
    }

    char json[96];
    snprintf(json, sizeof(json), "{ \"verbose\": %i }", http_verbose);
    send_response(sock, 200, json);

    return 0;
}

struct api_operation operations[] = {
    { "list", handle_list },
    { "info", handle_info },
    { "mapping", handle_mapping },
    { "write", handle_write },
    { "read", handle_read },
    { "alloc", handle_alloc },
    { "free", handle_free },
    { "pause", handle_pause },
    { "resume", handle_resume },
    { "sysctl", handle_sysctl },
    { "sensors", handle_sensors },
    { "signal", handle_signal },
    { "thrinfo", handle_thrinfo },
    { "kernbase", handle_kernbase },
    { "kread", handle_kread },
    { "fan", handle_fan },
    { "storage", handle_storage },
    { "ls", handle_ls },
    { "fstat", handle_fstat },
    { "dl", handle_dl },
    { "kdump", handle_kdump },
    { "verbose", handle_verbose },
    { "", 0 }
};

int handle_operation(int sock, char *operation, struct paramdict *params) {
    int i;

    for(i = 0; ; i++) {
        struct api_operation *oper = &operations[i];
        if(!oper->handler) {
            break;
        }

        if(!strcmp(oper->name, operation)) {
            vprintf("dispatching %s...", operation);
            return oper->handler(sock, params);
        }
    }

    return 1;
}

int handle_request(int sock) {
    char *buffer;
    int buffersize;
    int offset;
    int recvsize;
    int shouldcontinue;
    int i;

    offset = 0;
    buffersize = 4096;
    buffer = (char *)pfmalloc(buffersize);
    memset(buffer, 0, buffersize);

    shouldcontinue = 1;
    while(1) {
        recvsize = sceNetRecv(sock, buffer + offset, buffersize - offset, 0);

        if(recvsize) {
            shouldcontinue = 1;

            // search for \r\n\r\n which tells us it is the end
            for(i = 0; i < buffersize; i++) {
                if(!strncmp(buffer + i, "\r\n\r\n", 4)) {
                    shouldcontinue = 0;
                    break;
                }
            }

            if(shouldcontinue) {
                buffersize += 4096;
                offset += recvsize;
                buffer = (char *)realloc(buffer, buffersize);
            } else {
                shouldcontinue = 1;
                break;
            }
        } else {
            shouldcontinue = 0;
            break;
        }

    }

    struct paramdict *pd = paramdict_alloc();

    if(shouldcontinue) {
        // we only handle GET requests with parameters inside the url
        if(strncmp(buffer, "GET", 3)) {
            send_response(sock, 405, NULL);
            goto finish;
        }

        // the handling of this input will destroy the buffer's structure

        // break first line
        *strstr(buffer, "\r\n") = 0;

        // break second space
        *strstr(strstr(buffer, " ") + 1, " ") = 0;

        char *path = buffer + 4 + 1; // + 1 skip past the 'GET /'

        char operation[32];
        memset(operation, 0, sizeof(operation));

        char *qmark = strstr(path, "?");
        if(qmark) {
            strncpy(operation, path, qmark - path);
            
            char *params = path + strlen(operation) + 1;
            char *p = strtok(params, "&");
            while(p != NULL) {
                char *equalsign = strstr(p, "=");
                *equalsign = 0;
                paramdict_add(pd, p, equalsign + 1);

                p = strtok(NULL, "&");
            }
        } else {
            strncpy(operation, path, sizeof(operation));
        }
        
        vprintf("request path: %s", path);

        if(handle_operation(sock, operation, pd)) {
            send_response(sock, 404, NULL);
            goto finish;
        }
    }

finish:
    free(buffer);
    paramdict_free(pd);

    return 0;
}

int resolve() {
    int libc = sceKernelLoadStartModule("libSceLibcInternal.sprx", 0, NULL, 0, 0, 0);
    
    RESOLVE(libc, strtok);
    RESOLVE(libc, strtoull);
    RESOLVE(libKernelHandle, sceKernelGetSocSensorTemperature);

    return 0;
}

int start_http_server() {
    struct sockaddr_in server;
    struct sockaddr_in client;
    unsigned int len = sizeof(client);
    int serv, fd, r;

    uprintf("ps4 trainer http server");

    if(resolve()) {
        return 1;
    }

    // server structure
    server.sin_len = sizeof(server);
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = IN_ADDR_ANY;
    server.sin_port = sceNetHtons(SERVER_PORT);
    memset(server.sin_zero, NULL, sizeof(server.sin_zero));

    // start up server
    serv = sceNetSocket("httpmodsrv", AF_INET, SOCK_STREAM, 0);
    if(serv < 0) {
        uprintf("could not create socket!");
        return 1;
    }

    r = sceNetBind(serv, (struct sockaddr *)&server, sizeof(server));
    if(r) {
        uprintf("bind failed!");
        return 1;
    }

    r = sceNetListen(serv, 32);
    if(r) {
        uprintf("bind failed!");
        return 1;
    }

    while(1) {
        scePthreadYield();

        errno = NULL;
        fd = sceNetAccept(serv, (struct sockaddr *)&client, &len);
        if(fd > -1 && !errno) {
            vprintf("accepted a new client");

            if(handle_request(fd)) {
                uprintf("error handling client");
                break;
            }

            sceNetSocketClose(fd);
        }

        sceKernelUsleep(50000);
    }

    sceNetSocketClose(serv);

    return 0;
}
