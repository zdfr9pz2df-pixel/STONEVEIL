# STONEVEIL rendering foundation

This document records the first renderer-modernization slice. The DDA raycaster remains the geometry
solver. The current work improves the data and accumulation stages that feed its wall columns and
projected floor/ceiling blocks; it does not introduce hardware ray tracing or replace the grid/editor
architecture.

## Implemented pipeline

1. `Raycaster` asks `Lighting` for one `LightingFrame` per rendered frame.
2. Authored and runtime light placements resolve through the stable light catalog.
3. Light intensity is evaluated once, including deterministic flicker or pulse animation.
4. Important lights are ranked and capped through `LightingSettings`.
5. Each evaluated light is inserted only into the map cells it can influence.
6. Shadow-casting lights cache DDA grid visibility once per affected cell.
7. Wall-column and floor/ceiling samples visit only the candidates in their cell.
8. The material catalog supplies roughness, metallic, emissive, opacity, transmission, reflection,
   wetness, and normal-strength values to the surface response.

`Dungeon::addLight`, `moveLightAt`, and `removeLightAt` are the bounded runtime mutation path. Existing
level files still store only `(x, y, stable-light-id)`, so this slice needs no level or save migration.

## Compatibility and scaling

- The centered-eye DDA origin and hit calculations are unchanged.
- Shadows remain compatible with the 2D grid: the cache is a cell visibility field, not DXR or a
  recursive ray-traced effect.
- `LightingSettings` independently controls animation, shadows, active-light limits, shadow-light
  limits, and shadow distance. These are the building blocks for later Low/Medium/High/Ultra mappings.
- Unshadowed decorative lights remain cheap; important shadowed lights consume the visibility budget.
- Wetness darkens diffuse response, lowers effective roughness, and strengthens view-dependent
  highlights. Shallow water already carries transmissive/reflective material properties, but the
  dedicated water pass is intentionally not part of this slice.

## Next compatible slices

1. Add a viewport render target and explicit depth, normal, and material-ID buffers emitted by DDA.
2. Add a small GPU post-processing layer for tone mapping and depth-aware atmospheric fog.
3. Add editor-facing quality/profile controls and per-placement light direction/overrides.
4. Add reflection probes/environment fallback, then screen-space reflections using the DDA buffers.
5. Add low-resolution volumetric integration using the cached light field.
6. Build the dedicated water pass from mechanical water cells, followed by refraction and reflection.

The later passes should read the same `MaterialSurfaceProperties`, `LightDefinition`, and
`LightingSettings`; they should not infer behavior from texture names or create parallel catalogs.
