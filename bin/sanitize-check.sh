#!/usr/bin/env bash
# Sanitizer gate - Windows/Git entry point. Delegates to WSL Ubuntu where the
# clang-14 toolchain lives. Builds c104_tests under ASan, UBSan and TSan and
# runs the full Catch2 suite under each (see bin/sanitize-check-wsl.sh).
#
# Escape hatch (documented, not silent): C104_SKIP_SANITIZERS=1 bypasses the
#   gate for environments without the WSL toolchain.
#
# Usage:
#   bin/sanitize-check.sh [all|asan|ubsan|tsan]
set -u

if [ "${C104_SKIP_SANITIZERS:-0}" = "1" ]; then
  echo "[sanitize-gate] skipped (C104_SKIP_SANITIZERS=1)"
  exit 0
fi

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

# Convert a Windows path (C:\..., C:/... or Git Bash style /c/...) into
# /mnt/c/... for WSL
win() {
  echo "$1" | sed -E -e 's|\\|/|g' \
    -e 's|^([A-Za-z]):|/mnt/\L\1|' \
    -e 's|^/([A-Za-z])/|/mnt/\L\1/|'
}
WSL_REPO="$(win "$REPO")"

SELECT="${1:-all}"

if command -v wsl >/dev/null 2>&1; then
  # forward C104_* settings (compiler override, leak detection) into WSL
  export WSLENV="${WSLENV:+$WSLENV:}C104_SANIT_CC:C104_SANIT_CXX:C104_ASAN_LEAKS"
  # shellcheck disable=SC2016
  MSYS_NO_PATHCONV=1 wsl -d Ubuntu -- bash -lc "bash '$WSL_REPO/bin/sanitize-check-wsl.sh' '$WSL_REPO' '$SELECT'" 2>&1 | tail -40
  rc=${PIPESTATUS[0]}
else
  echo "[sanitize-gate] ERROR: WSL not available; cannot run sanitizers."
  echo "Set C104_SKIP_SANITIZERS=1 only if you accept SKIPPING the sanitizer gate."
  rc=1
fi

exit "$rc"
