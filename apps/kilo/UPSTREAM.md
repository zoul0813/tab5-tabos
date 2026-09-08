# Kilo provenance

- Source: https://github.com/antirez/kilo
- Revision: `323d93b29bd89a2cb446de90c4ed4fea1764176e`.
- License: BSD-2-Clause, copyright 2016 Salvatore Sanfilippo.
- Imported files: `kilo.c` (unaltered review baseline in `upstream/kilo.c.txt`) and `LICENSE`.

The maintained port in `src/` adapts the upstream row-based editor, C/C++ keyword table,
stateful syntax scanner, tab-column mapping, viewport, and incremental search design.
Terminal setup, event input, bounded transactional row operations, byte-preserving
line endings, and staged storage are TabOS adaptations. The upstream baseline is
reference material, not compiled. Ordinary builds require no network access.

Apply future upstream fixes selectively; preserve TabOS service boundaries and fault
handling. Keep this file and the BSD license with distributed binary materials.
