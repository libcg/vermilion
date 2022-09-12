#include <err.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include "kvm.h"

#define MAX_CPUID_ENTRIES   (128)

static int mKvmFd;

void kvmInit()
{
    mKvmFd = open("/dev/kvm", O_RDWR);
    if (mKvmFd < 0) {
        err(1, "failed to open KVM");
    }
}

// System ioctls

int kvmCreateVm()
{
    int res = ioctl(mKvmFd, KVM_CREATE_VM, 0);
    if (res < 0) {
        err(1, "KVM_CREATE_VM failed");
    }

    return res;
}

size_t kvmGetVcpuMmapSize()
{
    int res = ioctl(mKvmFd, KVM_GET_VCPU_MMAP_SIZE, 0);
    if (res < 0) {
        err(1, "KVM_GET_VCPU_MMAP_SIZE failed");
    }

    return res;
}

struct kvm_cpuid2* kvmGetSupportedCpuid()
{
    struct kvm_cpuid2* cpuid = malloc(sizeof(struct kvm_cpuid2) +
                                      MAX_CPUID_ENTRIES * sizeof(struct kvm_cpuid_entry2));

    cpuid->nent = MAX_CPUID_ENTRIES;

    int res = ioctl(mKvmFd, KVM_GET_SUPPORTED_CPUID, cpuid);
    if (res < 0) {
        err(1, "KVM_GET_SUPPORTED_CPUID failed");
    }

    return cpuid;
}

// VM ioctls

int kvmCreateVcpu(int vmFd, int cpuid)
{
    int res = ioctl(vmFd, KVM_CREATE_VCPU, cpuid);
    if (res < 0) {
        err(1, "KVM_CREATE_VCPU failed");
    }

    return res;
}

void kvmSetUserMemoryRegion(int vmFd, uint64_t memorySize, void* userspaceAddr)
{
    struct kvm_userspace_memory_region region = {
        .slot = 0,
        .flags = 0,
        .guest_phys_addr = 0x0,
        .memory_size = memorySize,
        .userspace_addr = (uint64_t)userspaceAddr,
    };

    int res = ioctl(vmFd, KVM_SET_USER_MEMORY_REGION, &region);
    if (res < 0) {
        err(1, "KVM_SET_TSS_ADDR failed");
    }
}

void kvmSetTssAddr(int vmFd, unsigned long addr)
{
    int res = ioctl(vmFd, KVM_SET_TSS_ADDR, addr);
    if (res < 0) {
        err(1, "KVM_SET_TSS_ADDR failed");
    }
}

void kvmCreateIrqchip(int vmFd)
{
    int res = ioctl(vmFd, KVM_CREATE_IRQCHIP, 0);
    if (res < 0) {
        err(1, "KVM_CREATE_IRQCHIP failed");
    }
}

void kvmCreatePit2(int vmFd)
{
    struct kvm_pit_config pitConf = {
        .flags = 0,
    };

    int res = ioctl(vmFd, KVM_CREATE_PIT2, &pitConf);
    if (res < 0) {
        err(1, "KVM_CREATE_PIT2 failed");
    }
}

// vCPU ioctls

void kvmRun(int vcpuFd)
{
    ioctl(vcpuFd, KVM_RUN, 0);
}

void kvmGetRegs(int vcpuFd, struct kvm_regs* regs)
{
    int res = ioctl(vcpuFd, KVM_GET_REGS, regs);
    if (res < 0) {
        err(1, "KVM_GET_REGS failed");
    }
}

void kvmSetRegs(int vcpuFd, const struct kvm_regs* regs)
{
    int res = ioctl(vcpuFd, KVM_SET_REGS, regs);
    if (res < 0) {
        err(1, "KVM_SET_REGS failed");
    }
}

void kvmGetSregs(int vcpuFd, struct kvm_sregs* sregs)
{
    int res = ioctl(vcpuFd, KVM_GET_SREGS, sregs);
    if (res < 0) {
        err(1, "KVM_GET_SREGS failed");
    }
}

void kvmSetSregs(int vcpuFd, const struct kvm_sregs* sregs)
{
    int res = ioctl(vcpuFd, KVM_SET_SREGS, sregs);
    if (res < 0) {
        err(1, "KVM_SET_SREGS failed");
    }
}

void kvmSetCpuid2(int vcpuFd, struct kvm_cpuid2* cpuid)
{
    int res = ioctl(vcpuFd, KVM_SET_CPUID2, cpuid);
    if (res < 0) {
        err(1, "KVM_SET_CPUID2 failed");
    }
}
