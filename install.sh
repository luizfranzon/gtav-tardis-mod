#!/bin/bash
# Copies ScriptHookV (third_party/runtime) and the mod into the GTA V Enhanced folder. --remove undoes it.
# TARDIS.ini is only copied when the game folder doesn't have one, so your settings survive reinstalls.
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
GTA=${GTA:-"/e/SteamLibrary/steamapps/common/Grand Theft Auto V Enhanced"}
RUNTIME=(ScriptHookV.dll dinput8.dll xinput1_4.dll args.txt OpenRPF.asi)
if [ "$1" = "--remove" ]; then
	for f in "${RUNTIME[@]}" TARDIS.asi; do rm -f "$GTA/$f"; done
	rm -rf "$GTA/TARDIS"
	echo "removed (TARDIS.ini and TARDIS.log left in place)"
	exit 0
fi
for f in "${RUNTIME[@]}"; do cp "$HERE/third_party/runtime/$f" "$GTA/"; done
cp "$HERE/build/TARDIS.asi" "$GTA/"
[ -f "$GTA/TARDIS.ini" ] || cp "$HERE/TARDIS.ini" "$GTA/"
# sounds come from your own TARDIS Rewrite download: python tools/extract_sounds.py <fastgmad.gma>
mkdir -p "$GTA/TARDIS/sounds" && cp "$HERE"/assets/sounds/*.wav "$GTA/TARDIS/sounds/"
mkdir -p "$GTA/TARDIS/translations" && cp "$HERE"/translations/*.ini "$GTA/TARDIS/translations/"
rm -f "$GTA/TARDIS_state.ini" # nothing is saved between sessions any more
echo "installed into $GTA"
