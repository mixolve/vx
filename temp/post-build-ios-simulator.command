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
	cleanup_fn=${1:-}

	trap - EXIT
	set +e

	if [ -n "$cleanup_fn" ]; then
		"$cleanup_fn"
	fi

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

cleanup_devices_json() {
	if [ -n "${devices_json:-}" ]; then
		rm -f "$devices_json"
	fi
}

trap 'vx_command_exit cleanup_devices_json' EXIT

app_bundle="$repo_root/temp/build/ios-simulator/Vx_iOSHost_artefacts/Debug/vx-app.app"

if [ ! -d "$app_bundle" ]; then
	echo "Missing iOS simulator app bundle: $app_bundle" >&2
	exit 1
fi

app_bundle_identifier=$(/usr/libexec/PlistBuddy -c 'Print :CFBundleIdentifier' "$app_bundle/Info.plist")

simulator_id=${1:-${VX_IOS_SIMULATOR_ID:-}}
if [ -z "$simulator_id" ]; then
	devices_json=$(mktemp)
	if ! xcrun simctl list devices --json > "$devices_json"; then
		echo "Unable to list iOS simulators." >&2
		exit 1
	fi
	if [ ! -s "$devices_json" ]; then
		echo "No iOS simulators found. Pass the simulator UUID as the first argument." >&2
		exit 1
	fi
	if ! command -v python3 >/dev/null 2>&1; then
		echo "python3 is required to auto-select an iOS simulator. Pass the simulator UUID as the first argument." >&2
		exit 1
	fi
	simulator_id=$(python3 - "$devices_json" <<'PY'
import json
import sys

with open(sys.argv[1], "r", encoding="utf-8") as handle:
    payload = json.load(handle)

devices = []
for runtime_name, runtime_devices in payload.get("devices", {}).items():
    if "iOS" not in runtime_name:
        continue
    for device in runtime_devices:
        if not device.get("isAvailable"):
            continue
        name = device.get("name", "")
        if not (name.startswith("iPhone") or name.startswith("iPad")):
            continue
        devices.append(device)

for device in devices:
    if device.get("state") == "Booted":
        identifier = device.get("udid")
        if identifier:
            print(identifier)
            sys.exit(0)

if devices:
    identifier = devices[0].get("udid")
    if identifier:
        print(identifier)
PY
)
fi

if [ -z "$simulator_id" ]; then
	echo "No available iOS simulator found. Pass the simulator UUID as the first argument." >&2
	exit 1
fi

simulator_running=0
if pgrep -x Simulator >/dev/null 2>&1; then
	simulator_running=1
fi

simulator_booted=0
if xcrun simctl list devices | grep -F "($simulator_id) (Booted)" >/dev/null 2>&1; then
	simulator_booted=1
fi

if [ "$simulator_running" -eq 0 ]; then
	echo "Opening iOS Simulator"
	open -a Simulator
fi

echo "Installing to iOS simulator: $simulator_id"
xcrun simctl boot "$simulator_id" >/dev/null 2>&1 || true
if [ "$simulator_running" -eq 0 ] || [ "$simulator_booted" -eq 0 ]; then
	vx_run_clean xcrun simctl bootstatus "$simulator_id" -b
fi
vx_run_clean xcrun simctl install "$simulator_id" "$app_bundle"
echo "Launching iOS simulator app: $app_bundle_identifier"
vx_run_clean xcrun simctl launch --terminate-running-process "$simulator_id" "$app_bundle_identifier"
