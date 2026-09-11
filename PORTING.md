# Linux 7 port notes

## Scope and source baseline

Target: x86-64, Ubuntu `7.0.0-31-generic`, GCC 15.2.0, Ascend 310P 26.0.rc1
host kernel sources, normal physical-device operation.

The port keeps the 31 non-`drv_vascend` modules from the vendor's normal-mode
DKMS manifest. This includes `drv_vascend_stub`, the host virtualization
interface stub already shipped by the vendor, plus the host virtual-management
and VPC infrastructure referenced by the physical-device stack. A stub replacing
the excluded `drv_vascend` implementation was not introduced.

The vLLM Ascend repository is a separate project and was left unchanged.

## Kernel API evidence

The installed headers and `/lib/modules/7.0.0-31-generic/build/Module.symvers`
are the authority for the build and exported symbol versions.

| Area | Port treatment |
| --- | --- |
| Kbuild | Modern `scripts/Makefile.lib` uses `ccflags-y`; all legacy module-local flags migrated mechanically. |
| Timers | A small forced-include header supplies the removed legacy names for Linux 6.15 and newer. It includes only `linux/version.h`, avoiding early inclusion of printk before a source file defines `pr_fmt`. |
| Hrtimers | Existing callers initialize the timer, assign its callback, then start it. `hrtimer_setup(..., NULL, ...)` preserves that sequence. |
| PFN lookup | `follow_pfnmap_start` acquires page-table locks; the wrapper copies the PFN before `follow_pfnmap_end` and releases nothing on a failed start. The caller must still hold the mmap lock and protect the mapping lifetime. |
| Unmapped area | Use `mm_get_unmapped_area`; the mm argument was removed in Linux 6.19. Calling the generic file-dispatching `get_unmapped_area` here could recurse into the same file operation. |
| PMD entries | On Linux 7, use `pmd_is_migration_entry` and `pmd_leaf` in the vendor walker. The three separately packaged copies receive the same change. |
| File constness | Return a const path pointer. Preserve the vendor's private file snapshot initialization without modifying the live VFS file's path. |
| CRC table | Keep the adapter's exported pointer-returning API and use a const, generated table for reflected polynomial `0xa001`. Known CRC-16/ARC and MODBUS vectors test the table. |

Primary references:

- [Linux timer API documentation](https://www.kernel.org/doc/html/v6.15/driver-api/basics.html).
- [Linux 6.13 PFN lookup declarations](https://github.com/torvalds/linux/blob/v6.13/include/linux/mm.h).
- [Linux 7 PFN lookup implementation and locking contract](https://github.com/torvalds/linux/blob/v7.0/mm/memory.c).
- [VFS change making `f_path` const](https://kernel.googlesource.com/pub/scm/linux/kernel/git/viro/vfs/+/2f7d98f10b8f64525b2c74cae7d70ae5278eb654).
- [Linux 6.19 unmapped-area API](https://github.com/torvalds/linux/blob/v6.19/include/linux/sched/mm.h).
- [Linux symbol namespaces](https://docs.kernel.org/6.14/core-api/symbol-namespaces.html).

## Virtualization limitation

The full vendor build fails in `vascend_drv/kvmdt.c` on removed `gfn_to_pfn`
and `kvm_release_pfn_clean`, and in `vascend_drv/dma_pool_pt.c` on removed
`iommu_domain_alloc`. More fundamentally, target-kernel exports including
`kvm_read_guest`, `kvm_write_guest`, `gfn_to_hva`, and `gfn_to_memslot` are
restricted to the namespace `module:kvm-amd,kvm-intel`.

The [Linux 7 namespace rules](https://github.com/torvalds/linux/blob/v7.0/Documentation/core-api/symbol-namespaces.rst)
and [KVM implementation](https://github.com/torvalds/linux/blob/v7.0/virt/kvm/kvm_main.c)
make these private exports different from an ordinary importable namespace.
Adding `MODULE_IMPORT_NS` is insufficient. A real virtualization port requires
reworking the integration around supported interfaces or coordinated kernel
changes. This fork rejects that build profile on the target kernel.

## Reproduction and evidence

`scripts/build-host.sh` performs a clean build, runs three CPU regression tests,
and calls `scripts/verify-modules.py`. The verifier requires the DKMS manifest to
match `modules.order` exactly, then checks all 31 actual ELF files, vermagic,
strong symbol references, every modversion CRC, and namespace imports. Its JSON
report contains module hashes and dependencies, and it packages the validated
modules without installing them.

Initial failure logs and subsequent build logs remain locally in `build-logs/`.
They cover the pristine build, Kbuild flag migration, API adaptation, and the
first successful strict modpost. The final clean-build log is
`build-logs/build-7.0.0-31-generic.log`; final module metadata is in
`build-artifacts/7.0.0-31-generic/validation.json`.

Neither unit tests nor symbol checks establish that a module can initialize a
card correctly. Runtime validation is still required for the PFN lifetime
assumptions, the existing manual page-table walk, the private file snapshot,
HAL interaction, DMA, resets, and device teardown. No card, host driver, HAL,
firmware, or system module configuration was installed or modified in this work.
