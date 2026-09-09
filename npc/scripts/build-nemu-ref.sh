#!/usr/bin/env bash

set -euo pipefail

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
npc_dir=$(CDPATH= cd -- "$script_dir/.." && pwd)
nemu_dir=$(CDPATH= cd -- "$npc_dir/../nemu" && pwd)
backup_dir=$(mktemp -d "${TMPDIR:-/tmp}/npc-nemu-ref.XXXXXX")
ref_binary="$nemu_dir/build/riscv32-nemu-interpreter-so"
ref_obj_dir="$nemu_dir/build/obj-riscv32-nemu-interpreter-so"
ref_defconfig=riscv32-npc-ref_defconfig

if grep -q '^CONFIG_NPC_PLATFORM_YSYXSOC=y$' "$npc_dir/.config"; then
  if grep -q '^CONFIG_NPC_YSYXSOC_SDRAM_32MB=y$' "$npc_dir/.config"; then
    ref_defconfig=riscv32-ysyxsoc-ref32_defconfig
  elif grep -q '^CONFIG_NPC_YSYXSOC_SDRAM_64MB=y$' "$npc_dir/.config"; then
    ref_defconfig=riscv32-ysyxsoc-ref64_defconfig
  else
    ref_defconfig=riscv32-ysyxsoc-ref128_defconfig
  fi
fi

config_present=0
config_old_present=0
include_config_present=0
include_generated_present=0

if [[ -e "$nemu_dir/.config" ]]; then
  cp -a "$nemu_dir/.config" "$backup_dir/config"
  config_present=1
fi
if [[ -e "$nemu_dir/.config.old" ]]; then
  cp -a "$nemu_dir/.config.old" "$backup_dir/config.old"
  config_old_present=1
fi
if [[ -e "$nemu_dir/include/config" ]]; then
  cp -a "$nemu_dir/include/config" "$backup_dir/include-config"
  include_config_present=1
fi
if [[ -e "$nemu_dir/include/generated" ]]; then
  cp -a "$nemu_dir/include/generated" "$backup_dir/include-generated"
  include_generated_present=1
fi

restore_path() {
  local backup_path=$1
  local target_path=$2
  local was_present=$3

  rm -rf "$target_path"
  if [[ $was_present -eq 1 ]]; then
    cp -a "$backup_path" "$target_path"
  fi
}

restore_config() {
  local status=$?

  restore_path "$backup_dir/config" "$nemu_dir/.config" "$config_present"
  restore_path "$backup_dir/config.old" "$nemu_dir/.config.old" "$config_old_present"
  restore_path "$backup_dir/include-config" "$nemu_dir/include/config" "$include_config_present"
  restore_path "$backup_dir/include-generated" "$nemu_dir/include/generated" "$include_generated_present"
  rm -rf "$backup_dir"
  exit "$status"
}

trap restore_config EXIT
trap 'exit 130' INT
trap 'exit 143' TERM HUP

make -C "$nemu_dir" NEMU_HOME="$nemu_dir" "$ref_defconfig"
# A changed file list is not a Make prerequisite of the old shared object.
# Remove only this configuration's outputs so an older link cannot be reused.
rm -f -- "$ref_binary"
rm -rf -- "$ref_obj_dir"
make -C "$nemu_dir" NEMU_HOME="$nemu_dir" app

printf 'NEMU reference: %s\n' "$ref_binary"
