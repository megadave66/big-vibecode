#!/usr/bin/env python3
"""Generate 10 original chiptune tracks (CC0). Deterministic.
Usage: python3 tools/gen_music.py [outdir]   -> writes trackNN.wav then encodes
to ../assets/music/trackNN.ogg with ffmpeg and writes tracks.json."""
import json, os, subprocess, sys, wave
import numpy as np

SR = 44100
FF = "/opt/homebrew/bin/ffmpeg"
FP = "/opt/homebrew/bin/ffprobe"
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MUSIC = os.path.join(ROOT, "assets", "music")
TMP = sys.argv[1] if len(sys.argv) > 1 else "/tmp/gd-assets/wav"

# (title, bpm, root midi note, scale, lead wave duty, drum density)
MINOR = [0, 2, 3, 5, 7, 8, 10]
MAJOR = [0, 2, 4, 5, 7, 9, 11]
PENTA = [0, 3, 5, 7, 10]
TRACKS = [
    ("Neon Drift",      104, 57, MINOR, 0.50, 0),
    ("Pixel Sunrise",   112, 60, MAJOR, 0.25, 0),
    ("Grid Runner",     120, 55, PENTA, 0.50, 1),
    ("Circuit Bloom",   128, 62, MINOR, 0.25, 1),
    ("Laser Garden",    136, 59, MAJOR, 0.125, 2),
    ("Turbo Lattice",   144, 57, MINOR, 0.50, 2),
    ("Overclock",       152, 64, PENTA, 0.25, 3),
    ("Static Rush",     160, 60, MINOR, 0.125, 3),
    ("Hyperdrive",      170, 58, MINOR, 0.25, 4),
    ("Final Voltage",   180, 62, MINOR, 0.125, 4),
]
PROGS = [[0, 5, 3, 4], [0, 3, 4, 3], [0, 4, 5, 3], [5, 3, 0, 4]]


def mtof(m):
    return 440.0 * 2 ** ((m - 69) / 12)


def osc(kind, f, n, duty=0.5):
    t = np.arange(n) / SR
    ph = (f * t) % 1.0
    if kind == "sq":
        return np.where(ph < duty, 1.0, -1.0)
    if kind == "tri":
        return 4 * np.abs(ph - 0.5) - 1
    return np.zeros(n)


def env(n, a=0.004, rel=0.04, sus=1.0):
    e = np.ones(n) * sus
    na, nr = int(a * SR), int(rel * SR)
    na, nr = min(na, n), min(nr, n)
    if na: e[:na] *= np.linspace(0, 1, na)
    if nr: e[-nr:] *= np.linspace(1, 0, nr)
    return e


def degree(scale, root, d):
    return root + scale[d % len(scale)] + 12 * (d // len(scale))


def build(idx, spec, seconds=48.0):
    title, bpm, root, scale, duty, dens = spec
    rng = np.random.default_rng(1000 + idx)
    step = 60.0 / bpm / 2  # eighth note
    nsteps = int(seconds / step)
    total = int(seconds * SR) + SR
    out = np.zeros(total)
    prog = PROGS[idx % len(PROGS)]
    # melody motif: 16 steps, reused with variation per bar
    motif = [int(x) for x in rng.integers(0, 7, 16)]
    rest = rng.random(16) < (0.35 - 0.05 * dens)
    for s in range(nsteps):
        bar, pos = divmod(s, 8)
        chord = prog[(bar // 1) % 4]
        t0 = int(s * step * SR)
        n = int(step * SR)
        # bass: triangle, root of chord, octave -1
        bn = degree(scale, root - 12, chord)
        if dens >= 1 and pos % 2 == 1:
            bn += 12
        seg = osc("tri", mtof(bn), n) * env(n, rel=0.03) * 0.45
        out[t0:t0 + n] += seg
        # arp (pulse) from chord tones
        if dens >= 2 or pos % 2 == 0:
            tone = [0, 2, 4, 2][pos % 4] if dens >= 2 else [0, 2][pos % 2 // 1 % 2]
            an = degree(scale, root + 12, chord + tone)
            out[t0:t0 + n] += osc("sq", mtof(an), n, 0.25) * env(n, rel=0.02) * 0.07
        # lead
        mi = (s + 3 * (bar // 4)) % 16
        if not rest[mi] or pos == 0:
            ln = degree(scale, root + 12, chord + motif[mi] % 5)
            if bar % 8 >= 4 and dens >= 1:
                ln += 12 if motif[mi] % 3 == 0 else 0
            dur = n if pos % 2 else int(n * 0.9)
            out[t0:t0 + dur] += osc("sq", mtof(ln), dur, duty) * env(dur, rel=0.03) * 0.16
        # drums
        kick = pos in (0, 4) or (dens >= 3 and pos in (2, 6) and bar % 2)
        snare = pos in (2, 6) if dens >= 1 else pos == 4
        hat = (dens >= 1 and pos % 2 == 1) or (dens >= 2)
        if kick:
            kn = int(0.12 * SR); tt = np.arange(kn) / SR
            f = 120 * np.exp(-tt * 30) + 40
            k = np.sin(2 * np.pi * np.cumsum(f) / SR) * np.exp(-tt * 22)
            out[t0:t0 + kn] += k * 0.6
        if snare:
            sn = int(0.10 * SR); tt = np.arange(sn) / SR
            out[t0:t0 + sn] += (rng.random(sn) * 2 - 1) * np.exp(-tt * 35) * 0.25
        if hat:
            hn = int(0.03 * SR); tt = np.arange(hn) / SR
            out[t0:t0 + hn] += (rng.random(hn) * 2 - 1) * np.exp(-tt * 120) * 0.10
    out = out[: int(seconds * SR)]
    peak = np.max(np.abs(out)) or 1
    return out / peak * 0.9


def write_wav(path, mono):
    pcm = (np.clip(mono, -1, 1) * 32767).astype("<i2")
    st = np.repeat(pcm[:, None], 2, axis=1)
    with wave.open(path, "wb") as w:
        w.setnchannels(2); w.setsampwidth(2); w.setframerate(SR)
        w.writeframes(st.tobytes())


def main():
    os.makedirs(TMP, exist_ok=True); os.makedirs(MUSIC, exist_ok=True)
    meta = []
    for i, spec in enumerate(TRACKS):
        name = f"track{i + 1:02d}"
        wav = os.path.join(TMP, name + ".wav")
        ogg = os.path.join(MUSIC, name + ".ogg")
        secs = 44.0 + (i % 4) * 3  # 44..53 s
        write_wav(wav, build(i, spec, secs))
        fade = secs - 2
        subprocess.run([FF, "-y", "-v", "error", "-i", wav, "-af",
                        f"afade=t=out:st={fade}:d=2,loudnorm=I=-16:TP=-1.5:LRA=11",
                        "-ar", "44100", "-ac", "2", "-c:a", "vorbis", "-strict", "-2", "-b:a", "160k", ogg], check=True)
        d = float(subprocess.check_output([FP, "-v", "error", "-show_entries", "format=duration",
                                           "-of", "csv=p=0", ogg]).decode().strip())
        meta.append({"file": f"music/{name}.ogg", "title": spec[0], "author": "gen_music.py (original, CC0)",
                     "seconds": round(d, 3), "bpm": spec[1]})
    with open(os.path.join(MUSIC, "tracks.json"), "w") as f:
        json.dump(meta, f, indent=2)


if __name__ == "__main__":
    main()
