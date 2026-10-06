#!/usr/bin/env bash
#
# List every load command in an app bundle that points outside it, and fail
# if there is one. A shipped bundle may search for libraries only relative to
# itself (LC_RPATH under @executable_path or @loader_path), and may name only
# the absolute paths every Mac has (/System, /usr/lib).
#
#   packaging/macos/check-bundle-paths.sh "build/dmg-staging/Sony Device Center.app"
#
# Prints "<file inside the bundle><TAB><load command><TAB><path>" per offender.
# build-dmg.sh rewrites the rpaths and install names this reports, then runs it
# again before signing; verify-dmg.sh runs it on the mounted image.
#
# One stray rpath is enough to break the app on a Mac that has Qt installed:
# CMake's /opt/homebrew/opt/qt/lib on the main binary let dyld resolve
# @rpath/QtCore.framework there, and a second QtCore loaded next to the
# bundled one ("Class KeyValueObserver is implemented in both").

set -euo pipefail

app=${1%/}
clean=true
while IFS= read -r -d '' binary; do
    file -b "$binary" | grep -q 'Mach-O' || continue
    # otool prints each slice of a universal binary; sort -u folds them.
    while IFS=$'\t' read -r cmd path; do
        case $cmd:$path in
            LC_RPATH:@executable_path|LC_RPATH:@executable_path/*) continue ;;
            LC_RPATH:@loader_path|LC_RPATH:@loader_path/*) continue ;;
            LC_RPATH:*) ;;
            *:/System/*|*:/usr/lib/*) continue ;;
            *:/*) ;;
            *) continue ;;
        esac
        printf '%s\t%s\t%s\n' "${binary#"$app"/}" "$cmd" "$path"
        clean=false
    done < <(otool -l "$binary" | awk '
        $1 == "cmd" { cmd = $2 }
        (cmd == "LC_RPATH" && $1 == "path") || (cmd ~ /DYLIB$/ && $1 == "name") {
            sub(/^ *(path|name) /, ""); sub(/ \(offset [0-9]+\)$/, "")
            print cmd "\t" $0
        }' | sort -u)
done < <(find "$app" -type f -print0)
$clean
