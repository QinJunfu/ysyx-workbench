#!/usr/bin/env bash

set -uo pipefail

WORKBENCH_HOME="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
CPU_TESTS_HOME="${WORKBENCH_HOME}/am-kernels/tests/cpu-tests"
LOG_HOME="${CPU_TESTS_HOME}/build/auto-test-logs"

export AM_HOME="${WORKBENCH_HOME}/abstract-machine"
export NEMU_HOME="${WORKBENCH_HOME}/nemu"
export NPC_HOME="${WORKBENCH_HOME}/npc"

declare -a TEST_NAMES=()
declare -a CHILD_PIDS=()
declare -A ARCH_NAMES=(
  [nemu]=riscv32-nemu
  [npc]=riscv32e-npc
)
declare -A BACKEND_PIDS=()
declare -A MAKE_STATUS=()
declare -A TOTAL_COUNT=()
declare -A PASS_COUNT=()
declare -A FAIL_COUNT=()
declare -A XFAIL_COUNT=()
declare -A RESULT_STATUS=()
declare -A FAILURE_DETAILS=()
declare -A LOG_FILES=()

usage() {
  printf 'Usage: bash %s {nemu|npc|all}\n' "${BASH_SOURCE[0]}" >&2
}

run_cpu_tests() {
  local arch="$1"
  local log_file="$2"

  (
    cd "${CPU_TESTS_HOME}" || exit 1
    make "ARCH=${arch}" batch
  ) >"${log_file}" 2>&1
}

start_backend() {
  local backend="$1"
  local arch="${ARCH_NAMES[${backend}]}"

  rm -f "${CPU_TESTS_HOME}/.result.${arch}"
  run_cpu_tests "${arch}" "${LOG_FILES[${backend}]}" &
  BACKEND_PIDS["${backend}"]="$!"
  CHILD_PIDS+=("$!")
}

extract_result_lines() {
  sed -n -E $'s/\x1b\\[[0-9;]*m//g; s/^\\[[[:space:]]*([^]]+)\\][[:space:]]+(PASS|\\*\\*\\*FAIL\\*\\*\\*)$/\\1\\t\\2/p' "$1"
}

completed_test_count() {
  local backend="$1"
  local result_file="${CPU_TESTS_HOME}/.result.${ARCH_NAMES[${backend}]}"

  if [[ -f "${result_file}" ]]; then
    wc -l <"${result_file}"
  else
    extract_result_lines "${LOG_FILES[${backend}]}" | wc -l
  fi
}

show_progress() {
  local -a backends=("$@")
  local backend count pid
  local any_running
  local snapshot=""
  local previous_snapshot=""
  local initialized=0
  declare -A progress=()

  for backend in "${backends[@]}"; do
    progress["${backend}"]=0
  done

  while :; do
    any_running=0
    snapshot=""

    for backend in "${backends[@]}"; do
      pid="${BACKEND_PIDS[${backend}]}"
      if kill -0 "${pid}" 2>/dev/null; then
        any_running=1
      fi

      count="$(completed_test_count "${backend}")"
      if ((count > progress[${backend}])); then
        progress["${backend}"]="${count}"
      fi
      snapshot+="${backend}=${progress[${backend}]};"
    done

    if [[ -t 1 && "${snapshot}" != "${previous_snapshot}" ]]; then
      if ((initialized == 1)); then
        printf '\033[%dA' "${#backends[@]}"
      fi
      for backend in "${backends[@]}"; do
        printf '\r%-24s\033[K\n' "${backend} ${progress[${backend}]}/${#TEST_NAMES[@]}"
      done
      initialized=1
      previous_snapshot="${snapshot}"
    fi

    ((any_running == 1)) || break
    sleep 0.1
  done
}

wait_for_backend() {
  local backend="$1"

  if wait "${BACKEND_PIDS[${backend}]}"; then
    MAKE_STATUS["${backend}"]=0
  else
    MAKE_STATUS["${backend}"]=$?
  fi
}

stop_children() {
  local pid

  trap - INT TERM
  for pid in "${CHILD_PIDS[@]}"; do
    kill "${pid}" 2>/dev/null || true
  done
  wait 2>/dev/null || true
  exit 130
}

