# Filament ANARI — Feature Roadmap

Milestones are ordered by ANARI SDK examples, progressing from basic tutorials
to full Blender addon support. Each milestone lists the ANARI features required
and their Filament mapping.

## Legend

- **Direct** — maps directly to a Filament API
- **Generate** — requires mesh/data generation at the ANARI device level
- **Shim** — approximated via a different Filament feature
- **Unsupported** — cannot be implemented through Filament

---

## Milestone 0 — Device Skeleton (done)

Bare `ANARIDevice` that loads, creates a frame, and returns status callbacks.
CI on all platforms.

| Feature | Status |
|---------|--------|
| `anariNewDevice` / `anariRelease` | Done |
| Status callback | Done |
| Frame (stub) | Done |

---

## Milestone 1 — Tutorial (`anariTutorial.c`) ✅ Done

Render a single triangle with a directional light and perspective camera.
This is the "hello world" of ANARI.

| ANARI Feature | Filament Mapping | Notes |
|---------------|-----------------|-------|
| `triangle` geometry | **Direct** — `RenderableManager` with `TRIANGLES` | Vertex positions + indices |
| `matte` material | **Direct** — `Material` (UNLIT or LIT) | `color` param → baseColor |
| `perspective` camera | **Direct** — `Camera::setProjection` | fovy, aspect, near, far |
| `directional` light | **Direct** — `LightManager::Builder(DIRECTIONAL)` | direction, color, intensity |
| `world` | **Direct** — `Scene` | Container for surfaces and lights |
| `surface` = geometry + material | **Direct** — `Renderable` entity | |
| `instance` / `group` | **Direct** — `TransformManager` | 4×4 transform |
| `frame` with `color` channel | **Direct** — `Renderer::render` → `readPixels` | FLOAT32_VEC4 / UFIXED8_VEC4 |
| `ARRAY1D` / `ARRAY2D` | Direct — host memory wrappers | Position, index, color arrays |
| `renderer` (default) | **Direct** — `Renderer` + `View` | |

### ANARI Parameters to Handle

- `anariSetParameter` for float, int, float-vec, string types
- `anariCommitParameters` → build/rebuild Filament objects
- `anariRenderFrame` / `anariFrameReady` / `anariMapFrame`

---

## Milestone 2 — Basic Test Scenes ✅ Done

Support enough features to render the simpler ANARI SDK test scenes.

### 2a — `textured_cube`

| ANARI Feature | Filament Mapping | Notes |
|---------------|-----------------|-------|
| `triangle` geometry with normals | **Direct** | Add normal vertex attribute |
| `matte` material with `image2D` sampler | **Direct** — `Texture` + `TextureSampler` | UV coordinates via `attribute0` |
| `image2D` sampler | **Direct** — `Texture::setImage` | 2D texture from pixel data |

### 2b — `random_spheres`

| ANARI Feature | Filament Mapping | Notes |
|---------------|-----------------|-------|
| `sphere` geometry | **Generate** — tessellate to triangle mesh | Icosphere or UV-sphere generation |
| Per-sphere radius | **Generate** — scale during mesh generation | |
| Per-primitive color | **Direct** — vertex colors or per-instance | |

### 2c — `pbr_spheres`

| ANARI Feature | Filament Mapping | Notes |
|---------------|-----------------|-------|
| `physicallyBased` material | **Direct** — Filament LIT material | baseColor, metallic, roughness, ior |
| Multiple `directional` lights | **Direct** — multiple `LightManager` entities | Filament supports many lights |

### 2d — `instanced_cubes`

| ANARI Feature | Filament Mapping | Notes |
|---------------|-----------------|-------|
| `group` with multiple surfaces | **Direct** — multiple renderables | |
| `instance` array with transforms | **Direct** — `TransformManager` per instance | One entity per instance |

---

## Milestone 3 — Remaining Test Scenes ✅ Done

### 3a — `random_cylinders`

| ANARI Feature | Filament Mapping | Notes |
|---------------|-----------------|-------|
| `cylinder` geometry | **Generate** — tessellate to triangle mesh | Capped cylinder generation |
| Per-cylinder radius | **Generate** — parameterized generation | |

### 3b — `cornell_box`

| ANARI Feature | Filament Mapping | Notes |
|---------------|-----------------|-------|
| `quad` light | **Shim** — area light approximated as point/spot | No native area lights in Filament |
| Multiple geometry in one scene | **Direct** | Already supported by Milestone 1 |

### 3c — `attributes` (all sampler types)

