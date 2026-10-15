#!/bin/sh
# tree.sh: the two ports as Top Gear Rally Remastered builds them.
#
# ports/brally and ports/tgrally are the games' own ports, nothing more.
# Everything Remastered adds lives here: its own files under brally/ and
# tgrally/, at the paths they take in each port, and its changes to the
# ports' files under patch/brally and patch/tgrally, one unified diff a file
# (patch/brally/platform/common/glide.c.patch changes
# ports/brally/platform/common/glide.c).  A patch that no longer applies
# stops the build: the port moved under it, and the patch is carried over by
# hand (make a tree, edit it, save it).
#
#   tree.sh make ROOT   ROOT/ports/brally and ROOT/ports/tgrally: each port,
#                       Remastered's files over it, its patches applied
#                       (exactly: no fuzz); beside them the repository's other
#                       folders, linked, so the ports' own scripts run in ROOT
#                       as they do at home (their outputs under ROOT/build)
#   tree.sh save ROOT   the other way, after editing ROOT's two trees: a file
#                       a port lacks goes to brally/ or tgrally/ here, a file
#                       that differs from the port's to its patch; the files
#                       and patches no longer needed are removed
#   tree.sh check ROOT  1 if saving ROOT would change anything here
set -e
cd "$(dirname "$0")/../.."
REPO=$(pwd)
R=ports/remastered
op=$1; ROOT=$2
[ -n "$op" ] && [ -n "$ROOT" ] || { echo "usage: tree.sh make|save|check ROOT" >&2; exit 2; }
case "$ROOT" in /*) ;; *) ROOT=$REPO/$ROOT;; esac
GAMES="brally tgrally"
# never saved
skip() { case "$1" in */__pycache__/*|*.pyc|*/.DS_Store|.DS_Store) return 0;; esac; return 1; }

make_tree() {
    mkdir -p "$ROOT/ports" "$ROOT/build/brally"
    for e in $(ls -A "$REPO"); do
        case "$e" in ports|build|.git) continue;; esac
        [ -e "$ROOT/$e" ] || ln -s "$REPO/$e" "$ROOT/$e"
    done
    for e in $(ls -A "$REPO/ports"); do
        case " $GAMES " in *" $e "*) continue;; esac
        [ -e "$ROOT/ports/$e" ] || ln -s "$REPO/ports/$e" "$ROOT/ports/$e"
    done
    # the verified build's placement (link64.sh makes it once)
    [ -e "$ROOT/build/brally/placement" ] || ln -s "$REPO/build/brally/placement" "$ROOT/build/brally/placement"
    for g in $GAMES; do
        rsync -a --delete --exclude __pycache__ "$REPO/ports/$g/" "$ROOT/ports/$g/"
        if [ -d "$R/$g" ]; then
            (cd "$R/$g" && find . -type f) | while read -r f; do
                mkdir -p "$(dirname "$ROOT/ports/$g/$f")"
                cp -p "$R/$g/$f" "$ROOT/ports/$g/$f"
            done
        fi
        [ -d "$R/patch/$g" ] || continue
        (cd "$R/patch/$g" && find . -name '*.patch' | sort) | while read -r p; do
            if ! patch -s -f -F0 -p1 -d "$ROOT/ports/$g" < "$R/patch/$g/$p" > "$ROOT/patch.log" 2>&1; then
                echo "tree: $R/patch/$g/${p#./} does not apply to ports/$g:" >&2
                cat "$ROOT/patch.log" >&2
                exit 1
            fi
            find "$ROOT/ports/$g" \( -name '*.orig' -o -name '*.rej' \) -delete
        done
    done
}

# what saving would write: into $1 (an empty folder), Remastered's files and patches
save_into() {
    out=$1
    for g in $GAMES; do
        base=$REPO/ports/$g
        tree=$ROOT/ports/$g
        (cd "$base" && find . -type f) | sort | while read -r f; do
            f=${f#./}
            skip "$f" && continue
            case "$f" in *.md) continue;; esac       # a port's notes: its own
            [ -f "$tree/$f" ] || { echo "tree: $tree/$f is gone; Remastered cannot remove a port's file" >&2; exit 1; }
        done
        (cd "$tree" && find . -type f) | sort | while read -r f; do
            f=${f#./}
            skip "$f" && continue
            if [ ! -f "$base/$f" ]; then
                mkdir -p "$(dirname "$out/$g/$f")"
                cp -p "$tree/$f" "$out/$g/$f"
            elif case "$f" in *.md) false;; *) ! cmp -s "$base/$f" "$tree/$f";; esac; then
                mkdir -p "$(dirname "$out/patch/$g/$f")"
                (cd "$REPO" && diff -u --label "a/$f" --label "b/$f" "ports/$g/$f" "$tree/$f" > "$out/patch/$g/$f.patch") || true
            fi
        done
    done
}

case "$op" in
make)
    make_tree;;
save|check)
    tmp=$(mktemp -d)
    save_into "$tmp"
    changed=0
    for d in brally tgrally patch; do
        if ! diff -r -q "$tmp/$d" "$R/$d" > /dev/null 2>&1; then
            if [ -d "$tmp/$d" ] || [ -d "$R/$d" ]; then changed=1; fi
        fi
    done
    if [ "$op" = check ]; then
        rm -rf "$tmp"
        [ $changed = 0 ] && echo "tree: $ROOT is what Remastered's files and patches make" || echo "tree: $ROOT differs"
        exit $changed
    fi
    for d in brally tgrally patch; do
        rm -rf "$R/$d"
        [ -d "$tmp/$d" ] && cp -Rp "$tmp/$d" "$R/$d"
    done
    rm -rf "$tmp"
    echo "tree: saved $(find $R/brally $R/tgrally -type f 2>/dev/null | wc -l | tr -d ' ') files and $(find $R/patch -name '*.patch' 2>/dev/null | wc -l | tr -d ' ') patches";;
*)
    echo "usage: tree.sh make|save|check ROOT" >&2; exit 2;;
esac
