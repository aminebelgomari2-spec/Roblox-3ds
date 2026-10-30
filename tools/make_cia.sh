#!/bin/bash
set -e
TARGET=roblox3ds
BIN="${DEVKITPRO:-/opt/devkitpro}/tools/bin"

"$BIN/bannertool" makebanner -i assets/banner.png -a assets/audio.wav -o banner.bnr
"$BIN/bannertool" makesmdh -s "Roblox 3DS" -l "Jeu 3D de fan : bombe, roquette, epee" -p "Moi" -i assets/icon.png -o icon.icn
"$BIN/makerom" -f cia -o $TARGET.cia -elf $TARGET.elf -rsf app.rsf -target t -exefslogo -icon icon.icn -banner banner.bnr

echo "CIA cree : $TARGET.cia"
