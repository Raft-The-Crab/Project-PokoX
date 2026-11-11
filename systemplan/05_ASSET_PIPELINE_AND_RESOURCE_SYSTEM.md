# 05 — Asset Pipeline and Resource System

Support meshes, textures, materials, shaders, audio, fonts, animation, skeletons, UI and scene data.

## Pipeline
Import, validate, convert to runtime representation, generate previews, record dependencies, hash outputs and support incremental rebuilds.

## Runtime
Asynchronous loading, resource handles, lifetime rules, caching/eviction, streaming where needed, versioned metadata and missing-resource fallbacks.

Use glTF as a planned interchange format. Verify importer licenses. Build a CLI asset compiler before a Studio browser.

## Exit gate
A sample project imports, validates, converts, packages and loads representative assets with reproducible outputs and useful errors.
