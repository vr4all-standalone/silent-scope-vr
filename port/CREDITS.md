# Source provenance

## Current Dreamcast release

Silent Scope VR 0.0.68 uses Flycast, based on mikermak's VR fork at commit
`8398fead532ede9d52c37addef3353611b6c5ce7`. The matching engine, dependency
sources, licenses and notices accompany the named complete-source release asset.
The engine's GNU GPL v2 license is preserved. Source-file headers identify
additional licenses and copyright holders.

The Arcade cabinet rifle and AWP model were supplied by the project maintainer
and converted into the local rendering meshes. The model headers preserve the
exact geometry used by the release. Controller model attribution and their
CC BY 4.0 license links are in `assets/models/CREDITS.txt`. Roboto remains under
the SIL Open Font License included with the font. The launch instruction pictures
are rendered from these local rifle and controller models.

Konami owns Silent Scope and the original promotional artwork. Cover-image
provenance is recorded in `docs/github/ARTWORK.md`.

The sections below record the earlier arcade research and shared-code history.

The generic purple panel/font drawing support in `native/shared/xr_panel.*`, `xr_theme.h`, `ui_compat.*` and `types.h` was individually adapted from the VR4ALL Teraburst/Flycast shared support (Copyright 2026 mikermak, GPL-2.0-or-later). Silent Scope menu rows, input policy, pagination and camera controls are separate adapters. The bundled Roboto Bold font is Copyright 2011 The Roboto Project Authors, licensed under SIL Open Font License 1.1; its complete license is in `assets/fonts/OFL.txt`. Vendored stb_image and Dear ImGui's stb_truetype retain their license notices in their headers.

Original Silent Scope adapter, input policies, frame bridge, scoped rifle mesh and rendering: VR4ALL development, 2026, GPL-2.0-or-later. The rifle geometry is procedural original work; it contains no extracted game model or textures.

The generic `native/gl_context.h` and `.cpp` were adapted from the existing VR4ALL Teraburst host, changing only namespace and window names. OpenXR API call patterns were consulted from that host; this port's controls, frame ownership and rifle presentation are separate. Existing Teraburst production files were read only.

Generic Voodoo triangle/material/texture capture, immutable snapshots, GPU combine shaders and scene replay support under `native/scene/` were individually adapted from VR4ALL Teraburst, with namespaces changed. Silent Scope camera signatures, native lens extraction, scene composition, main/scope ownership, scale and rifle input remain game-specific. No Teraburst game RAM offsets, reticle extraction, controls or menus are used. Shader combine rules follow MAME's BSD-licensed Voodoo renderer by Aaron Giles. Android SDL/Gradle support and launcher patterns also use individually audited shared VR4ALL support; the original provider source trees were unchanged.

The engine is MAME 0.289 commit `f34f02505e32c1993c6a782b6814232cbfc74e36`; upstream source, notices and per-file licenses remain intact in the dedicated compiler checkout. The Hornet driver is BSD-3-Clause, by Ville Linde; MAME as a whole includes GPL and BSD components. Preserve complete matching source and notices before redistributing a binary.

The live internal-SRAM read decoder follows the pinned MAME ADSP21062 core's address maps and48-bit packing (Ville Linde, BSD-3-Clause). It changes no emulated clocks, writes, MMIO or short-word/SSE handling. The original dispatcher remains available for every-read verification and diagnostics.

Vendored generic dependencies were copied individually from the existing VR4ALL source-support tree: GLM (`vendor/glm/copying.txt`), OpenXR headers (`vendor/openxr/LICENSE`, Apache-2.0), and the generated GLAD OpenGL/GLES loader with its generated header/license comments. No native game media, calibration/NVRAM or signing keys belong in a source or binary package.
