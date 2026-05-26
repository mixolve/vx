#!/bin/bash

set -euo pipefail

export NO_COLOR=1
export CLICOLOR=0
export CLICOLOR_FORCE=0
export GCC_COLORS=
export CLANG_FORCE_COLOR_DIAGNOSTICS=NO
export XCODE_COLORS=NO
export TERM=dumb

strip_ansi() {
    perl -pe 's/\e\[[0-?]*[ -\/]*[@-~]//g; s/\e\][^\a]*(?:\a|\e\\)//g'
}

finish_command() {
    local status=$?

    trap - EXIT
    set +e

    if [ "$status" -eq 0 ]; then
        printf '\033[32mDONE\033[0m\n'
    else
        printf '\033[31mUNDONE\033[0m\n' >&2
    fi

    if [ "${VX_COMMAND_KEEP_OUTPUT:-1}" != "0" ] && [ -t 0 ]; then
        cd "${SCRIPT_DIR:-$PWD}" >/dev/null 2>&1 || true
        exec "${SHELL:-/bin/zsh}" -l
    fi

    exit "$status"
}

trap finish_command EXIT

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build/ios-app_auv3"
APP_PATH="${VX_IOS_APP_PATH:-${BUILD_DIR}/Vx_iOSHost_artefacts/Debug/vx.app}"
APPEX_PATH="${APP_PATH}/PlugIns/vx.appex"
RULES_FILE="${SCRIPT_DIR}/rules_builds.md"

resolve_app_path() {
    local build_dir="$1"
    local found_path=""

    found_path="$(find "${build_dir}" -type d -name "vx.app" -not -path "*/Index.noindex/*" -print -quit)"
    if [ -n "${found_path}" ]; then
        printf '%s\n' "${found_path}"
        return 0
    fi

    return 1
}

read_rules_device_udid() {
    [ -f "${RULES_FILE}" ] || return 0
    sed -n 's/.*UDID.*`\([^`]*\)`.*/\1/p' "${RULES_FILE}" | head -n 1
}

resolve_device_identifier() {
    local requested_id="${1:-}"
    local devices_json
    devices_json="$(mktemp)"

    if ! xcrun devicectl list devices --json-output "${devices_json}" --timeout 10 >/dev/null; then
        rm -f "${devices_json}"
        return 1
    fi

    python3 - "${devices_json}" "${requested_id}" <<'PY'
import json
import sys

json_path = sys.argv[1]
requested_id = sys.argv[2]

with open(json_path, "r", encoding="utf-8") as handle:
    data = json.load(handle)

devices = data.get("result", {}).get("devices", [])

for device in devices:
    identifier = device.get("identifier", "")
    hardware = device.get("hardwareProperties", {})
    connection = device.get("connectionProperties", {})
    udid = hardware.get("udid", "")
    platform = hardware.get("platform", "")
    reality = hardware.get("reality", "")
    paired = connection.get("pairingState") == "paired"

    if requested_id:
        if requested_id in {identifier, udid}:
            print(identifier)
            sys.exit(0)
        continue

    if platform in {"iOS", "iPadOS"} and reality == "physical" and paired and identifier:
        print(identifier)
        sys.exit(0)

sys.exit(1)
PY
    local status=$?
    rm -f "${devices_json}"
    return "${status}"
}

verify_bundle() {
    local bundle_path="$1"
    local bundle_label="$2"

    if [ ! -d "${bundle_path}" ]; then
        echo "${bundle_label} not found: ${bundle_path}" >&2
        echo "Run ./temp/build-ios-app_auv3.command first." >&2
        exit 1
    fi

    if ! codesign --verify --strict --verbose=2 "${bundle_path}" >/dev/null 2>&1; then
        codesign --verify --strict --verbose=2 "${bundle_path}" >&2
        echo "${bundle_label} code signature verification failed." >&2
        exit 1
    fi

    local profile_path="${bundle_path}/embedded.mobileprovision"
    if [ ! -f "${profile_path}" ]; then
        echo "${bundle_label} provisioning profile is missing: ${profile_path}" >&2
        exit 1
    fi
}

