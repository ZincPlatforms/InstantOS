#include <fs/partition/partition.hpp>
#include <graphics/console.hpp>

PartitionBlockDevice::PartitionBlockDevice(BlockDevice* base, uint64_t startByte, uint64_t lengthByte)
    : base(base), startByte(startByte), lengthByte(lengthByte) {}

bool PartitionBlockDevice::read(uint64_t offset, void* buffer, uint64_t size) {
    if (!base) return false;
    if (offset > lengthByte || size > lengthByte - offset) return false;
    return base->read(startByte + offset, buffer, size);
}

bool PartitionBlockDevice::write(uint64_t offset, const void* buffer, uint64_t size) {
    if (!base) return false;
    if (offset > lengthByte || size > lengthByte - offset) return false;
    return base->write(startByte + offset, buffer, size);
}

uint64_t PartitionBlockDevice::getSize() {
    return lengthByte;
}

bool PartitionBlockDevice::flush() {
    return base && base->flush();
}

bool mbrIsFatType(uint8_t type) {
    switch (type) {
        case 0x01: // FAT12
        case 0x04: // FAT16 <32M
        case 0x06: // FAT16
        case 0x0B: // FAT32 CHS
        case 0x0C: // FAT32 LBA
        case 0x0E: // FAT16 LBA
            return true;
        default:
            return false;
    }
}

bool mbrFindFatPartition(BlockDevice* device, uint64_t* startByte, uint64_t* lengthByte, uint8_t* typeOut) {
    if (!device) return false;

    uint8_t sector[512];
    if (!device->read(0, sector, 512)) {
        Console::get().drawText("[MBR] Failed to read sector 0\n");
        return false;
    }

    uint16_t signature = (uint16_t)sector[510] | ((uint16_t)sector[511] << 8);
    if (signature != MBR_SIGNATURE) {
        Console::get().drawText("[MBR] No MBR signature (0xAA55)\n");
        return false;
    }

    for (int i = 0; i < MBR_MAX_PARTITIONS; i++) {
        MBRPartitionEntry entry;
        const uint8_t* src = sector + MBR_PARTITION_TABLE_OFFSET + i * 16;
        for (size_t j = 0; j < sizeof(MBRPartitionEntry); j++) {
            ((uint8_t*)&entry)[j] = src[j];
        }

        if (entry.type == 0x00 || entry.sectorCount == 0) {
            continue;
        }

        if (!mbrIsFatType(entry.type)) {
            continue;
        }

        if (startByte) *startByte = (uint64_t)entry.lbaStart * 512;
        if (lengthByte) *lengthByte = (uint64_t)entry.sectorCount * 512;
        if (typeOut) *typeOut = entry.type;

        Console::get().drawText("[MBR] FAT partition ");
        Console::get().drawNumber(i);
        Console::get().drawText(" type=0x");
        Console::get().drawNumber(entry.type);
        Console::get().drawText(" lbaStart=");
        Console::get().drawNumber((int64_t)entry.lbaStart);
        Console::get().drawText(" sectors=");
        Console::get().drawNumber((int64_t)entry.sectorCount);
        Console::get().drawText("\n");
        return true;
    }

    Console::get().drawText("[MBR] No FAT partition found\n");
    return false;
}

bool mbrIsLinuxType(uint8_t type) {
    // 0x83 = Linux native (ext2/3/4 and other Linux filesystems).
    return type == 0x83;
}

bool mbrFindPartition(BlockDevice* device, uint64_t* startByte, uint64_t* lengthByte, uint8_t* typeOut) {
    if (!device) return false;

    uint8_t sector[512];
    if (!device->read(0, sector, 512)) {
        Console::get().drawText("[MBR] Failed to read sector 0\n");
        return false;
    }

    uint16_t signature = (uint16_t)sector[510] | ((uint16_t)sector[511] << 8);
    if (signature != MBR_SIGNATURE) {
        Console::get().drawText("[MBR] No MBR signature (0xAA55)\n");
        return false;
    }

    for (int i = 0; i < MBR_MAX_PARTITIONS; i++) {
        MBRPartitionEntry entry;
        const uint8_t* src = sector + MBR_PARTITION_TABLE_OFFSET + i * 16;
        for (size_t j = 0; j < sizeof(MBRPartitionEntry); j++) {
            ((uint8_t*)&entry)[j] = src[j];
        }

        if (entry.type == 0x00 || entry.sectorCount == 0) {
            continue;
        }
        if (!mbrIsFatType(entry.type) && !mbrIsLinuxType(entry.type)) {
            continue;
        }

        if (startByte) *startByte = (uint64_t)entry.lbaStart * 512;
        if (lengthByte) *lengthByte = (uint64_t)entry.sectorCount * 512;
        if (typeOut) *typeOut = entry.type;

        Console::get().drawText("[MBR] partition ");
        Console::get().drawNumber(i);
        Console::get().drawText(" type=0x");
        Console::get().drawNumber(entry.type);
        Console::get().drawText(" lbaStart=");
        Console::get().drawNumber((int64_t)entry.lbaStart);
        Console::get().drawText(" sectors=");
        Console::get().drawNumber((int64_t)entry.sectorCount);
        Console::get().drawText("\n");
        return true;
    }

    Console::get().drawText("[MBR] No supported partition found\n");
    return false;
}

