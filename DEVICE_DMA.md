# Device-initiated DMA: reservation foundation

This candidate prepares exclusive PCIe DMA queue ownership for Qwen PLE
lookups on four Ascend 310P NPUs. It is **installed, with reservation lifecycle validated on four physical NPUs**.
It does not yet expose a device submission interface. The v2 candidate adds
a drained, permanent host routing transition to resolve the fixed-instance
conflict found in v1. All-rank ACL initialization and ordinary host DMA coexistence passed for v2;
QFN serving is qualified with all four reservations enabled.

## Evidence and intended data path

Private-buffer tests in the vLLM Ascend workspace established that the host
can convert ordinary shared read-only pages, sealed memfds, and ACL host
allocations into DMA descriptors and copy exact data to all four NPUs.
The AICore PLE selector also passed exact row/offset and graph replay tests.

A custom AI CPU function executed on device, but its loaded
`drvMemConvertAddr` entry matched the SDK device stub byte for byte: it
returned success without constructing a descriptor. A symbol lookup or a
zero return code is therefore insufficient capability evidence. Device-side
inventory showed a separate ARM Linux environment, not the x86 host kernel.
Changing this host fork alone does not replace that device library or driver.

The intended RAM path is:

```text
cold host setup:
  shared immutable table -> pin + per-device DMA mappings
  reserve queue + fixed destinations + generation-scoped ownership

steady-state device work:
  token/history -> AICore row IDs -> source mapping lookup
  -> device submission -> PCIe DMA -> device completion -> PLE projection

cold teardown:
  revoke producer -> drain/reset with proof -> unmap -> unpin
```

The 95.37-GiB table must remain one shared physical object across ranks.
Per-device DMA translations may differ. Neither a host virtual address nor a
host driver's opaque descriptor handle is a portable device address.

## Implemented dynamic reservation

The regular driver loads with **all channels available to ordinary clients**.
There is no reservation module parameter. A model loader opens
`/dev/ascend_device_dma` and explicitly acquires a reservation through an ioctl.
The node is initially root-only; deployment may delegate access through its
normal device-permission policy. The existing vendor HAL ABI is unchanged.

The 64-byte, versioned UAPI is in `inc/driver/ascend_device_dma.h`:

| ioctl | Purpose |
| --- | --- |
| `ASCEND_DEVICE_DMA_ACQUIRE` | Reserve one idle traffic channel on the requested physical 310P |
| `ASCEND_DEVICE_DMA_QUERY` | Report the current lease on this open file |
| `ASCEND_DEVICE_DMA_RELEASE` | Release the matching device/channel/generation lease |

One lease belongs to one open file description; dup/inherited FDs share its
lifetime. Each physical NPU permits one lease. Keep its FD open throughout
model use. An explicit release validates the device, hardware channel, and
monotonically increasing generation. Last close releases an idle lease.
Reset detaches the device and invalidates the old FD before freeing ring
storage; reusing a device ID does not revive that FD. An unexpectedly nonempty
queue on close is quarantined until reset rather than returned to host use.

Admission selects the final traffic channel, leaves at least one other traffic
channel, and rejects overlap with another pool. It locks CQ processing and
submission in that order, verifies that completed SQ head equals submitted SQ
tail, and marks ownership atomically. If a transfer or completion callback is
still active, acquisition returns `EBUSY` with no reservation; the loader may
retry. It never waits for hardware while holding a spinlock. Other chip
families, virtual modes, unavailable devices, invalid ABI fields, duplicate
owners, and stale releases are rejected. A failed acquisition reply copy
releases the newly acquired lease.

Host pool ranges and fixed-instance modulo identities stay immutable. The first
successful acquisition drains the selected queue under CQ/SQ locks, then
permanently redirects its fixed host instances to the first traffic channel.
Publication shares the submission lock: a caller that selected the old queue
before acquisition gets `EBUSY` internally and retries only its stable successor.
`ENOSPC` remains `ENOSPC`; ordered streams never retry arbitrary queues.
Round-robin selection skips the retired channel, including after lease close.

Release removes ownership **but does not undo the host redirect**: work may
still be outstanding on the successor. Only channel destruction/reset restores
the original host pool. Even a failed acquire reply can therefore leave one
channel retired from host use. This one-time capacity cost prevents routing
reversal races. Regular driver startup still reserves and retires nothing.
Direct physical, virtual-channel, and small-packet submission guards reject
both leased and retired channels; clients caching a physical channel pointer
must reacquire it instead of treating it as a logical fixed instance.
Host completion processing and its periodic guard skip leased channels.
SR-IOV reassignment is serialized with admission and refuses an active lease.
Steady-state dispatch does not take the lease registry mutex or wait for drain.

The read-only `device_dma` attribute alongside `dma_info` reports the selected
logical device, reserved hardware channel, available traffic channels, and
queue depths. Like existing PCI diagnostics, `davinci_dev_id` selects the
logical device. It performs no firmware RPC and exposes no physical addresses.

