#!/bin/sh
# clang aimed at the iPhone (arm64, iOS 16 and later): link.sh's CC for HOST=ios.
# TGR_IOS_SIM=1 aims at the Simulator instead.
if [ -n "$TGR_IOS_SIM" ]; then
    exec xcrun -sdk iphonesimulator clang -target arm64-apple-ios16.0-simulator "$@"
fi
exec xcrun -sdk iphoneos clang -target arm64-apple-ios16.0 "$@"
