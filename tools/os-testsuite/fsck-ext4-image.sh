#!/usr/bin/env bash
# P1.4 host e2fsck oracle for the kernel-written ext4 disk.
#
# After booting the os-testsuite ISO under QEMU with build/os-testsuite/ext4.img
# attached as the AHCI disk (the kernel mounts it as the writable ext4 root and
# the runner exercises it), this script verifies the on-disk image is still
# consistent from the host's point of view -- the Phase 1.4 exit oracle.
#
# The ext4 filesystem lives in an MBR partition at 1 MiB (LBA 2048); we extract
# that partition (unprivileged, no loop device) and run e2fsck.
#
# Usage:  fsck-ext4-image.sh [image] [-y]
#   image  path to the disk image (default: build/os-testsuite/ext4.img)
#   -y     use `e2fsck -fy` (replay journal + auto-fix) instead of `-fn`
#          (read-only strict). Use -y for the power-cut/journal-replay test.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
IMG="${1:-$ROOT/build/os-testsuite/ext4.img}"
MODE="-fn"
if [ "${2:-}" = "-y" ] || [ "${1:-}" = "-y" ]; then
  MODE="-fy"
  [ "${1:-}" = "-y" ] && IMG="$ROOT/build/os-testsuite/ext4.img"
fi

if [ ! -f "$IMG" ]; then
  echo "FSCK-ORACLE: image not found: $IMG" >&2
  exit 2
fi

PART="$(mktemp --suffix=.ext4part)"
trap 'rm -f "$PART"' EXIT
# Partition starts at 1 MiB (sfdisk start=2048 sectors * 512).
dd if="$IMG" of="$PART" bs=1048576 skip=1 status=none

echo "FSCK-ORACLE: e2fsck $MODE on $(basename "$IMG") partition"
if e2fsck $MODE "$PART"; then
  echo "FSCK-ORACLE: CLEAN"
else
  rc=$?
  echo "FSCK-ORACLE: e2fsck reported problems (exit $rc)" >&2
  exit "$rc"
fi
