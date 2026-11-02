#!/bin/bash
# Coverage gate - WSL side. Block commit if either required coverage < 95%.
#
#   python3 bin/coverage-check-wsl.sh <repo_path>
#
# Measures:
#   (1) C++  : gcovr over src/*.cpp from the coverage-instrumented c104_tests
#   (2) Python: coverage.py over c104/*.py (library installed via pip install $PWD)
#
# Fails (exit 1) if EITHER is below ${THRESHOLD} (default 95%).
set -u

REPO="${1:?usage: coverage-check-wsl.sh <repo_path>}"
THRESHOLD="${C104_COVERAGE_THRESHOLD:-95.0}"
export PATH="$HOME/.local/bin:$PATH"

cd "$REPO" || { echo "ERROR: cannot cd to $REPO"; exit 1; }

echo "== c104 coverage gate (>= ${THRESHOLD}%) =="
echo "repo: $REPO"

# --------------------------------------------------------------------------
# C++ coverage via gcovr (coverage-instrumented c104_tests)
# --------------------------------------------------------------------------
BUILD_DIR="${REPO}/build-coverage"
pass_cpp=0
if [ "${C104_SKIP_CPP_COVERAGE:-0}" = "1" ]; then
  echo "[skip] C++ coverage disabled (C104_SKIP_CPP_COVERAGE=1)"
  pass_cpp=1
else
  if command -v gcovr >/dev/null 2>&1 && command -v gcc >/dev/null 2>&1; then
    if [ ! -f "${BUILD_DIR}/CMakeCache.txt" ]; then
      echo ".. configuring coverage build (first run may take a while)"
      mkdir -p "$BUILD_DIR"
      (cd "$BUILD_DIR" && cmake -DCMAKE_BUILD_TYPE=Debug \
        -DCMAKE_CXX_FLAGS="--coverage -fno-inline -O0 -g" \
        -DCMAKE_C_FLAGS="--coverage -fno-inline -O0 -g" \
        -DCMAKE_EXE_LINKER_FLAGS="--coverage" \
        -DCMAKE_SHARED_LINKER_FLAGS="--coverage" "$REPO") \
        >/dev/null || { echo ".. cmake configure failed"; }
    fi
    echo ".. building c104_tests (coverage)"
    cmake --build "$BUILD_DIR" --target c104_tests -j "$(nproc)" >/dev/null 2>&1
    # drop stale counters and reports from previous runs
    find "$BUILD_DIR" -name '*.gcda' -delete
    rm -f /tmp/cov_gate.txt
    if [ -x "$BUILD_DIR/c104_tests" ]; then
      ( cd "$BUILD_DIR" && PYTHONUNBUFFERED=1 ./c104_tests >/tmp/cov_gate_test.log 2>&1 ) \
        || echo ".. c104_tests reported failures (see /tmp/cov_gate_test.log)"
    else
      echo ".. c104_tests binary missing after build"
    fi
    # negative_hits: non-atomic gcov counters race in multithreaded code
    python3 -m gcovr "$BUILD_DIR" --root "$REPO" --gcov-ignore-errors=all \
      --gcov-ignore-parse-errors=negative_hits.warn_once_per_file \
      --filter 'src/.*\.cpp' --txt /tmp/cov_gate.txt >/tmp/cov_gate_gcovr.log 2>&1 \
      || { echo ".. gcovr failed:"; tail -5 /tmp/cov_gate_gcovr.log; }
    if [ -f /tmp/cov_gate.txt ]; then
      cpp_pct=$(grep -E '^src/.*\.cpp' /tmp/cov_gate.txt \
        | awk '{l+=$2; e+=$3} END {printf "%.2f", (l? (100*e/l) : 0)}')
    else
      cpp_pct=0
    fi
    echo "C++ (src/*.cpp) coverage: ${cpp_pct}%"
    if awk "BEGIN{exit !($cpp_pct >= $THRESHOLD)}"; then
      pass_cpp=1
    else
      echo "!! C++ coverage ${cpp_pct}% is below ${THRESHOLD}%"
    fi
  else
    echo "[warn] gcc/gcovr not available in this environment"
    pass_cpp=0
    echo "!! cannot verify C++ coverage (toolchain missing)"
  fi
fi

# --------------------------------------------------------------------------
# Python coverage via coverage.py on installed c104 (cwd must NOT be the repo
# so the installed package is imported, not the in-tree source dir)
# --------------------------------------------------------------------------
pass_py=0
if [ "${C104_SKIP_PY_COVERAGE:-0}" = "1" ]; then
  echo "[skip] Python coverage disabled (C104_SKIP_PY_COVERAGE=1)"
  pass_py=1
else
  if command -v python3 >/dev/null 2>&1 && python3 -c "import coverage" >/dev/null 2>&1; then
    TMP=$(mktemp -d)
    ( cd "$TMP" && \
      python3 -m coverage run --source c104 -m unittest discover \
        -s "$REPO/tests" -p 'test_c104.py' >/tmp/cov_gate_py.log 2>&1 && \
      python3 -m coverage report > /tmp/cov_gate_py_rpt.txt 2>&1 )
    py_pct=$(grep -E '^TOTAL' /tmp/cov_gate_py_rpt.txt 2>/dev/null | awk '{print $NF}')
    # strip trailing % ; fallback
    py_val=$(echo "$py_pct" | tr -d '%')
    echo "Python (c104/*.py) coverage: ${py_pct:-0%}"
    if [ -z "$py_val" ]; then py_val=0; fi
    if awk "BEGIN{exit !($py_val >= $THRESHOLD)}"; then
      pass_py=1
    else
      echo "!! Python coverage ${py_pct}% is below ${THRESHOLD}%"
    fi
    rm -rf "$TMP"
  else
    echo "[warn] python3/coverage not available in this environment"
    pass_py=0
    echo "!! cannot verify Python coverage (toolchain missing)"
  fi
fi

echo "------------------------------------------------------------"
if [ "$pass_cpp" = "1" ] && [ "$pass_py" = "1" ]; then
  echo "COVERAGE GATE PASSED (>= ${THRESHOLD}% on C++ and Python)"
  exit 0
else
  echo "COVERAGE GATE FAILED (C++ pass=$pass_cpp, Python pass=$pass_py)"
  echo "Commit blocked: coverage is below required threshold."
  exit 1
fi
