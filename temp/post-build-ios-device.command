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

app_bundle="$repo_root/temp/build/ios-device/Vx_iOSHost_artefacts/Debug/vx-app.app"
appex_bundle="$app_bundle/PlugIns/vx.appex"

if [ ! -d "$app_bundle" ]; then
	echo "Missing iOS device app bundle: $app_bundle" >&2
	exit 1
fi

if [ ! -d "$appex_bundle" ]; then
	echo "Missing iOS AUv3 extension bundle: $appex_bundle" >&2
	exit 1
fi

check_embedded_profile() {
	bundle_path=$1
	bundle_label=$2
	profile_path="$bundle_path/embedded.mobileprovision"

	if [ ! -f "$profile_path" ]; then
		echo "Missing $bundle_label provisioning profile: $profile_path" >&2
		exit 1
	fi

	expiration_date=$(security cms -D -i "$profile_path" 2>/dev/null | plutil -extract ExpirationDate raw -o - - 2>/dev/null || true)
	if [ -z "$expiration_date" ]; then
		echo "Unable to read $bundle_label provisioning profile expiration: $profile_path" >&2
		exit 1
	fi

	expiration_epoch=$(date -j -u -f "%Y-%m-%dT%H:%M:%SZ" "$expiration_date" "+%s" 2>/dev/null || true)
	if [ -z "$expiration_epoch" ]; then
		echo "Unable to parse $bundle_label provisioning profile expiration: $expiration_date" >&2
		exit 1
	fi

	now_epoch=$(date -u "+%s")
	if [ "$expiration_epoch" -le "$now_epoch" ]; then
		echo "$bundle_label provisioning profile expired: $expiration_date" >&2
		echo "Rebuild iOS device target to refresh signing before installing." >&2
		exit 1
	fi

	echo "$bundle_label provisioning profile expires: $expiration_date"
}

check_embedded_profile "$app_bundle" "iOS app"
check_embedded_profile "$appex_bundle" "iOS AUv3 extension"

app_bundle_identifier=$(/usr/libexec/PlistBuddy -c 'Print :CFBundleIdentifier' "$app_bundle/Info.plist")

device_id=${1:-${VX_IOS_DEVICE_ID:-}}
if [ -z "$device_id" ]; then
	devices_json=$(mktemp)
	if ! xcrun devicectl list devices --json-output "$devices_json" >/dev/null; then
		echo "Unable to list connected iOS devices." >&2
		exit 1
	fi
	if [ ! -s "$devices_json" ]; then
		echo "No connected iOS devices found. Pass the device identifier as the first argument." >&2
		exit 1
	fi
	if ! command -v python3 >/dev/null 2>&1; then
		echo "python3 is required to auto-select an iOS device. Pass the device identifier as the first argument." >&2
		exit 1
	fi
	device_id=$(python3 - "$devices_json" <<'PY'
import json
import sys

with open(sys.argv[1], "r", encoding="utf-8") as handle:
    payload = json.load(handle)

for device in payload.get("result", {}).get("devices", []):
    hardware = device.get("hardwareProperties", {})
    connection = device.get("connectionProperties", {})
    if hardware.get("platform") in {"iOS", "iPadOS"} and hardware.get("reality") == "physical" and connection.get("pairingState") == "paired":
        identifier = device.get("identifier")
        if identifier:
            print(identifier)
            break
PY
)
fi

if [ -z "$device_id" ]; then
	echo "No paired physical iOS device found. Pass the device identifier as the first argument." >&2
	exit 1
fi

echo "Installing to iOS device: $device_id"
vx_run_clean xcrun devicectl device install app --device "$device_id" "$app_bundle"
echo "Launching iOS device app: $app_bundle_identifier"
vx_run_clean xcrun devicectl device process launch --device "$device_id" --terminate-existing "$app_bundle_identifier"
