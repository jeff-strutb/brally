#!/usr/bin/env python3
"""Retired. Filed matches into sliceN_MM.c by address — that is how 570
functions were stranded. A match is born in its module (CLAUDE.md rule 6).
See docs/archive/incidents.md."""
import sys
sys.stderr.write(
    'autofile.py is retired: it filed matches into sliceN_MM.c by address.\n'
    'Transcribe into the module that owns the neighbours; the pre-commit hook\n'
    'refuses a new VA in an address batch. See docs/MATCHING.md.\n'
)
sys.exit(2)
