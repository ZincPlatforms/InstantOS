#pragma once

// In-OS ext4 self-test. Runs at boot (from kernel main, after the ext4 root is
// mounted) and exercises the driver through the real VFS -> ext4 -> AHCI path:
// reads a seeded file, then creates/writes/reads-back/stats/unlinks files and a
// directory (which also drives write-side journaling on a simple-journal
// volume). Results are printed to the serial console. It cleans up the files it
// creates, so a completed run leaves the root filesystem as it found it.
void ext4RunSelfTest();
