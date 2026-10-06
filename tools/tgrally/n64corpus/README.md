# N64 IDO corpus

`build_corpus.py` builds the local IDO 5.3 corpus of ROM-confirmed functions
from the public N64 decompilations (compiled with our own `tools/toolchains/ido53`),
and `query.py` looks up a shape in it. The corpus itself is generated under
`build/tgrally/ext/n64corpus/`. See docs/tgrally/decomp/toolchain/n64-ido-corpus.md.
