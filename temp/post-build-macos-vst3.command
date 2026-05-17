#!/bin/sh

set -eu

export NO_COLOR=1
export CLICOLOR=0
export CLICOLOR_FORCE=0
export CMAKE_COLOR_DIAGNOSTICS=OFF
export CMAKE_BUILD_PARALLEL_LEVEL=4
export TERM=dumb

vx_strip_ansi() {
	sed "$(printf 's/\033\\[[0-9;?]*[ -/]*[@-~]//g')"
}

vx_run_clean() {
	status_file=$(mktemp)
	(
		set +e
		"$@"
		printf '%s' "$?" > "$status_file"
	) 2>&1 | vx_strip_ansi
	status=$(cat "$status_file" 2>/dev/null || printf '1')
	rm -f "$status_file"
	return "$status"
}

vx_command_exit() {
	status=$?

	trap - EXIT
	set +e

	if [ "$status" -eq 0 ]; then
		printf '\033[32mDONE\033[0m\n'
	else
		printf '\033[31mUNDONE\033[0m\n'
	fi

	if [ -t 0 ] && [ "${VX_COMMAND_KEEP_OUTPUT:-1}" != "0" ]; then
		"${SHELL:-/bin/zsh}" -i
	fi

	exit "$status"
}

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_root=$(CDPATH= cd -- "$script_dir/.." && pwd)

cd "$repo_root"

trap 'vx_command_exit' EXIT

source_bundle="$repo_root/temp/build/macos-vst3/Vx_artefacts/Debug/VST3/vx.vst3"
destination_dir="/Library/Audio/Plug-Ins/ALL"
destination_bundle="$destination_dir/vx.vst3"

if [ ! -d "$source_bundle" ]; then
	echo "Missing macOS VST3 bundle: $source_bundle" >&2
	exit 1
fi

echo "Installing macOS VST3 to $destination_bundle"
vx_run_clean sudo mkdir -p "$destination_dir"
vx_run_clean sudo rm -rf "$destination_bundle"
vx_run_clean sudo ditto "$source_bundle" "$destination_bundle"
