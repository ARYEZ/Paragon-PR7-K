# dmc_unrar

A dependency-free, single-file RAR reader by Sven Hesse (DrMcCoy), licensed GPL-2.0-or-later (see `COPYING`), taken
here under GPL-3.0, which PR7-K's AGPL-3.0 may be combined with.

- Source: https://github.com/DrMcCoy/dmc_unrar
- Commit: ab8bbddfb84f4b5853c8184be84a8c2565f4a4bb (2026-09-05), `dmc_unrar.c` unchanged.

PR7-K uses it (1.2.0) to unpack sample libraries their makers publish as RAR archives (Unreal Instruments' koto),
in `libraries::extractRar` (SampleLibraries.cpp). Checked on that archive: all 773 files unpacked byte for byte as
libarchive's bsdtar does, plus a folder with a Japanese name bsdtar couldn't convert.
