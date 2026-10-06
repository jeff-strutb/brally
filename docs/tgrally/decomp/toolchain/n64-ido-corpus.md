# N64 ido corpus

> Local ROM-confirmed IDO 5.3 corpus from SM64/GoldenEye/Banjo/MK64 decomps (12,894 fns, ~9,800 at -O2) with a shape query tool; build/tgrally/ext/n64corpus

Built 2026-10-04 in `build/tgrally/ext/n64corpus/` (git-ignored; never copy their C into the repo, same rule as the PC corpus [corpus-query-tool](../../../brally/decomp/corpus/corpus-query-tool.md)).

- Repos (shallow clones): sm64, 007, banjo-kazooie (+ lib/ultralib submodule), mk64. Also cloned but NOT useful: sf64 and pokemonsnap (game code IDO 7.1), sk2 (Snowboard Kids 2 = KMC GCC).
- the project lead's No-Intro ROMs sit in each game folder (USA; Banjo USA 1.0), all SHA1-verified.
- `build_corpus.py`: compiles each repo's .c with OUR tools/toolchains/ido53/cc over several flag sets; keeps functions whose relocation-masked words are found in that game's ROM (Banjo: rarezip 0x1172 blocks inflated first). Output corpus.jsonl (words, mask, flags, C body). Counts: banjo 7195, sm64 2968 (mostly -g, little use), 007 1910, mk64 821.
- `query.py VA [--at OFF --len N] [--k 5] [--opt -O2]`: register-class/reloc-normalised instruction shape, k-gram index + longest common run, prints the C. Plain grep over corpus C (by API names) is also effective.
- Measured so far: no corpus cross product has BrVec3Cross's stack shape; GoldenEye joyRumblePakInit shape did not fix BrSchedInit.
