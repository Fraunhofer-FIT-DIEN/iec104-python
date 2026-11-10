#!/bin/bash
# Sanitizer gate - WSL side. Builds c104_tests under ASan, UBSan and TSan and
# runs the full Catch2 suite under each, exiting non-zero when any selected
# sanitizer reports an error or the suite fails.
#
#   bash bin/sanitize-check-wsl.sh <repo_path> [all|asan|ubsan|tsan]
#
# Configuration (environment):
#   C104_SANIT_CC     C compiler (default clang-18, fallback gcc)
#   C104_SANIT_CXX    C++ compiler (default clang++-18, fallback g++)
#   C104_ASAN_LEAKS   1 to enable leak detection (default 0: embedded CPython
#                     is not leak-free at exit, so LSan reports interpreter
#                     internals, not just project leaks)
set -u

REPO="${1:?usage: sanitize-check-wsl.sh <repo_path> [all|asan|ubsan|tsan]}"
SELECT="${2:-all}"
if [ "${SELECT}" = "all" ]; then
  SELECT="asan,ubsan,tsan"
fi

CC_BIN="${C104_SANIT_CC:-$(command -v clang-18 || echo gcc)}"
CXX_BIN="${C104_SANIT_CXX:-$(command -v clang++-18 || echo g++)}"
ASAN_LEAKS="${C104_ASAN_LEAKS:-0}"

cd "$REPO" || { echo "ERROR: cannot cd to $REPO"; exit 1; }

fail_count=0

run_sanitizer() {
  local name="$1"
  local sanit_flags="$2"
  local sanit_env="$3"
  local build_dir="${REPO}/cmake-build-${name}"

  echo "======================================================================"
  echo "== [$name] configuring $build_dir"
  echo "======================================================================"
  mkdir -p "$build_dir"
  # The post-build catch_discover_tests step runs the instrumented binary;
  # sanitizer options must already be non-halting in the environment so that
  # step cannot abort the build (see the export before cmake --build below).
  case "$name" in
    asan)
      export ASAN_OPTIONS="detect_leaks=$ASAN_LEAKS"
      export UBSAN_OPTIONS="print_stacktrace=1"
      ;;
    ubsan)
      export UBSAN_OPTIONS="print_stacktrace=1"
      ;;
    tsan)
      export TSAN_OPTIONS=""
      ;;
  esac
  if ! cmake -S "$REPO" -B "$build_dir" -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_C_COMPILER="$CC_BIN" -DCMAKE_CXX_COMPILER="$CXX_BIN" \
    -DCMAKE_C_FLAGS="$sanit_flags" -DCMAKE_CXX_FLAGS="$sanit_flags" \
    -DCMAKE_EXE_LINKER_FLAGS="$sanit_flags" \
    -DPython_EXECUTABLE="$(command -v python3)" >"$build_dir/configure.log" 2>&1; then
    echo "!! [$name] cmake configure failed (see $build_dir/configure.log)"
    return 1
  fi

  local test_log="$build_dir/sanitize-$name.log"
  echo "== [$name] running full c104_tests suite"
  cmake --build "$build_dir" --target c104_tests -j "$(nproc)" \
    >"$build_dir/build.log" 2>&1 || {
    echo "!! [$name] build failed (see $build_dir/build.log)"
    return 1
  }
  (
    cd "$build_dir" || exit 1
    bash "$REPO/bin/gen-certs.sh" >"$build_dir/gencerts.log" 2>&1
    openssl pkcs8 -topk8 -in "$REPO/tests/certs/server1.key" \
      -passout pass:test-pass -out "$REPO/tests/certs/server1_encrypted.key" \
      >>"$build_dir/gencerts.log" 2>&1 || true
    # shellcheck disable=SC2086
    env $sanit_env PYTHONUNBUFFERED=1 ./c104_tests >"$test_log" 2>&1
  )
  local suite_rc=$?

  local summary
  summary=$(grep -E "All tests passed|test cases:|assertions:" "$test_log" | tail -1)
  echo "== [$name] suite rc=$suite_rc  $summary"

  local report
  report=$(grep -E "ERROR: (AddressSanitizer|LeakSanitizer|ThreadSanitizer|UndefinedBehaviorSanitizer)|runtime error:|WARNING: ThreadSanitizer|SUMMARY: (AddressSanitizer|LeakSanitizer|ThreadSanitizer|UndefinedBehaviorSanitizer)" "$test_log" | head -10)
  if [ -n "$report" ]; then
    echo "!! [$name] sanitizer reports:"
    echo "$report"
  fi

  if [ "$suite_rc" -ne 0 ] || [ -n "$report" ]; then
    echo "!! [$name] FAILED (full log: $test_log)"
    return 1
  fi
  echo "== [$name] PASSED"
  return 0
}

IFS=',' read -ra chosen <<<"$SELECT"
for san in "${chosen[@]}"; do
  case "$san" in
    asan)
      if ! run_sanitizer asan \
        "-fsanitize=address -fno-omit-frame-pointer -fno-sanitize-recover=all -O1" \
        "ASAN_OPTIONS=detect_leaks=$ASAN_LEAKS:halt_on_error=1:abort_on_error=1"; then
        fail_count=$((fail_count + 1))
      fi
      ;;
    ubsan)
      if ! run_sanitizer ubsan \
        "-fsanitize=undefined -fno-sanitize-recover=all -O1" \
        "UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1"; then
        fail_count=$((fail_count + 1))
      fi
      ;;
    tsan)
      if ! run_sanitizer tsan \
        "-fsanitize=thread -fno-omit-frame-pointer -O1" \
        "TSAN_OPTIONS=halt_on_error=1"; then
        fail_count=$((fail_count + 1))
      fi
      ;;
    *)
      echo "!! unknown sanitizer '$san' (choose asan, ubsan, tsan, or all)"
      fail_count=$((fail_count + 1))
      ;;
  esac
done

echo "======================================================================"
if [ "$fail_count" -eq 0 ]; then
  echo "SANITIZER GATE PASSED ($SELECT)"
  exit 0
else
  echo "SANITIZER GATE FAILED ($fail_count sanitizer(s) failed)"
  exit 1
fi