The v2 candidate reports `ASCEND_DEVICE_DMA_CAP_HOST_ROUTING_ISOLATED` (bit 0)
in the ioctl response and
`device_submit=0` in sysfs. **It reserves an idle channel but does not expose
its memory, doorbells, or a device producer.** The existing fault/reset machinery
remains in place. Adding producer activation requires a separate revocation,
drain, and memory-lifetime contract; closing a reservation FD alone must not
be treated as proof that a future device producer has stopped.

The vLLM workspace has a startup-only client in `tools/qwen4exp/ple_driver.py`:

```python
from tools.qwen4exp.ple_driver import DeviceDMALease

with DeviceDMALease.acquire(driver_device_id) as lease:
    info = lease.query()
    # Host routing isolation is separate from device submission capability.
    # Keep this scope alive throughout the eventual model/transport lifetime.
```

The serving entry point now accepts repeated `--dma-reserve-device ID` options.
It acquires the complete group before ACL initialization, requires host routing
isolation, holds FDs throughout API lifetime, and closes partial acquisitions
on failure. Default launches do not acquire anything. Deployment must qualify
coexistence before enabling these options. Closing a lease leaves the one-time
host routing transition in place until reset.

## v1 validation, 2026-10-14

- `make all KERNEL_UNAME=7.0.0-38-generic CC=x86_64-linux-gnu-gcc DKMS_JOBS=8`
  passed for the bare-metal profile.
- Nine CPU regression tests passed. The new C test executes the actual
  planner, admission, release, ioctl, and host dispatch functions with mocked
  kernel services over channel counts 1 through 32. They cover default startup,
  busy queues, pool overlap, range overflow, competing owners, stale generations,
  copy faults, close cleanup, reset/reacquisition, quarantine, and 87,264
  successful host dispatches. Reserved fixed instances are rejected and shared
  dispatch never submits to a leased channel. These tests do not prove hardware
  concurrency. The startup client has 16 additional passing offline tests.
- `scripts/verify-modules.py 7.0.0-38-generic --profile bare-metal` passed all
  31 modules: architecture, kernel release, strong symbols, version CRCs,
  namespace imports, and isolated dependency resolution.
- The candidate was installed on the host. A clean unload succeeded, but live
  reload timed out waiting for the device firmware boot state, so the host was
  rebooted. All 31 loaded module source versions match the candidate; initramfs
  and the DKMS source tree were updated. Original modules/source are backed up.
- All four physical NPUs passed default-unreserved startup, simultaneous lease
  acquisition, query, competing-owner rejection (`EBUSY`), stale release
  rejection (`ESTALE`), explicit release/reacquire, and last-close cleanup.
  Reserved hardware channels were 20/25 on each Duo, leaving two traffic
  channels per NPU. SQ/CQ depths were 65,536 entries.
- With no reservations, 16 exact page-crossing 320-byte DMA transfers passed
  across four NPUs and shared RW/RO, sealed-memfd, and ACL host allocations.
- With reservations acquired before ACL initialization, a fixed-instance host
  copy hit the reserved channel and returned `EBUSY` (ACL 507033). The guard
  behaved as designed, but the current reservation cannot coexist transparently
  with arbitrary vendor clients. All test reservations were released. No
  automatic serving acquisition or device submission is enabled; no speedup
  is claimed.

The local base is `99b8459` (including the separate virtualization port).
The previous host used `453e106`, whose hrtimer setup fix is included in this
candidate, together with its regression test. This preserves the installed
fix instead of packaging the older local timer callbacks. The current work
adds no new timer behavior beyond that already running on the host.

The module bundle and manifest are under
`build-artifacts/7.0.0-38-generic/bare-metal/`. New source changes in this
candidate have not been committed. Rebuild all modules together because an
internal shared channel structure changed; replacing only one module can
leave other modules with an incompatible layout.

## v2 hardware validation, 2026-10-14

- All 31 loaded module source versions match the validated v2 build after a
  planned reboot; the DKMS source and initramfs match the candidate.
- All four NPUs passed acquire/query, competing-owner rejection, stale release,
  explicit release/reacquire, and last-close cleanup with capability bit 0 set.
- Three rounds of four simultaneous ACL processes ran in each of three phases:
  before leases, with all four leases held, and after close. All **144 exact
  page-crossing DMA transfers** passed across RW/RO shared mappings, sealed
  memfds, and ACL host allocations. The v1 ACL startup failure did not recur.
- Sysfs confirms one retired host channel and two available traffic channels
  per NPU after release. Server startup acquired channels 20/25 on each Duo
  using the unprivileged model-server account. The udev rule delegates the node
  to the existing `HwHiAiUser` group (0660).
- The Python client/server tests pass 62 cases, including partial acquisition,
  capability rejection, and FD lifetime through server startup failure.
