import os
"""Write reference/tgrally/XM/SAMPLE-SOURCES.md from provenance.json plus the hand verdicts below."""
import json, datetime, glob
P = json.load(open('provenance.json'))
OUT = os.path.join(os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '..', '..')), 'reference/tgrally/XM/SAMPLE-SOURCES.md')
MOD = {'0xebc00': ('Title', 'xm_0EBC00'), '0x113660': ('Desert', 'xm_113660'), '0x12eab0': ('Mountain', 'xm_12EAB0'),
       '0x149c80': ('Coastline', 'xm_149C80'), '0x164b60': ('Strip Mine', 'xm_164B60'), '0x17fd10': ('Jungle', 'xm_17FD10')}
USED = {'0xebc00#5', '0xebc00#1', '0xebc00#10', '0xebc00#13', '0x164b60#19', '0x17fd10#0', '0x17fd10#1'}
NOTE = {
 '0xebc00#5': 'UPGRADE, in use. permian.xm (Mod Archive #54465) holds it 16-bit at twice the rate. The ROM copy is that halved in rate, cut to 8-bit and amplified until its peaks clip; excluding the clipped peaks the two correlate 1.0000.',
 '0xebc00#10': 'UPGRADE, in use. rave_opera.s3m (Mod Archive #70077) sample 4. BIT-EXACT: taking every sqrt(2)-th sample of the source (nearest neighbour) reproduces the ROM copy byte for byte.',
 '0xebc00#1': 'UPGRADE, in use. windscrn.xm (Mod Archive #63094) instrument 8, sqrt(2) times the rate. Correlation 0.994; 92% of the ROM bytes are identical to plain decimation of it.',
 '0xebc00#13': 'UPGRADE, in use. purespin.s3m (Mod Archive #57172) sample 13, twice the rate (correlation 0.98). The same recording, also at twice the rate, is in "do you really" by Mittag-Leffler (Modland, 0.991).',
 '0x164b60#19': 'UPGRADE, in use. lotus_2_completition_music_remix_2.xm (Mod Archive #135802) instrument 16, 1.68 times the rate (correlation 0.99).',
 '0x17fd10#0': 'UPGRADE, in use. The ST-86 disk sample "grandpiano3"; the ROM copy is it pitched up 3 semitones (rate x1.189) and amplified about 1.8x. The "3" in the name is the semitone shift.',
 '0x17fd10#1': 'UPGRADE, in use. The ST-86 disk sample "grandpiano4"; the ROM copy is it pitched up 4 semitones (rate x1.260) and amplified about 1.8x.',
 '0x164b60#2': 'ST-58 disk "distchord", same rate, correlation 0.999. The source is longer (31304 samples); the ROM copy is it cut to 24530. Not used: the song never plays past the ROM length, so the swap would sound identical. A WAV of it is in this folder anyway.',
 '0x149c80#3': 'COMPOSER-MADE. A chord built from the "my mind..." string (ST-58 rsstring): the name is the recipe, root plus 3 and 7 semitones (minor). A solver puts the weight on exactly those three pitches; the exact mix (levels, resampler) was not reproduced. Nothing to source beyond rsstring.',
 '0x149c80#4': 'COMPOSER-MADE. Chord of ST-58 rsstring at 0, 4 and 9 semitones (see 037.iff).',
 '0x149c80#5': 'COMPOSER-MADE. Chord of ST-58 rsstring at 0, 5 and 8 semitones (see 037.iff).',
 '0xebc00#15': 'Byte-identical to "xbass" on the ST-A5 disk and to the sample in Allister Brimble\'s Project-X tune (anarchy-2-1629.mod). NOT the same as XXBASS.WAV in x4songa.it (Mod Archive #155594): that one is 16-bit 44.1 kHz but a different recording (correlation 0.20 at every rate).',
 '0x17fd10#2': 'Same sample as the title piece\'s xxbass (see there). XXBASS.WAV in x4songa.it is a different recording.',
 '0x164b60#13': 'PROBABLE only. dazzler2.mod (Mod Archive #36504) sample 7, 4 semitones higher (correlation 0.94, 0.90 over the full length). Not strong enough to call the same recording; not used.',
 '0x164b60#20': 'The ST-28 disk\'s "yesbunk" (also in switchblade_2_title.mod and planet_fall_boom.mod). The ROM copy is an edited, shortened version at the same rate (correlation 0.976). Same 8-bit quality; not an upgrade.',
 '0x164b60#5': 'ST-46 disk "killvoice", same rate, correlation 0.989, slightly longer. Same quality; not an upgrade.',
 '0x149c80#7': 'Counterpoint\'s f_o_c.mod / fochoice.mod sample 2, same length, correlation 0.989: an edited copy (level or small changes), not byte-identical. Same quality.',
 '0x149c80#12': 'Counterpoint\'s f_o_c.mod / fochoice.mod sample 4, same length, correlation 0.989: an edited copy. Same quality.',
 '0xebc00#11': 'NOT FOUND. Rhino\'s modules (aura, craft2, rose_garden and others on Mod Archive) use the same instrument text "/4518" and a sample named "sd1", but the audio does not match (best 0.85 at x1.414).',
 '0x12eab0#8': 'NOT FOUND. One of seven similar "NoName" samples (7301-7479 samples each) in this piece; probably one sound the composer rendered at several pitches or with different processing.',
}
NOTE['0x12eab0#4'] = 'PROBABLE only. Counterpoint\'s f_o_c.mod sample 15 ("for more MODs in the", 23028 samples) at 1.782x the rate, i.e. pitched 10 semitones (correlation 0.89). The name agrees; the audio match is below the bar.'
NOTE['0x113660#6'] = 'Same recording as the common Protracker sample in "road to the city" (MMB) and "legoukon painajainen" (Bloody), and "STYLEBLE.MOD" on Sounds Terrific: correlation 0.971 over the full length. The composer moved the loop (1733+857 instead of 3058+288) and edited about 400 of its 3348 bytes. Not the Roland SC-55/SC-88 "Slap Bass 2" (SC-88 note tested: 0.58).'
NOTE['0x113660#5'] = 'Mellow-D\'s own FT2 module "nouveau monde" (Modland), instrument 1, same length, correlation 0.990. "@mellow-d" is the musician\'s credit. Not a Juno/D-50 sound (D-550 and D-20 notes tested: best 0.60).'
NOTE['0x164b60#3'] = 'Uncle Ben\'s Protracker module "exchange" (Modland), sample 3, byte-identical. The three Strip Mine names ("exchange" by gt, uncle ben/gigatron, from the demo) are that module\'s credit lines.'
NOTE['0x164b60#6'] = 'Uncle Ben\'s "exchange", sample 1, byte-identical (see "exchange" by gt).'
NOTE['0x164b60#7'] = 'Uncle Ben\'s "exchange", sample 2, byte-identical (see "exchange" by gt).'
NOTE['0xebc00#12'] = 'PROBABLE only. "teknicida" by Atlasz (Modland FT2), instrument 16, at 1.78x the rate (correlation 0.91). Below the bar.'
NOTE['0x12eab0#0'] = 'NOT FOUND anywhere searched.'
for k in range(9, 15): NOTE['0x12eab0#%d' % k] = 'NOT FOUND. One of the seven "NoName" samples, see #8.'

