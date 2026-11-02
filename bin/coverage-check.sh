#!/usr/bin/env bash
# Coverage gate - Windows/Git entry point. Delegates to WSL Ubuntu where the
# gcc/gcovr/coverage toolchain lives. Block commit if C++ or Python coverage
# < 95% (see bin/coverage-check-wsl.sh).
#
# Escape hatch (documented, not silent): C104_SKIP_COVERAGE=1 bypasses the gate
#   for environments without the WSL toolchain.
#
# Usage: this script is invoked by pre-commit as a local hook.
set -u

if [ "${C104_SKIP_COVERAGE:-0}" = "1" ]; then
  echo "[coverage-gate] skipped (C104_SKIP_COVERAGE=1)"
  exit 0
fi

# Locate the repo root (the pre-commit hook runs from the repo root normally)
REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

# Convert a Windows path (C:\..., C:/... or Git Bash style /c/...) into
# /mnt/c/... for WSL
win() {
  echo "$1" | sed -E -e 's|\\|/|g' \
    -e 's|^([A-Za-z]):|/mnt/\L\1|' \
    -e 's|^/([A-Za-z])/|/mnt/\L\1/|'
}
WSL_REPO="$(win "$REPO")"

if command -v wsl >/dev/null 2>&1; then
  # forward C104_* settings (thresholds, partial skips) into WSL
  export WSLENV="${WSLENV:+$WSLENV:}C104_COVERAGE_THRESHOLD:C104_SKIP_CPP_COVERAGE:C104_SKIP_PY_COVERAGE"
  # shellcheck disable=SC2016
  MSYS_NO_PATHCONV=1 wsl -d Ubuntu -- bash -lc "bash '$WSL_REPO/bin/coverage-check-wsl.sh' '$WSL_REPO'"
  rc=$?
else
  echo "[coverage-gate] ERROR: WSL not available; cannot verify coverage."
  echo "Set C104_SKIP_COVERAGE=1 only if you accept SKIPPING the coverage gate."
  rc=1
fi

exit $rc
