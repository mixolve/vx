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
temp_dir="$script_dir"

trap 'vx_command_exit' EXIT

source_bundle="$temp_dir/build/macos-au/Vx_artefacts/Debug/AU/vx.component"

if [ ! -d "$source_bundle" ]; then
	echo "Missing macOS AU bundle: $source_bundle" >&2
	exit 1
fi

vx_install_bundle() {
	destination_dir="$1"
	destination_bundle="$destination_dir/vx.component"

	echo "Installing macOS AU to $destination_bundle"

	if ! vx_run_clean mkdir -p "$destination_dir"; then
		echo "Skipping macOS AU install, cannot create $destination_dir" >&2
		return 0
	fi

	if ! vx_run_clean rm -rf "$destination_bundle"; then
		echo "Skipping macOS AU install, cannot replace $destination_bundle" >&2
		return 0
	fi

	if ! vx_run_clean ditto "$source_bundle" "$destination_bundle"; then
		echo "Skipping macOS AU install, cannot copy to $destination_bundle" >&2
		return 0
	fi
}

vx_install_bundle "/Library/Audio/Plug-Ins/Components"
vx_install_bundle "$HOME/Library/Audio/Plug-Ins/Components"