collect_result() {
  local backend="$1"
  local log_file="${LOG_FILES[${backend}]}"
  local test_name outcome
  local pass=0
  local fail=0
  local xfail=0
  local missing=0
  local details=""
  local make_status="${MAKE_STATUS[${backend}]}"
  declare -A seen=()

  while IFS=$'\t' read -r test_name outcome; do
    [[ -n "${test_name}" ]] || continue
    seen["${test_name}"]=1

    if [[ "${test_name}" == wrong ]]; then
      if [[ "${outcome}" == "***FAIL***" ]]; then
        ((xfail += 1))
      else
        ((fail += 1))
        details+="wrong (unexpected PASS), "
      fi
    elif [[ "${outcome}" == PASS ]]; then
      ((pass += 1))
    else
      ((fail += 1))
      details+="${test_name}, "
    fi
  done < <(extract_result_lines "${log_file}")

  for test_name in "${TEST_NAMES[@]}"; do
    if [[ -z "${seen[${test_name}]+set}" ]]; then
      ((missing += 1))
      ((fail += 1))
      details+="${test_name} (missing), "
    fi
  done

  TOTAL_COUNT["${backend}"]="${#TEST_NAMES[@]}"
  PASS_COUNT["${backend}"]="${pass}"
  FAIL_COUNT["${backend}"]="${fail}"
  XFAIL_COUNT["${backend}"]="${xfail}"
  FAILURE_DETAILS["${backend}"]="${details%, }"

  if ((fail == 0 && xfail == 1 && make_status == 0)); then
    RESULT_STATUS["${backend}"]=PASS
    rm -f "${log_file}"
  elif ((missing > 0 || (make_status != 0 && fail == 0))); then
    RESULT_STATUS["${backend}"]=ERROR
  else
    RESULT_STATUS["${backend}"]=FAIL
  fi
}

print_summary() {
  local backend
  local total=0
  local pass=0
  local fail=0
  local xfail=0
  local overall=PASS

  printf '%-8s %5s %7s %7s %7s %8s\n' Backend Total Passed Failed XFail Result
  for backend in "$@"; do
    printf '%-8s %5d %7d %7d %7d %8s\n' \
      "${backend}" \
      "${TOTAL_COUNT[${backend}]}" \
      "${PASS_COUNT[${backend}]}" \
      "${FAIL_COUNT[${backend}]}" \
      "${XFAIL_COUNT[${backend}]}" \
      "${RESULT_STATUS[${backend}]}"

    ((total += TOTAL_COUNT[${backend}]))
    ((pass += PASS_COUNT[${backend}]))
    ((fail += FAIL_COUNT[${backend}]))
    ((xfail += XFAIL_COUNT[${backend}]))
    if [[ "${RESULT_STATUS[${backend}]}" != PASS ]]; then
      overall=FAIL
    fi
  done

  printf '\nOverall: %s (%d total, %d passed, %d failed, %d expected failures)\n' \
    "${overall}" "${total}" "${pass}" "${fail}" "${xfail}"

  if [[ "${overall}" == FAIL ]]; then
    for backend in "$@"; do
      if [[ "${RESULT_STATUS[${backend}]}" != PASS ]]; then
        if [[ -n "${FAILURE_DETAILS[${backend}]}" ]]; then
          printf '%s failures: %s\n' "${backend}" "${FAILURE_DETAILS[${backend}]}"
        fi
        printf '%s log: %s\n' "${backend}" "${LOG_FILES[${backend}]}"
      fi
    done
    return 1
  fi

  rmdir "${LOG_HOME}" 2>/dev/null || true
}

if [[ ! -f "${CPU_TESTS_HOME}/Makefile" ]]; then
  printf 'Error: cpu-tests directory not found: %s\n' "${CPU_TESTS_HOME}" >&2
  exit 1
fi

if [[ $# -ne 1 ]]; then
  usage
  exit 2
fi

for test_file in "${CPU_TESTS_HOME}"/tests/*.c; do
  TEST_NAMES+=("$(basename "${test_file}" .c)")
done

mkdir -p "${LOG_HOME}"
LOG_FILES[nemu]="${LOG_HOME}/nemu.log"
LOG_FILES[npc]="${LOG_HOME}/npc.log"
trap stop_children INT TERM

case "$1" in
  nemu)
    start_backend nemu
    show_progress nemu
    wait_for_backend nemu
    CHILD_PIDS=()
    collect_result nemu
    print_summary nemu
    ;;
  npc)
    start_backend npc
    show_progress npc
    wait_for_backend npc
    CHILD_PIDS=()
    collect_result npc
    print_summary npc
    ;;
  all)
    start_backend nemu
    start_backend npc
    show_progress nemu npc
    wait_for_backend nemu
    wait_for_backend npc
    CHILD_PIDS=()

    collect_result nemu
    collect_result npc
    print_summary nemu npc
    ;;
  *)
    printf 'Error: unknown test target: %s\n' "$1" >&2
    usage
    exit 2
    ;;
esac
