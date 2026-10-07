#!/bin/sh
# A presentation layer includes only the library API, the platform interface
# headers and its own headers. Usage: check_layers.sh <repo root> <layer dir>...
root="$1"
shift
allowed=' muslimtify.h muslimtify_cycle.h platform.h notification.h toast_activator.h util.h version.h '
status=0
for layer in "$@"; do
  for f in "$root/$layer"/*.c "$root/$layer"/*.h; do
    [ -e "$f" ] || continue
    for inc in $(sed -n 's/^[[:space:]]*#[[:space:]]*include[[:space:]]*"\([^"]*\)".*/\1/p' "$f"); do
      case "$allowed" in *" $inc "*) continue ;; esac
      [ -e "$root/$layer/$inc" ] && continue
      echo "$f includes $inc, which is not the library API, a platform interface or its own header"
      status=1
    done
  done
done
exit $status
