#ifndef KVM_H_
#define KVM_H_

#include <stdint.h>
#include <string.h>
#include <linux/kvm.h>

void kvmInit();

// System ioctls
int kvmCreateVm();
size_t kvmGetVcpuMmapSize();
struct kvm_cpuid2* kvmGetSupportedCpuid();

// VM ioctls
int kvmCreateVcpu(int vmFd, int cpuid);
void kvmSetUserMemoryRegion(int vmFd, uint64_t memorySize, void* userspaceAddr);
void kvmSetTssAddr(int vmFd, unsigned long addr);
void kvmCreateIrqchip(int vmFd);
void kvmCreatePit2(int vmFd);

// vCPU ioctls
void kvmRun(int vcpuFd);
void kvmGetRegs(int vcpuFd, struct kvm_regs* regs);
void kvmSetRegs(int vcpuFd, const struct kvm_regs* regs);
void kvmGetSregs(int vcpuFd, struct kvm_sregs* sregs);
void kvmSetSregs(int vcpuFd, const struct kvm_sregs* sregs);
void kvmSetCpuid2(int vcpuFd, struct kvm_cpuid2* cpuid);

#endif // KVM_H_
