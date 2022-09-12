#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <asm/bootparam.h>
#include <asm/e820.h>
#include <linux/serial_reg.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include "kvm.h"

#define COM1_PORT           (0x03F8)

// https://crosvm.dev/book/appendix/memory_layout.html
#define BOOT_PARAMS_ADDR    (0x00007000)
#define CMDLINE_ADDR        (0x00020000)
#define KERNEL_ADDR         (0x00200000)
#define MEMORY_SIZE         (0x40000000) // 1GB

#define IO_DATA8 \
    (*(int8_t*)((void*)run + run->io.data_offset))

static void setVcpuCpuId(int vcpuFd)
{
    struct kvm_cpuid2* cpuid = kvmGetSupportedCpuid();

    kvmSetCpuid2(vcpuFd, cpuid);
    free(cpuid);
}

static void setVcpuRegs(int vcpuFd)
{
    struct kvm_sregs sregs;
    kvmGetSregs(vcpuFd, &sregs);
    sregs.cs.base = 0;
    sregs.cs.limit = ~0;
    sregs.cs.g = 1;
    sregs.cs.db = 1;
    sregs.ds.base = 0;
    sregs.ds.limit = ~0;
    sregs.ds.g = 1;
    sregs.fs.base = 0;
    sregs.fs.limit = ~0;
    sregs.fs.g = 1;
    sregs.gs.base = 0;
    sregs.gs.limit = ~0;
    sregs.gs.g = 1;
    sregs.es.base = 0;
    sregs.es.limit = ~0;
    sregs.es.g = 1;
    sregs.ss.base = 0;
    sregs.ss.limit = ~0;
    sregs.ss.g = 1;
    sregs.ss.db = 1;
    sregs.cr0 |= 1; // Enable protected mode
    kvmSetSregs(vcpuFd, &sregs);

    struct kvm_regs regs;
    kvmGetRegs(vcpuFd, &regs);
    regs.rflags = 2;
    regs.rip = KERNEL_ADDR;
    regs.rsi = BOOT_PARAMS_ADDR;
    kvmSetRegs(vcpuFd, &regs);
}

static void loadBzImage(void* mem, const char* path)
{
    int imageFd = open(path, O_RDONLY);
    if (imageFd < 0) {
        err(1, "failed to open %s", path);
    }

    size_t imageSize;
    struct stat st;
    fstat(imageFd, &st);
    imageSize = st.st_size;

    void* image = mmap(0, imageSize, PROT_READ, MAP_PRIVATE, imageFd, 0);
    close(imageFd);

    // Load boot parameters
    struct boot_params* params = mem + BOOT_PARAMS_ADDR;
    memcpy(params, image, sizeof(struct boot_params));

    // Load kernel
    size_t setupSize = (params->hdr.setup_sects + 1) * 512;
    memcpy(mem + KERNEL_ADDR, image + setupSize, imageSize - setupSize);

    munmap(image, imageSize);
}

static void setBootParams(void* mem, const char* cmdline)
{
    struct boot_params* params = mem + BOOT_PARAMS_ADDR;
    params->hdr.type_of_loader = 0xFF;
    params->hdr.ramdisk_image = 0x0;
    params->hdr.ramdisk_size = 0x0;
    params->hdr.ext_loader_ver = 0x0;
    params->hdr.cmd_line_ptr = CMDLINE_ADDR;
    params->hdr.cmdline_size = strlen(cmdline);

    // Declare usable memory regions
    params->e820_entries = 1;
    params->e820_table[0] = (struct boot_e820_entry) {
        .addr = KERNEL_ADDR,
        .size = MEMORY_SIZE - KERNEL_ADDR, // FIXME need to reserve space for MMIO above ~3GB
        .type = E820_RAM,
    };

    // Set command line
    memcpy(mem + CMDLINE_ADDR, cmdline, params->hdr.cmdline_size);
}

static void handleIoSerial(struct kvm_run* run, unsigned index)
{
    if (run->io.direction == KVM_EXIT_IO_IN && index == UART_LSR) {
        IO_DATA8 = UART_LSR_THRE; // Ready to transmit
    }

    if (run->io.direction == KVM_EXIT_IO_OUT && index == UART_TX) {
        putchar(IO_DATA8);
    }
}

static void handleIo(struct kvm_run* run)
{
    switch(run->io.port) {
    case 0x0061:
        // Dunno
        break;
    case 0x03B4:
    case 0x03B5:
        // VGA, ignore
        break;
    case 0x0CF8:
    case 0x0CFC:
    case 0x0CFE:
        // PCI, ignore
        break;
    case COM1_PORT + 0:
    case COM1_PORT + 1:
    case COM1_PORT + 2:
    case COM1_PORT + 3:
    case COM1_PORT + 4:
    case COM1_PORT + 5:
    case COM1_PORT + 6:
    case COM1_PORT + 7:
        handleIoSerial(run, run->io.port - COM1_PORT);
        break;
    default:
        printf("dir=%d size=%d port=%X count=%d offset=%llu\n",
               run->io.direction, run->io.size, run->io.port, run->io.count, run->io.data_offset);
    }
}

int main(int argc, char* argv[])
{
    kvmInit();

    int vmFd = kvmCreateVm();
    kvmCreateIrqchip(vmFd);
    kvmCreatePit2(vmFd);

    void *mem = mmap(NULL, MEMORY_SIZE, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, -1, 0);
    kvmSetUserMemoryRegion(vmFd, MEMORY_SIZE, mem);

    int vcpuFd = kvmCreateVcpu(vmFd, 0);
    setVcpuCpuId(vcpuFd);
    setVcpuRegs(vcpuFd);

    loadBzImage(mem, "/boot/vmlinuz-linux");
    setBootParams(mem, "earlyprintk=ttyS0,keep");

    size_t runSize = kvmGetVcpuMmapSize();
    struct kvm_run* run = mmap(NULL, runSize, PROT_READ | PROT_WRITE, MAP_SHARED, vcpuFd, 0);

    while (1) {
        kvmRun(vcpuFd);

        switch (run->exit_reason) {
        case KVM_EXIT_IO:
            handleIo(run);
            break;
        default:
            printf("unhandled exit reason: %d\n", run->exit_reason);
        }
    }

    return 0;
}
