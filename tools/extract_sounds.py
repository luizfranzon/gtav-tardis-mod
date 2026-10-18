"""Pull the TARDIS sounds out of your own copy of TARDIS Rewrite (Workshop 307175678) into assets/sounds/ as WAV.

Usage: python tools/extract_sounds.py <path to fastgmad.gma> [out dir]
Needs ffmpeg on PATH for the .ogg ones. The sounds stay on your machine: they are not part of this repo.
"""
import os
import struct
import subprocess
import sys
import tempfile

# mod file name -> path inside the GMA (TARDIS Rewrite: interiors/base.lua + templates_default_exterior.lua)
SOUNDS = {
    "spawn.wav": "sound/p00gie/tardis/base/repairfinish.wav",       # Exterior.Sounds.Spawn
    "delete.wav": "sound/p00gie/tardis/base/tardis_delete.wav",     # Exterior.Sounds.Delete
    "flight_loop.wav": "sound/drmatt/tardis/flight_loop.wav",       # Exterior.Sounds.FlightLoop
    "flight_land.wav": "sound/p00gie/tardis/default/tardis_land.ogg",  # Toyota FlightLand
    "flight_fall.wav": "sound/p00gie/tardis/fall.wav",              # Exterior.Sounds.FlightFall
}


def read_gma(path):
    """Yield (name, bytes) for every file in a Garry's Mod .gma archive."""
    with open(path, "rb") as f:
        if f.read(4) != b"GMAD":
            raise SystemExit(f"{path} is not a GMA file")
        f.read(1 + 8 + 8)

        def cstr():
            b = bytearray()
            while (c := f.read(1)) != b"\0":
                b += c
            return b.decode("utf8", "replace")

        while cstr():  # required content list
            pass
        cstr(), cstr(), cstr()  # name, description, author
        f.read(4)
        entries = []
        while struct.unpack("<I", f.read(4))[0]:
            name = cstr()
            size, = struct.unpack("<q", f.read(8))
            f.read(4)
            entries.append((name, size))
        for name, size in entries:
            yield name, f.read(size)


def main():
    if len(sys.argv) < 2:
        raise SystemExit(__doc__)
    out = sys.argv[2] if len(sys.argv) > 2 else os.path.join(os.path.dirname(__file__), "..", "assets", "sounds")
    os.makedirs(out, exist_ok=True)
    wanted = {v: k for k, v in SOUNDS.items()}
    found = set()
    for name, data in read_gma(sys.argv[1]):
        if name not in wanted:
            continue
        dest = os.path.join(out, wanted[name])
        if name.endswith(".wav"):
            open(dest, "wb").write(data)
        else:
            with tempfile.NamedTemporaryFile(suffix=os.path.splitext(name)[1], delete=False) as tmp:
                tmp.write(data)
            subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", tmp.name, dest], check=True)
            os.remove(tmp.name)
        found.add(name)
        print(f"{wanted[name]:18} <- {name}")
    missing = set(wanted) - found
    if missing:
        raise SystemExit(f"missing from the GMA: {sorted(missing)}")


if __name__ == "__main__":
    main()
