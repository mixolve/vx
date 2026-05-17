#!/bin/sh

set -eu

export NO_COLOR=1
export CLICOLOR=0
export CLICOLOR_FORCE=0
export CMAKE_COLOR_DIAGNOSTICS=OFF
export CMAKE_BUILD_PARALLEL_LEVEL=4
export TERM=dumb

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

build_dir="$repo_root/temp/build/ios-simulator"
developer_dir=$(xcode-select -p)
simulator_sdk_root="$developer_dir/Platforms/iPhoneSimulator.platform/Developer/SDKs/iPhoneSimulator.sdk"

if [ -d "$build_dir" ] && {
	[ ! -f "$build_dir/CMakeCache.txt" ] || ! grep -q "^CMAKE_HOME_DIRECTORY:INTERNAL=$repo_root/temp$" "$build_dir/CMakeCache.txt"
}; then
	rm -rf "$build_dir"
fi

cmake -S temp -B "$build_dir" -G Xcode -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_SYSROOT="$simulator_sdk_root" && cmake --build "$build_dir" --config Debug --target Vx_AUv3 --parallel 4 && cmake --build "$build_dir" --config Debug --target Vx_iOSHost --parallel 4