def fmt_src(e, fz=False):
    s = e['src']
    if fz:
        r = e['ratio']
        how = 'same rate' if abs(r - 1) < 0.01 else ('%.3fx the rate' % r)
        return '%s (%s, %d-bit, correlation %.3f)' % (s, how, e['bits'], e['corr'])
    return '%s (%s)' % (s, e['kind'])

def verdict(key, v):
    if key in USED: return 'better copy, used'
    if key == '0x164b60#2': return 'longer copy, not used'
    if key in ('0x149c80#3', '0x149c80#4', '0x149c80#5'): return 'composer-made'
    if NOTE.get(key,'').startswith('PROBABLE'): return 'probable only'
    if NOTE.get(key,'').startswith('NOT FOUND'): return 'NOT FOUND'
    if NOTE.get(key,'').startswith('Same recording'): return 'same recording, altered'
    if v['exact']: return 'exact source'
    fz = [f for f in v['fuzzy'] if f['corr'] >= 0.97]
    if fz: return 'same recording, altered'
    if v['fuzzy']: return 'probable only'
    return 'NOT FOUND'

lines = []
w = lines.append
w('# Top Gear Rally (N64) soundtrack: where every sample came from')
w('')
w('This records the source of every sample in the six music modules of the N64 ROM, how each match was proved, and which sources are better than the ROM copy. It exists so none of this has to be worked out again.')
w('')
w('Last updated %s. %s' % (datetime.date.today().isoformat(), open('scanstatus.txt').read().strip()))
w('')
w('## Summary')
w('')
cnt = {}
for k, v in P.items():
    cnt[verdict(k, v)] = cnt.get(verdict(k, v), 0) + 1
