"""Pack recorded impulse responses into Assets/private-ir/irs.bin, which the build embeds into the program.

Usage: python tools/pack_irs.py <folder with the IR zip files>

Expects (names as sold / downloaded):
  "Fender Twin Reverb.zip"                         3 cabinet IRs (CLEAN, BALANCED, MIDS)
  "Two Rock 2x12 Open Back WGS 65B*.zip"           3 cabinet IRs (U87, 545, 421/U5/M160)
  "Fender Spring Reverb Tank.zip"                  100 spring IRs, "Dwell NN_Tone NN"

The output is personal-use only (git-ignored): most IR licenses allow using the files but not redistributing them.

Spring IRs are trimmed where their decay reaches the noise floor (the files run on for up to 27 s, and the end holds
sweep artefacts), faded out, and stored as 16-bit. Cabinet IRs are aligned on their main peak so the MIC knob can
blend them without comb filtering, and stored as 32-bit float. The whole pack is zlib-compressed.

Format (little endian), after zlib:
  "JGIR" u32 version u32 count, then per entry: u32 group (0 Fender cab, 1 Two-Rock cab, 2 spring) u32 a u32 b
  u32 sampleRate u32 length u32 sampleFormat (1 float32, 2 int16) f32 scale u32 offset; then the sample data.
"""
import array, glob, io, math, os, re, struct, sys, zipfile, zlib

