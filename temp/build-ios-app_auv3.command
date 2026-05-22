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
temp_dir="$script_dir"
project_root=$(CDPATH= cd -- "$temp_dir/.." && pwd)

trap 'vx_command_exit' EXIT

resolve_juce_dir() {
	if [ -n "${JUCE_DIR:-}" ]; then
		printf '%s\n' "$JUCE_DIR"
		return 0
	fi

	if [ -d "$project_root/JUCE" ]; then
		printf '%s\n' "$project_root/JUCE"
		return 0
	fi

	if [ -d "$project_root/../JUCE" ]; then
		printf '%s\n' "$project_root/../JUCE"
		return 0
	fi

	return 1
}

sdk_path=$(xcrun --sdk iphoneos --show-sdk-path)
build_dir="$temp_dir/build/ios-app_auv3"
cache_source_dir="CMAKE_HOME_DIRECTORY:INTERNAL=$temp_dir"
juce_subbuild_dir="$temp_dir/_deps/juce-subbuild"
juce_tools_dir="$temp_dir/_deps/juce-build/tools"
use_local_juce=0
juce_dir=""

if juce_dir=$(resolve_juce_dir); then
	if [ -d "$juce_dir/modules" ]; then
		use_local_juce=1
	else
		echo "Ignoring invalid JUCE directory: $juce_dir" >&2
	fi
fi

if [ -d "$build_dir" ] && {
	[ ! -f "$build_dir/CMakeCache.txt" ] || ! grep -Fqx "$cache_source_dir" "$build_dir/CMakeCache.txt" || ! grep -Fqx "CMAKE_GENERATOR:INTERNAL=Xcode" "$build_dir/CMakeCache.txt" || ! grep -Fqx "CMAKE_SYSTEM_NAME:UNINITIALIZED=iOS" "$build_dir/CMakeCache.txt" || ! grep -Fqx "MIXOLVE_VX_USE_LOCAL_JUCE:BOOL=$( [ "$use_local_juce" -eq 1 ] && printf ON || printf OFF )" "$build_dir/CMakeCache.txt"
}; then
	echo "Removing stale iOS build directory: $build_dir"
	rm -rf "$build_dir"
fi

if [ "$use_local_juce" -eq 1 ] && [ -f "$build_dir/CMakeCache.txt" ]; then
	cached_juce_dir=$(grep '^JUCE_DIR:' "$build_dir/CMakeCache.txt" | cut -d= -f2- || true)
	if [ -n "$cached_juce_dir" ] && [ "$cached_juce_dir" != "$juce_dir" ]; then
		echo "Removing stale iOS build directory with mismatched JUCE_DIR: $build_dir"
		rm -rf "$build_dir"
	fi
fi

if [ -f "$juce_subbuild_dir/CMakeCache.txt" ]; then
	cached_home=$(grep '^CMAKE_HOME_DIRECTORY:INTERNAL=' "$juce_subbuild_dir/CMakeCache.txt" | cut -d= -f2- || true)
	if [ -n "$cached_home" ] && [ "$cached_home" != "$juce_subbuild_dir" ]; then
		echo "Removing stale JUCE cache in $temp_dir/_deps"
		rm -rf "$temp_dir/_deps/juce-build" "$juce_subbuild_dir"
	fi
fi

if [ -f "$juce_tools_dir/CMakeCache.txt" ] && ! grep -Fqx "CMAKE_GENERATOR:INTERNAL=Xcode" "$juce_tools_dir/CMakeCache.txt"; then
	rm -rf "$juce_tools_dir"
fi

if [ -f "$juce_tools_dir/CMakeCache.txt" ] && grep -Eq 'CMAKE_OSX_SYSROOT:.*iPhone|CMAKE_SYSTEM_NAME:.*iOS' "$juce_tools_dir/CMakeCache.txt"; then
	echo "Removing stale JUCE helper tools built for iOS: $juce_tools_dir"
	rm -rf "$juce_tools_dir"
fi

if [ ! -f "$build_dir/CMakeCache.txt" ]; then
	if [ "$use_local_juce" -eq 1 ]; then
		cmake -S "$temp_dir" -B "$build_dir" \
			-G Xcode \
			-DCMAKE_SYSTEM_NAME=iOS \
			-DCMAKE_OSX_ARCHITECTURES=arm64 \
			-DCMAKE_OSX_DEPLOYMENT_TARGET=16.0 \
			-DCMAKE_OSX_SYSROOT="$sdk_path" \
			-DMIXOLVE_VX_USE_LOCAL_JUCE=ON \
			-DJUCE_DIR="$juce_dir"
	else
		cmake -S "$temp_dir" -B "$build_dir" \
			-G Xcode \
			-DCMAKE_SYSTEM_NAME=iOS \
			-DCMAKE_OSX_ARCHITECTURES=arm64 \
			-DCMAKE_OSX_DEPLOYMENT_TARGET=16.0 \
			-DCMAKE_OSX_SYSROOT="$sdk_path"
	fi
fi

cmake --build "$build_dir" --config Debug --target Vx_iOSHost --parallel 4 -- -allowProvisioningUpdates
