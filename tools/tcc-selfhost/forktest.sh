#!/bin/bash
# Bash fork/exec stress test for InstantOS. Exercises every path where GNU bash
# calls the kernel to create a new process -- the exact path that used to fault
# with an int3 storm (new-process FPUState allocation returning heap poison).
#
# Only bash builtins + /bin/tcc + /bin/bash are available (no coreutils). Each
# phase prints a sentinel with a completion count so a mid-phase fault is visible
# as a missing/short count. /bin/tcc -v is a cheap external exec (prints version).
set +e

echo "BASHFORK_START"

# --- Phase 1: repeated external exec (fork + execve of /bin/tcc) -------------
i=0
while [ $i -lt 5 ]; do
  /bin/tcc -v >/dev/null 2>&1
  i=$((i+1))
done
echo "BASHFORK_EXEC_DONE=$i"

# --- Phase 2: subshell forks (fork with no exec) ----------------------------
i=0
while [ $i -lt 10 ]; do
  ( : )
  i=$((i+1))
done
echo "BASHFORK_SUBSHELL_DONE=$i"

# --- Phase 3: pipelines (both stages fork) ----------------------------------
i=0
while [ $i -lt 4 ]; do
  /bin/tcc -v 2>&1 | /bin/tcc -v >/dev/null 2>&1
  i=$((i+1))
done
echo "BASHFORK_PIPE_DONE=$i"

# --- Phase 4: nested bash -c (fork + execve of bash itself) ------------------
bad=0
i=0
while [ $i -lt 5 ]; do
  /bin/bash -c 'exit 7'
  [ $? -eq 7 ] || bad=$((bad+1))
  i=$((i+1))
done
echo "BASHFORK_NESTED_DONE=$i BAD=$bad"

# --- Phase 5: background jobs + wait (concurrent process creation) -----------
i=0
while [ $i -lt 5 ]; do
  /bin/tcc -v >/dev/null 2>&1 &
  i=$((i+1))
done
wait
echo "BASHFORK_BG_DONE=$i"

# --- Phase 6: command substitution (fork + capture) -------------------------
i=0
sum=0
while [ $i -lt 5 ]; do
  v="$(/bin/tcc -v 2>&1)"
  [ -n "$v" ] && sum=$((sum+1))
  i=$((i+1))
done
echo "BASHFORK_SUBST_DONE=$i NONEMPTY=$sum"

echo "BASHFORK_ALL_OK"
exit 0