// --- GPT ---------------------------------------------------------------------

namespace {

// "Linux filesystem data" partition type GUID (0FC63DAF-8483-4772-8E79-
// 3D69D8477DE4) in on-disk mixed-endian byte order.
const uint8_t kGptLinuxFsGuid[16] = {
    0xAF, 0x3D, 0xC6, 0x0F, 0x83, 0x84, 0x72, 0x47,
    0x8E, 0x79, 0x3D, 0x69, 0xD8, 0x47, 0x7D, 0xE4
};

bool gptSignatureOk(const uint8_t* block) {
    // "EFI PART"
    return block[0] == 'E' && block[1] == 'F' && block[2] == 'I' && block[3] == ' ' &&
           block[4] == 'P' && block[5] == 'A' && block[6] == 'R' && block[7] == 'T';
}

uint32_t rd32le(const uint8_t* p, int off) {
    return (uint32_t)p[off] | ((uint32_t)p[off + 1] << 8) |
           ((uint32_t)p[off + 2] << 16) | ((uint32_t)p[off + 3] << 24);
}

uint64_t rd64le(const uint8_t* p, int off) {
    uint64_t v = 0;
    for (int i = 0; i < 8; i++) {
        v |= (uint64_t)p[off + i] << (8 * i);
    }
    return v;
}

bool guidIsZero(const uint8_t* g) {
    for (int i = 0; i < 16; i++) {
        if (g[i]) return false;
    }
    return true;
}

bool guidEqual(const uint8_t* a, const uint8_t* b) {
    for (int i = 0; i < 16; i++) {
        if (a[i] != b[i]) return false;
    }
    return true;
}

}  // namespace

bool gptIsPresent(BlockDevice* device) {
    if (!device) return false;
    uint8_t block[512];
    if (!device->read((uint64_t)GPT_HEADER_LBA * 512, block, 512)) {
        return false;
    }
    return gptSignatureOk(block);
}

bool gptFindPartition(BlockDevice* device, uint64_t* startByte, uint64_t* lengthByte, bool* isLinux) {
    if (!device) return false;

    uint8_t hdr[512];
    if (!device->read((uint64_t)GPT_HEADER_LBA * 512, hdr, 512)) {
        return false;
    }
    if (!gptSignatureOk(hdr)) {
        Console::get().drawText("[GPT] No GPT header (EFI PART)\n");
        return false;
    }

    const uint64_t entryLba = rd64le(hdr, 0x48);
    uint32_t numEntries = rd32le(hdr, 0x50);
    const uint32_t entrySize = rd32le(hdr, 0x54);
    if (entrySize < 128 || entrySize > 512 || entryLba == 0) {
        Console::get().drawText("[GPT] Invalid entry geometry\n");
        return false;
    }
    if (numEntries > 256) {   // cap the scan against corrupt/huge values
        numEntries = 256;
    }

    // Single pass: remember the first Linux-typed partition (preferred) and the
    // first used partition (fallback).
    bool haveLinux = false, haveAny = false;
    uint64_t linuxStart = 0, linuxLen = 0;
    uint64_t anyStart = 0, anyLen = 0;

    uint8_t entry[512];
    for (uint32_t i = 0; i < numEntries; i++) {
        const uint64_t off = entryLba * 512 + (uint64_t)i * entrySize;
        if (!device->read(off, entry, entrySize)) {
            break;
        }
        if (guidIsZero(entry)) {
            continue;   // unused slot
        }
        const uint64_t firstLba = rd64le(entry, 0x20);
        const uint64_t lastLba = rd64le(entry, 0x28);
        if (lastLba < firstLba) {
            continue;
        }
        const uint64_t s = firstLba * 512;
        const uint64_t len = (lastLba - firstLba + 1) * 512;

        if (!haveAny) {
            anyStart = s;
            anyLen = len;
            haveAny = true;
        }
        if (!haveLinux && guidEqual(entry, kGptLinuxFsGuid)) {
            linuxStart = s;
            linuxLen = len;
            haveLinux = true;
        }
    }

    if (haveLinux) {
        if (startByte) *startByte = linuxStart;
        if (lengthByte) *lengthByte = linuxLen;
        if (isLinux) *isLinux = true;
        return true;
    }
    if (haveAny) {
        if (startByte) *startByte = anyStart;
        if (lengthByte) *lengthByte = anyLen;
        if (isLinux) *isLinux = false;
        return true;
    }

    Console::get().drawText("[GPT] No used partition found\n");
    return false;
}