w('The six modules hold %d samples with audio (empty slots and 2-byte placeholders are not counted).' % len(P))
w('')
w('| Verdict | Samples |')
w('|---|---|')
for name in ['exact source', 'same recording, altered', 'better copy, used', 'longer copy, not used', 'composer-made', 'probable only', 'NOT FOUND']:
    w('| %s | %d |' % (name, cnt.get(name, 0)))
w('')
w('- **exact source**: a byte-identical copy of the ROM sample exists in an older module or sample disk (listed). Same 8-bit quality, so nothing to gain.')
w('- **same recording, altered**: the same recording exists, but the ROM copy was resampled, pitched, amplified, trimmed or edited, so it is not byte-identical. Correlation 0.97 or better over the ROM sample; a correlation of 1.000 at the same rate means only the volume differs. Unless marked as used, the source is no better than the ROM copy.')
w('- **better copy, used**: the source is 16-bit, a higher rate, or both. These seven WAVs are in this folder and `package_app.sh --hq-samples` swaps them in (see the repo README, Mac port).')
w('- **composer-made**: built by the composer from another sample (chords), so only the ingredient can be sourced.')
w('- **probable only**: a likely source correlating below 0.97. Not treated as proven.')
w('')
w('## Settled verdicts (do not relitigate)')
w('')
w('- **The modules are Barry Leitch\'s.** Mod Archive `tgrtit.xm`, `tgr3.xm`, `tgr4.xm` and `tgr5.xm` (and Modland `Fasttracker 2/Barry Leitch/tgr*.xm`) are byte-identical to four of the ROM modules; `tgr1.xm` and `tgr2.xm` differ slightly. These are rips of this game, so they are never counted as sources. `tgrjtit.xm` is a 14-channel variant of the title.')
w('- **Most samples are borrowed.** They come from late 80s and 90s Amiga and PC tracker modules and the Amiga ST-XX sample disks. The sample names are often text lines from the donor module (musicians wrote messages in the sample-name slots), which is how many donors were found: "for more MODs in the / near future!!!!" is Counterpoint\'s f_o_c.mod, "this piece is 100% of / my mind... / all samples by me" is dazzler.mod, "also brimble for" is Allister Brimble\'s Project-X, and so on.')
w('- **Almost nothing better exists.** The donors are themselves 8-bit Amiga-era material. Only the seven samples marked "better copy, used" have a better copy anywhere searched.')
w('- **XXBASS.WAV in x4songa.it is not the xxbass sample**, despite the name (correlation 0.20).')
w('- **The three .iff chords (037, 049, 058) are chords of ST-58 rsstring**, named after their semitone offsets.')
w('- **grandpiano3/4 are ST-86 grandpiano3/4 pitched up 3 and 4 semitones**, then amplified.')
w('')
w('## Where was searched')
w('')
w('- The Mod Archive (modarchive.org): instrument-text search on every sample name, Barry Leitch\'s artist page (40 modules), all ten Twisted Edge Snowboarding modules, and about 110 donor candidates, downloaded and compared.')
w('- The Amiga ST-XX sample disks, 10,555 samples (archive.org item AmigaSTXX, ST-XX.zip).')
w('- Modland (ftp.modland.com): every Barry Leitch module in a sample-based format (216 files), then a full scan of its Protracker, Soundtracker, Screamtracker 3, Multitracker, Composer 669, Ultratracker, Fasttracker 2 and Impulsetracker folders (164,749 modules, about 74 GB, streamed and not kept).')
w('')
w('## Also checked, no match')
w('')
w('- **Roland JV-1080 wave ROMs** (all 8 MB, decoded: 16-bit word scrambling per the emuscd project, then Roland FCE delta decoding). Best score of any game sample 0.87 against a chance level of 0.57. Barry Leitch has said the JV-1080, Kurzweil K2500 and Korg Trinity were used for the synth track of the intro video, not the modules.')
w('- **Korg Trinity sample data** (the four KSCSNDRAW banks and 32 MB of 8-bit PCM in the TRINITY v1.1.4 installer). Best 0.927, the 909 kick, which is a similar 909 kick and not the recording (its real source, permian.xm, matches 1.00); chance level 0.60.')
w('- **Korg TR-Rack notes** from freewavesamples.com (86 files, rendered through presets): best 0.91 against chance 0.52.')
w('- **Roland SC-88 Slap Bass, D-550 and D-20 notes** from freewavesamples.com: no match to Slapbas2 or @mellow-d (0.58 and 0.60). There are no Juno-106, Alpha Juno or D-50 samples on that site.')
w('- **Amiga sample CDs**: Sounds Terrific I (1994) and II (1996) by Weird Science, Da Capo Vol. 1, and every archive in Aminet mods/smpl, mods/inst and mods/instr (76,590 files). They repeat known sources and add no new ones.')
w('')
w('## Candidate upgrades rejected')
w('')
w('The full scan flagged 1,163 copies at a higher rate or in 16-bit that match a game sample at 0.97 or better. None beat the seven in use. The tests that rejected them:')
w('')
w('- **16-bit files holding 8-bit data.** A genuine 16-bit sample has thousands of distinct values (the 909 in use: 6,057). Copies in Pro-XeX, DJ AIL, DJ Keen, Patosz, Draygen, Matley and similar modules have 1 to about 300: 8-bit samples rescaled or padded into 16-bit. DJ AIL\'s copy of "too much homework" is the game\'s own bytes scaled (residual -167 dB).')
w('- **Upsampled copies.** Badliz\'s 16-bit copies at twice the rate have every other sample exactly on an 8-bit step. The 3.5x "near future!!!!" in Phoenix\'s nameless.it has only interpolation-level content above the original band (-22 dB), and the game\'s copy is byte-identical to Counterpoint\'s 1993 original, so it is an upsample of that.')
w('- **Same data, no gain.** Many higher-rate 8-bit copies (Michael K. Berg in Lord Mystic\'s modules, the pianos in Falcon\'s death-box and Sounds Terrific\'s PIANO5.SND) are the same as the copy already in use.')
w('')
w('The seven in use pass all of these. The 909\'s gain is bit depth only: its doubled rate adds nothing above the original band.')
w('')
w('Modland files the sample reader could not parse (81, mostly unusual IT and XM variants) were not checked.')
w('')
w('## How a match is proved')
w('')
w('1. **Exact**: the ROM sample\'s bytes occur in the source sample (16-bit sources compared by their top 8 bits).')
w('2. **Same recording at another rate or pitch**: the source is resampled by the length ratio (or a semitone grid) and cross-correlated with the ROM sample, normalised. 0.97 or better over the whole ROM sample is treated as the same recording. The bulk scan pre-filters with a 48-band loudness envelope and the zero-crossing count (both unchanged by resampling) before correlating.')
w('3. **Bit-exact derivation** where possible: search simple converters (nearest, linear, averaging; gain; rounding) for one that rebuilds the ROM bytes. Only the Rave Opera string rebuilds exactly; the rest were resampled with filtering or amplified into clipping, which cannot be undone.')
w('')
w('The scripts are in `tools/` beside this file (not part of the repo): `modsamp.py` reads MOD, XM, S3M and IT samples (including IT 2.14/2.15 compressed samples), `match2.py` and `verify.py` do the correlation and converter search, `scan.py` is the streaming Modland scanner, `consolidate.py` and `writedoc.py` build this file. `tools/data/` keeps every scan\'s results, so the file can be rebuilt without searching again: run the two scripts from `tools/data/` with `tools/` on the Python path.')
w('')
w('## Every sample')
w('')
for mo, (piece, fn) in MOD.items():
    keys = [k for k in P if k.startswith(mo + '#')]
    keys.sort(key=lambda k: int(k.split('#')[1]))
    w('### %s piece (%s)' % (piece, fn))
    w('')
    w('| # | Sample name | Length | Verdict | Sources |')
    w('|---|---|---|---|---|')
    for k in keys:
        v = P[k]; i = int(k.split('#')[1])
        name = v['name'].split(' / ', 1)[-1].strip() or v['name'].split(' / ')[0].strip() or '(unnamed)'
        srcs = []
        seen = set()
        for e in v['exact']:
            if e['src'] in seen: continue
            seen.add(e['src']); srcs.append(fmt_src(e))
        for e in sorted(v['fuzzy'], key=lambda e: -e['corr']):
            if e['src'] in seen or e['corr'] < 0.9: continue
            seen.add(e['src']); srcs.append(fmt_src(e, True))
        more = ''
        if len(srcs) > 4: more = '; and %d more' % (len(srcs) - 4)
        cell = '; '.join(srcs[:4]) + more if srcs else ''
        if k in NOTE: cell = (NOTE[k] + (' Other copies: ' + cell if cell and not NOTE[k].startswith('NOT FOUND') else '')).strip()
        if not cell: cell = 'none found'
        w('| %d | %s | %d | %s | %s |' % (i, name.replace('|', '/'), v['len'], verdict(k, v), cell.replace('|', '/')))
    w('')
open(OUT, 'w').write('\n'.join(lines) + '\n')
print('wrote', OUT, cnt)
