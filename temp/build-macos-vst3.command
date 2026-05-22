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

trap 'vx_command_exit' EXIT

build_dir="$temp_dir/build/macos-vst3"
cache_source_dir="CMAKE_HOME_DIRECTORY:INTERNAL=$temp_dir"
juce_subbuild_dir="$temp_dir/_deps/juce-subbuild"
juce_tools_dir="$temp_dir/_deps/juce-build/tools"

if [ -d "$build_dir" ] && {
	[ ! -f "$build_dir/CMakeCache.txt" ] || ! grep -Fqx "$cache_source_dir" "$build_dir/CMakeCache.txt"
}; then
	rm -rf "$build_dir"
fi

if [ -f "$juce_subbuild_dir/CMakeCache.txt" ] && ! grep -Fqx "CMAKE_GENERATOR:INTERNAL=Xcode" "$juce_subbuild_dir/CMakeCache.txt"; then
	rm -rf "$juce_subbuild_dir"
fi

if [ -f "$juce_tools_dir/CMakeCache.txt" ] && ! grep -Fqx "CMAKE_GENERATOR:INTERNAL=Xcode" "$juce_tools_dir/CMakeCache.txt"; then
	rm -rf "$juce_tools_dir"
fi

cmake -S "$temp_dir" -B "$build_dir" -G Xcode && cmake --build "$build_dir" --config Debug --target Vx_VST3 --parallel 4
