#!/bin/sh
# ios_sign.sh: sign the iPhone build with the player's own Xcode signing, when
# this Mac has it: a code-signing certificate in the keychain (Apple
# Development or Distribution) and an unexpired iOS provisioning profile made
# for that certificate (Xcode keeps one for each app it has signed), either for
# any app (a wildcard) or for this one (an app ID ending in "tgrally").
#
#   ios_sign.sh check       the signing found: "identity<TAB>profile<TAB>bundle id";
#                           exit 1 when there is none
#   ios_sign.sh sign APP    sign APP (a .app) with it: the profile embedded, the
#                           bundle identifier and entitlements the profile's
#
# Uses only what macOS has: security, plutil, base64, shasum, codesign.
set -u
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
NOW=$(date -u +%Y-%m-%dT%H:%M:%SZ)

# the certificates that can sign: SHA-1 and name
security find-identity -v -p codesigning 2>/dev/null |
    sed -n 's/^ *[0-9][0-9]*) \([0-9A-F]\{40\}\) "\(.*\)"$/\1	\2/p' |
    grep -E '	(Apple Development|Apple Distribution|iPhone Developer|iPhone Distribution):' > "$TMP/ids"

# the best profile for one of them: one for this app first, then a wildcard
find_signing() {
    best= bestrank=9
    for dir in "$HOME/Library/MobileDevice/Provisioning Profiles" \
               "$HOME/Library/Developer/Xcode/UserData/Provisioning Profiles"; do
        [ -d "$dir" ] || continue
        for f in "$dir"/*.mobileprovision; do
            [ -f "$f" ] || continue
            p="$TMP/p.plist"
            security cms -D -i "$f" > "$p" 2>/dev/null || continue
            plutil -extract Platform xml1 -o - "$p" 2>/dev/null | grep -q '<string>iOS</string>' || continue
            exp=$(plutil -extract ExpirationDate raw -o - "$p" 2>/dev/null) || continue
            [ "$exp" \> "$NOW" ] || continue
            appid=$(plutil -extract Entitlements.application-identifier raw -o - "$p" 2>/dev/null) || continue
            team=${appid%%.*}
            bundle=${appid#*.}
            case "$bundle" in
                *[Tt][Gg][Rr][Aa][Ll][Ll][Yy]) rank=0 ;;
                '*') rank=1; bundle=com.strutb.tgrally ;;
                *) continue ;;
            esac
            [ $rank -lt $bestrank ] || continue
            n=$(plutil -extract DeveloperCertificates raw -o - "$p" 2>/dev/null) || continue
            i=0
            while [ $i -lt "$n" ]; do
                sha=$(plutil -extract DeveloperCertificates.$i raw -o - "$p" | base64 -D | shasum -a 1 | cut -c1-40 | tr a-f A-F)
                id=$(grep "^$sha	" "$TMP/ids" | head -1 | cut -f1)
                if [ -n "$id" ]; then
                    best="$id	$f	$bundle	$team"
                    bestrank=$rank
                    break
                fi
                i=$((i + 1))
            done
        done
    done
    [ -n "$best" ] || return 1
    printf '%s\n' "$best"
}

case "${1:-}" in
check)
    s=$(find_signing) || exit 1
    id=$(printf '%s' "$s" | cut -f1)
    name=$(grep "^$id	" "$TMP/ids" | head -1 | cut -f2)
    printf '%s\t%s\t%s\n' "$name" "$(basename "$(printf '%s' "$s" | cut -f2)")" "$(printf '%s' "$s" | cut -f3)"
    ;;
sign)
    app=${2:?usage: ios_sign.sh sign APP}
    s=$(find_signing) || { echo "no signing certificate with a provisioning profile on this Mac" >&2; exit 1; }
    id=$(printf '%s' "$s" | cut -f1)
    prof=$(printf '%s' "$s" | cut -f2)
    bundle=$(printf '%s' "$s" | cut -f3)
    team=$(printf '%s' "$s" | cut -f4)
    cp "$prof" "$app/embedded.mobileprovision" || exit 1
    plutil -replace CFBundleIdentifier -string "$bundle" "$app/Info.plist" || exit 1
    security cms -D -i "$prof" > "$TMP/p.plist" &&
        plutil -extract Entitlements xml1 -o "$TMP/ent.plist" "$TMP/p.plist" &&
        plutil -replace application-identifier -string "$team.$bundle" "$TMP/ent.plist" || exit 1
    codesign --force --sign "$id" --entitlements "$TMP/ent.plist" --timestamp=none "$app" || exit 1
    ;;
*)
    echo "usage: ios_sign.sh check | sign APP" >&2
    exit 2
    ;;
esac
