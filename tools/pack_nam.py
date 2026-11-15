"""Pack NAM captures into Assets/private-ir/nam.bin, which the build embeds into the program (personal builds only).

Usage: python tools/pack_nam.py <folder with the NAM zip files>

Expects (names as downloaded from TONE3000):
  "Dumble ODS #102 Ford*.zip"   the Dumble captures: the standard (_S) ones go on the Dumble head's VOICE knob,
                                ordered from clean to driven by the "gain" each capture reports
  "Ibanez TS9*.zip"             TS9 captures at Drive 1, 3, 6 and 9: they go on the Tube Screamer's DRIVE knob
  "Boss SD-1*.zip"              SD-1 captures, Boost I-III then Drive I-VI: they go on the Overdrive's DRIVE knob

TONE3000 captures may be used but not redistributed, so the output is git-ignored.

Format (little endian), after zlib:
  "JGNM" u32 version u32 count, then per entry: u32 group (0 Dumble, 1 TS9, 2 SD-1) f32 order u32 nameLength name
  u32 jsonLength json
"""
import glob, io, json, os, re, struct, sys, zipfile, zlib

def main():
    src = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), '..', '..')
    out = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'Assets', 'private-ir', 'nam.bin')

    def zip_named(pattern):
        hits = glob.glob(os.path.join(src, pattern))
        if not hits:
            sys.exit('missing ' + pattern + ' in ' + src)
        return zipfile.ZipFile(hits[0])

    entries = []

    z = zip_named('Dumble ODS*.zip')
    dumble = []
    for name in z.namelist():
        if not name.endswith('_S.nam'):
            continue   # the XS versions are smaller copies of the same captures
        text = z.read(name).decode('utf-8')
        meta = json.loads(text).get('metadata', {})
        label = os.path.splitext(os.path.basename(name))[0].replace('SLAMMIN_DUMBLE_FORD_', '')[:-2]
        dumble.append((float(meta.get('gain', 0.5)), label, text))
    dumble.sort()
    for gain, label, text in dumble:
        entries.append((0, gain, label, text))
    print('Dumble, clean to driven:', ', '.join('%s %.2f' % (l, g) for g, l, _ in dumble))

    z = zip_named('Ibanez TS9*.zip')
    for name in z.namelist():
        m = re.search(r'_D(\d+)_', name)
        if m and name.endswith('.nam'):
            label = os.path.splitext(os.path.basename(name))[0]
            entries.append((1, float(m.group(1)), label, z.read(name).decode('utf-8')))
    print('TS9:', ', '.join(e[2] for e in sorted(e for e in entries if e[0] == 1)))

    z = zip_named('Boss SD-1*.zip')
    roman = ['I', 'II', 'III', 'IV', 'V', 'VI']
    for name in z.namelist():
        m = re.search(r'(Boost|Drive)-([IVX]+)\.nam$', name)
        if m:
            order = (0 if m.group(1) == 'Boost' else 3) + roman.index(m.group(2))   # boosts first, then the drives
            label = os.path.splitext(os.path.basename(name))[0].replace('APP-', '')
            entries.append((2, float(order), label, z.read(name).decode('utf-8')))
    print('SD-1:', ', '.join(e[2] for e in sorted(e for e in entries if e[0] == 2)))

    blob = io.BytesIO()
    blob.write(b'JGNM' + struct.pack('<II', 1, len(entries)))
    for group, order, label, text in entries:
        name = label.encode('utf-8')
        data = text.encode('utf-8')
        blob.write(struct.pack('<IfI', group, order, len(name)) + name + struct.pack('<I', len(data)) + data)
    packed = zlib.compress(blob.getvalue(), 9)
    os.makedirs(os.path.dirname(out), exist_ok=True)
    open(out, 'wb').write(packed)
    print('wrote', os.path.normpath(out), len(packed) // 1024, 'KB')

main()
