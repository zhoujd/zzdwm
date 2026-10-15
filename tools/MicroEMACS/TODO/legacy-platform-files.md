# Legacy Platform Files

Status: implemented.

Obsolete platform-specific sources are kept under `sys/legacy/` rather than
mixed with active platform implementations. This preserves historical code
without suggesting that current builds use it.

## BSD and VAX Memory Helpers

The unused `bcopy` family was moved to `sys/legacy/vax/`:

- `bcopy-bsd.c` — the former top-level BSD-compatible C fallback for
  `bcopyr` and `bfill`.
- `bcopy-vax.c` — the former `sys/unix/bcopy.c` Ultrix-32 C implementation.
- `bcopy-vax.s` — the former `sys/unix/bcopy.s` VAX assembly implementation
  using `movc3` and `movc5`.

These routines predate portable use of `memmove`, `memcpy`, and `memset`.
Current MicroEMACS code calls the standard C memory functions directly. The
legacy files are intentionally absent from `Make.Linux` and `Make.Mingw`, so
they do not affect Linux or Windows builds.

The BSD C fallback is grouped with the VAX sources because it belongs to the
same obsolete `bcopy` compatibility family and has no active build target.
