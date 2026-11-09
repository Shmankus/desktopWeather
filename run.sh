#!/bin/bash

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
PREFIX_FILE="$SCRIPT_DIR/build/prefix.sh"
APP="$SCRIPT_DIR/build/bin/desktop-weather"

if [[ ! -f "$PREFIX_FILE" || ! -x "$APP" ]]; then
    printf 'Development build not found. Run %s/reinstall.sh first.\n' "$SCRIPT_DIR" >&2
    exit 1
fi

source "$PREFIX_FILE"
exec "$APP" "$@"