def read_wav_bytes(data):
    assert data[:4] == b'RIFF' and data[8:12] == b'WAVE'
    pos, fmt, frames = 12, None, None
    while pos + 8 <= len(data):
        cid = data[pos:pos + 4]
        size = struct.unpack('<I', data[pos + 4:pos + 8])[0]
        body = data[pos + 8:pos + 8 + size]
        if cid == b'fmt ':
            tag, ch, rate, _, _, bits = struct.unpack('<HHIIHH', body[:16])
            if tag == 0xFFFE:
                tag = struct.unpack('<H', body[24:26])[0]
            fmt = (tag, ch, rate, bits)
        elif cid == b'data':
            frames = body
        pos += 8 + size + (size & 1)
    tag, ch, rate, bits = fmt
    if tag == 3 and bits == 32:
        v = array.array('f'); v.frombytes(frames[:len(frames) // 4 * 4]); x = list(v)
    elif bits == 16:
        v = array.array('h'); v.frombytes(frames[:len(frames) // 2 * 2]); x = [s / 32768.0 for s in v]
    elif bits == 24:
        n = len(frames) // 3
        ba = bytearray(4 * n)
        ba[1::4] = frames[0:3 * n:3]; ba[2::4] = frames[1:3 * n:3]; ba[3::4] = frames[2:3 * n:3]
        v = array.array('i'); v.frombytes(bytes(ba)); x = [s / 2147483648.0 for s in v]
    elif bits == 32:
        v = array.array('i'); v.frombytes(frames[:len(frames) // 4 * 4]); x = [s / 2147483648.0 for s in v]
    else:
        raise ValueError('unsupported wav: %r' % (fmt,))
    return rate, x[0::ch]   # first channel

def align_peak(x, lead=16):
    peak = max(range(len(x)), key=lambda i: abs(x[i]))
    shift = peak - lead
    return x[shift:] if shift > 0 else [0.0] * (-shift) + x

def align_to(ref, x, max_lag=96):
    """shift (and if needed flip) x so it lines up best with ref: blending then adds up instead of cancelling"""
    n = min(len(ref), len(x), 4096)
    best, best_lag = 0.0, 0
    for lag in range(-max_lag, max_lag + 1):
        c = 0.0
        for i in range(max(0, -lag), min(n, n - lag)):
            c += ref[i] * x[i + lag]
        if abs(c) > abs(best):
            best, best_lag = c, lag
    y = x[best_lag:] if best_lag > 0 else [0.0] * (-best_lag) + x
    if best < 0:
        y = [-v for v in y]
    return y

def trim_spring(x, rate):
    blk = int(rate * 0.02)
    env = []
    for s in range(0, len(x), blk):
        seg = x[s:s + blk]
        env.append(math.sqrt(sum(v * v for v in seg) / len(seg)) + 1e-12)
    top = max(env)
    db = [20 * math.log10(e / top) for e in env]
    end = len(db)
    run_min, run_min_at = 0.0, 0
    for i in range(3, len(db)):
        if db[i] < -66.0:
            end = i
            break
        if db[i] < run_min:
            run_min, run_min_at = db[i], i
        elif i * blk > 0.3 * rate and db[i] > run_min + 9.0:   # rising again: the noise floor and sweep artefacts
            end = run_min_at
            break
    n = min(len(x), end * blk + blk, int(4.0 * rate))
    y = x[:n]
    fade = max(int(0.05 * rate), n // 6)
    for i in range(fade):
        y[n - fade + i] *= 0.5 + 0.5 * math.cos(math.pi * i / fade)
    return y

def main():
    src = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), '..', '..')
    out = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'Assets', 'private-ir', 'irs.bin')

    def zip_named(pattern):
        hits = glob.glob(os.path.join(src, pattern))
        if not hits:
            sys.exit('missing ' + pattern + ' in ' + src)
        return zipfile.ZipFile(hits[0])

    entries = []   # (group, a, b, rate, samples, format)

    # cabinets: knob order from dark to bright
    for group, pattern, order in ((0, 'Fender Twin Reverb*.zip', ['MIDS', 'BALANCED', 'CLEAN']),
                                  (1, 'Two Rock 2x12*.zip', ['U87', '545', '421'])):
        z = zip_named(pattern)
        names = [n for n in z.namelist() if n.lower().endswith('.wav')]
        for idx, key in enumerate(order):
            name = next(n for n in names if key in n.upper())
            rate, x = read_wav_bytes(z.read(name))
            x = align_peak(x)
            if idx > 0:   # line each mic up with the previous one, so neighbours on the MIC knob blend cleanly
                x = align_to(entries[-1][4], x)
            x = x[: int(rate * 0.5)]
            entries.append((group, idx, 0, rate, x, 1))
            print('cab', group, idx, name, len(x) / rate, 's')

    # springs
    z = zip_named('Fender Spring Reverb Tank*.zip')
    springs = {}
    for name in z.namelist():
        m = re.search(r'Dwell (\d+)_Tone (\d+)', name)
        if m and name.lower().endswith('.wav'):
            rate, x = read_wav_bytes(z.read(name))
            springs[(int(m.group(1)), int(m.group(2)))] = (rate, trim_spring(x, rate))
    ref_rate, ref = springs[(5, 5)]
    ref_energy = math.sqrt(sum(v * v for v in ref))
    for (dwell, tone), (rate, x) in sorted(springs.items()):
        x = [v / ref_energy for v in x]   # Dwell 5 / Tone 5 has unit energy; the others keep their level relative to it
        entries.append((2, dwell, tone, rate, x, 2))
    print('springs:', len(springs), 'average', sum(len(x) / r for r, x in springs.values()) / len(springs), 's')

    header = io.BytesIO()
    data = io.BytesIO()
    header.write(b'JGIR' + struct.pack('<II', 1, len(entries)))
    for group, a, b, rate, x, fmt in entries:
        offset = data.tell()
        if fmt == 1:
            data.write(array.array('f', x).tobytes()); scale = 1.0
        else:
            peak = max(abs(v) for v in x) or 1.0
            scale = peak / 32767.0
            data.write(array.array('h', [max(-32767, min(32767, int(round(v / scale)))) for v in x]).tobytes())
        header.write(struct.pack('<IIIIIIfI', group, a, b, rate, len(x), fmt, scale, offset))
    blob = zlib.compress(header.getvalue() + data.getvalue(), 9)
    os.makedirs(os.path.dirname(out), exist_ok=True)
    open(out, 'wb').write(blob)
    print('wrote', os.path.normpath(out), len(blob) // 1024, 'KB')

main()
