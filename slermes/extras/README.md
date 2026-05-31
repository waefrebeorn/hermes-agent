# extras/ — Forward-Thinking C Toolchest

Standalone C99 image processing tools that **you** (the AI agent) can use
at build time, and that **slermes** can compile in as internal utilities.
Zero runtime deps beyond libc + libm. Image loading via `stb_image.h`.

## Tools

| Tool | What | CLI | Output |
|------|------|-----|--------|
| **imggrid** | Grid-detect multi-color SVG (pixel art) | `./imggrid in.jpg out.svg 20 26 10` | SVG file |
| **img2svg** | Contour-trace bitmap to SVG vector | `./img2svg in.jpg out.svg --eyes --crop` | SVG file |
| **palette** | Extract dominant color palette | `./palette in.jpg 12` | JSON to stdout |
| **asciimg** | Convert image to ASCII art | `./asciimg in.jpg 80 block --invert` | ASCII to stdout |
| **morph** | Edge detection & mask operations | `./morph in.jpg edges out.pgm` | PGM file |

## Build

```bash
cd slermes/extras
make          # builds all
make test     # builds + runs on slermes.jpg
make clean
```

`make test` runs every tool on `slermes/assets/slermes.jpg` and verifies
non-empty output.

## Slermes Integration

Each tool is a standalone `.c` file with no global state and no `main()`
dependency pattern. To integrate into slermes:

- **img2svg** → `src/tools/img2svg.c` — tool `image_to_svg` for the agent
- **palette** → `src/tools/palette.c` — tool `image_palette`
- **asciimg** → already partially in `src/tools/vision.c` — replace with C version
- **morph** → `lib/libmorph/` — library module for image morphology

## Architecture

```
extras/
├── README.md         ← this file
├── Makefile          ← builds all tools
├── common.h          ← shared: color math, vec2, contour trace, DP simplify, SVG writer
├── stb_image.h       ← single-header image loader (public domain, by Sean Barrett)
├── imggrid.c         ← grid-based multi-color SVG (pixel art style)
├── img2svg.c         ← contour-trace SVG pipeline
├── palette.c         ← color palette extraction
├── asciimg.c         ← ASCII art converter
└── morph.c           ← edge detect + mask operations
```

## Adding New Extras

1. Create `your-tool.c` — single file, only deps are `common.h` and `stb_image.h`
2. Add to `Makefile` — pattern: `$(CC) $(CFLAGS) your-tool.c $(LDLIBS) -o your-tool`
3. Add to `test:` target
4. Update this README
