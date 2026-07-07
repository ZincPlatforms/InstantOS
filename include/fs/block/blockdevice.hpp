#pragma once

#include <stdint.h>

// Abstract byte-addressed block device.
//
// `offset` and `size` are in BYTES (not sectors). Implementations include the
// AHCI-backed whole-disk device, `PartitionBlockDevice` (a bounds-checked view
// onto a partition), and host-side test shims. A filesystem is always handed a
// device whose offset 0 is the start of the filesystem (i.e. partition-relative
// when mounted from a partition), so on-disk structures can be addressed with
// their documented byte offsets directly.
//
// This lives in its own header (rather than inside a specific filesystem's
// header) so every filesystem (FAT32, ext4, ...) and the partition layer share
// a single definition without cross-including each other.
class BlockDevice {
public:
    virtual ~BlockDevice() {}
    virtual bool read(uint64_t offset, void* buffer, uint64_t size) = 0;
    virtual bool write(uint64_t offset, const void* buffer, uint64_t size) = 0;
    virtual uint64_t getSize() = 0;
    // Flush any device-side write cache to stable storage (fsync barrier).
    // Default is a no-op success for devices/shims without a cache.
    virtual bool flush() { return true; }
};