- Full model loading completed (slowest rank 116.85 seconds), six ten-case
  model suites passed, and the final 12-case serving gate passed with leases
  held throughout. All retained controls and graph sizes 3/6/9/12 were restored.
  C1 median HTTP throughput was 31.93 tok/s (3 x 256); C4 aggregate was
  63.43 tok/s (4 x 256), versus 32.39/62.38 in the earlier unreserved run.
  These short checks do not establish a speedup. Existing PLE eager work remains.
- These are reservation and coexistence results, not device-initiated DMA or
  a PLE speedup. Device submission remains disabled.

## Next implementation gates

The v2 drained/sticky routing transition passed ordinary host copies and
full model loading/execution while leases remained open. The v2 CPU tests cover
87,934 successful host dispatches, acquisition between route selection and
submission, no redirect on failed drain, fixed-route exhaustion, and stable
routing after release with outstanding work on the successor.

1. Establish a real device-side control interface. The custom AI CPU context
   can run native code, but its HAL conversion stub cannot submit DMA. Identify
   the device driver mapping/queue mechanism and its source or supported ABI;
   a host-only capability change must not advertise device access.
2. Extend the implemented FD-owned, generation-scoped reservation with exact
   source/destination region bounds and producer activation. Hold long-term pins and per-device DMA mappings until producer revocation
   and DMA drain are proven. Process exit/reset must invalidate old requests;
   no raw physical addresses or arbitrary MMIO ranges should be caller supplied.
3. Expose only the reserved queue and its required doorbells to that owner.
   Prove device descriptor publication, queue-tail visibility, payload-before-
   completion ordering, CQ wrap, timeout, and reset behavior on small buffers.
   A host C polling/submission thread does not satisfy device initiation.
4. Exercise page-crossing 320-byte rows, repeated rows, variable row sets,
   and concurrent use with the normal host DMA pools. Requalify driver/HAL
   startup, DMA, reset, and unload before admitting the full table.
5. Connect the already-qualified device row selector, benchmark complete
   PLE fetch/projection, then integrate into serving graphs. The SSD backend
   additionally requires filesystem/block lifetime and storage queue ownership;
   saved file extents alone are insufficient.

## v3 host source registration

The additive ABI-v1 source ioctls are `SOURCE_REGISTER` (`0xc04044d3`),
`SOURCE_QUERY` (`0xc04044d4`), and `SOURCE_RELEASE` (`0x404044d5`). Capability
bit 1 reports source registration independently of host-routing isolation.
Neither bit reports device submission. Each source structure is 64 bytes.

One immutable host range may be registered per live lease. Registration pins
the calling process's pages with `FOLL_LONGTERM`, charges `RLIMIT_MEMLOCK`,
and maps the exact first/last-byte bounds with `DMA_TO_DEVICE` for that NPU.
Replies contain generation-scoped region IDs, page counts, and mapped segment
counts. They expose no physical addresses, DMA addresses, or MMIO mappings.
An application must populate the range before registration and keep it immutable
until release. A shared sealed memfd is suitable; the registration API itself
does not enforce immutability against writes through other mappings.

The range limit is 128 GiB; this is a bound, not evidence that full-size table
registration has been hardware-qualified. Four leases may pin the same shared
table without copying its physical contents, but each lease separately charges
locked-memory accounting and allocates page/SG metadata. The full PLE table
therefore needs an appropriate memlock limit and a separate metadata/memory
pressure qualification. Registration runs only at startup and serializes with
lease/reset operations; it does not add a hot-path registry lock.

Allocation, partial pin, scatterlist, mapping, and reply-copy failures unwind
resources. Source release retains the queue lease. Lease release, last close,
and device detach clean up sources and invalidate their handles. This cleanup
is valid because **no source address is published or submitted yet**. Enabling
a producer requires changing teardown to revoke and drain before unmapping;
the current close path must never be reused unchanged for active DMA.

The v3 driver is installed. All 31 loaded modules match the candidate after
boot `aad0b818-d05a-4ccf-9812-b035a444bee4`. Twelve page-crossing registrations
of one shared sealed read-only memfd passed across all four NPUs. Locked-memory
limit enforcement, stale handles, explicit source release, and last-close
cleanup passed; the calling process's locked-memory accounting returned to its
baseline. All 144 ordinary host DMA transfers passed before, during (with four
source mappings held), and after leases. These checks did not submit DMA using
the new registered sources. Full-table registration remains unqualified.

The actual kernel registration code is exercised with injected allocation,
memlock, partial-pin (including a later batch), SG-allocation and DMA-mapping
failures. Lease tests cover stale region IDs, failed replies, explicit release,
close, and detach. Hardware evidence is recorded in the vLLM workspace under
`ple-driver-source-hardware-v3.json` and `ple-driver-dma-coexist-v3.json`.