| ANARI Feature | Filament Mapping | Notes |
|---------------|-----------------|-------|
| `image1D` sampler | **Direct** — 1D texture lookup (or 1×N 2D) | May use 2D texture with height=1 |
| `image3D` sampler | **Direct** — `Texture::Builder(SAMPLER_3D)` | Filament supports 3D textures |
| `transform` sampler | **Direct** — computed on CPU | Apply 4×4 transform to attribute |
| `primitive` sampler | **Direct** — per-face attribute expansion | Expand to per-vertex |

---

## Milestone 4 — ANARI Viewer (subset) ✅ Done

Support the features needed for the interactive anariViewer application.

| ANARI Feature | Filament Mapping | Notes |
|---------------|-----------------|-------|
| `orthographic` camera | **Direct** — `Camera::setProjection(ortho)` | |
| Dynamic parameter changes | **Direct** — rebuild Filament objects | Respond to `anariCommit` |
| Multiple renderers / frames | **Direct** — multiple `View`/`Renderer` | |
| Object introspection | Direct — `anariGetProperty` | Query bounds, extensions |
| Frame `depth` channel | **Direct** — depth buffer readback | `FLOAT32` depth |

---

## Milestone 5 — Full Geometry, Introspection & Screenshot Tests ✅ Done

| ANARI Feature | Filament Mapping | Notes |
|---------------|-----------------|-------|
| `cone` geometry | **Generate** — tessellate to triangle mesh | Done (Milestone 3) |
| `curve` geometry | **Generate** — tessellate tubes | Per-vertex radii, colors, primitive.index |
| `quad` geometry | **Generate** — split each quad into 2 triangles | Done (Milestone 3) |
| `FLOAT32_VEC4` frame format | **Direct** — RGBA32F texture + float readback | For HDR / Blender |
| Frame `depth` channel (parameter) | Parsed but not readable | Filament readPixels only reads COLOR |
| `getObjectSubtypes` introspection | **Direct** — static subtype arrays | All object types |
| Screenshot comparison tests | PPM golden-image regression | 4 visual tests + float32 + introspection |

---

## Milestone 6 — Blender Addon 🔄 Planned

Full support for the [ANARI Blender addon](https://github.com/KhronosGroup/anari-blender-addon).

> **Note:** The `anari` Python package (built from the SDK) is installed and
> importable in Blender 5.1's bundled Python 3.13. The upstream
> `CustomAnariRender.py` example calls `anariLoadLibrary()` which was renamed
> to `loadLibrary()` in a newer SDK — upstream fix pending.

| ANARI Feature | Filament Mapping | Notes |
|---------------|-----------------|-------|
| `triangle` geometry with normals + UVs + vertex colors | **Direct** | All vertex attributes |
| `matte` material (Blender default) | **Direct** | |
| `perspective` + `orthographic` cameras | **Direct** | |
| `group` / `instance` hierarchy | **Direct** | Blender object transforms |
| Accumulation rendering (progressive) | **Direct** — multi-pass render | Filament TAA / custom accumulation |
| `UFIXED8_VEC4` frame format | **Direct** — `readPixels` with RGBA8 | Blender expects 8-bit |

---

## Features That Cannot Be Supported

These ANARI features have no equivalent in Filament's rasterization pipeline.

| ANARI Feature | Reason | Possible Workaround |
|---------------|--------|-------------------|
| `volume` object | Filament has no volumetric rendering | None — would require custom shaders or skip |
| `transferFunction1D` volume | Requires volume rendering | None |
| `structuredRegular` spatial field | Requires volume rendering | None |
| `hdri` light | No environment-map emission | **Shim** — use Filament IBL (`IndirectLight`) for ambient, but no visible HDRI dome |
| `ring` light | No native ring/area lights | **Shim** — approximate as spot light |
| `quad` light (physically accurate) | No area lights in rasterizer | **Shim** — approximate as point/spot light at centroid |
| Path tracing / global illumination | Filament is a rasterizer | Filament has SSAO, SSR as approximations |

---

## Priority Order Summary

1. **Milestone 0** — Device skeleton ✅
2. **Milestone 1** — Tutorial (triangle, matte, perspective, directional) ✅
3. **Milestone 2a** — Textured cube (textures, normals) ✅
4. **Milestone 2b** — Random spheres (sphere mesh generation) ✅
5. **Milestone 2c** — PBR spheres (physicallyBased material) ✅
6. **Milestone 2d** — Instanced cubes (instancing) ✅
7. **Milestone 3a** — Random cylinders (cylinder mesh generation) ✅
8. **Milestone 3b** — Cornell box (quad light shim) ✅
9. **Milestone 3c** — Attributes (all sampler types) ✅
10. **Milestone 4** — Viewer subset (ortho camera, depth, introspection) ✅
11. **Milestone 5** — Full geometry, introspection, screenshot tests ✅
12. **Milestone 6** — Blender addon (accumulation, full vertex attrs) 🔄