if [ ! -d "${APP_PATH}" ] && [ -z "${VX_IOS_APP_PATH:-}" ]; then
    APP_PATH="$(resolve_app_path "${BUILD_DIR}" || true)"
    if [ -z "${APP_PATH}" ]; then
        echo "iOS host app not found in ${BUILD_DIR}. Checked name: vx.app" >&2
        echo "Run ./temp/build-ios-app_auv3.command first." >&2
        exit 1
    fi

    APPEX_PATH="${APP_PATH}/PlugIns/vx.appex"
fi

verify_bundle "${APP_PATH}" "iOS host app"
verify_bundle "${APPEX_PATH}" "iOS AUv3 extension"

APP_BUNDLE_ID="$(/usr/libexec/PlistBuddy -c 'Print :CFBundleIdentifier' "${APP_PATH}/Info.plist")"
APPEX_BUNDLE_ID="$(/usr/libexec/PlistBuddy -c 'Print :CFBundleIdentifier' "${APPEX_PATH}/Info.plist")"

if [[ "${APPEX_BUNDLE_ID}" != "${APP_BUNDLE_ID}".* ]]; then
    echo "AUv3 bundle id must be prefixed by host bundle id: ${APP_BUNDLE_ID} -> ${APPEX_BUNDLE_ID}" >&2
    exit 1
fi

REQUESTED_DEVICE_ID="${1:-${VX_IOS_DEVICE_ID:-${VX_IOS_DEVICE_UDID:-}}}"
if [ -z "${REQUESTED_DEVICE_ID}" ]; then
    REQUESTED_DEVICE_ID="$(read_rules_device_udid || true)"
fi

RESOLVED_DEVICE_ID="$(resolve_device_identifier "${REQUESTED_DEVICE_ID}" || true)"
if [ -z "${RESOLVED_DEVICE_ID}" ]; then
    if [ -n "${REQUESTED_DEVICE_ID}" ]; then
        echo "iOS device ${REQUESTED_DEVICE_ID} is not visible to CoreDevice. Unlock/pair it or pass another device id." >&2
    else
        echo "No paired physical iOS device found. Pass a device id or set VX_IOS_DEVICE_ID." >&2
    fi
    exit 1
fi

if [ "${VX_IOS_UNINSTALL_FIRST:-0}" = "1" ]; then
    xcrun devicectl device uninstall app --device "${RESOLVED_DEVICE_ID}" "${APP_BUNDLE_ID}" 2>&1 | strip_ansi || true
fi

echo "Installing ${APP_BUNDLE_ID} with ${APPEX_BUNDLE_ID} to device ${RESOLVED_DEVICE_ID}"
xcrun devicectl device install app --device "${RESOLVED_DEVICE_ID}" "${APP_PATH}" 2>&1 | strip_ansi

if [ "${VX_IOS_LAUNCH:-1}" != "0" ]; then
    set +e
    LAUNCH_OUTPUT="$(xcrun devicectl device process launch --device "${RESOLVED_DEVICE_ID}" --terminate-existing "${APP_BUNDLE_ID}" 2>&1 | strip_ansi)"
    LAUNCH_STATUS=$?
    set -e

    if [ "${LAUNCH_STATUS}" -ne 0 ]; then
        printf '%s\n' "${LAUNCH_OUTPUT}" >&2

        if printf '%s\n' "${LAUNCH_OUTPUT}" | grep -qiE 'Locked|device was not, or could not, be unlocked'; then
            echo "App installed; launch skipped because the device is locked." >&2
            exit 0
        fi

        exit "${LAUNCH_STATUS}"
    fi

    printf '%s\n' "${LAUNCH_OUTPUT}"
fi
