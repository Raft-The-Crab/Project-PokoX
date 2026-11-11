# 02 — Repository and Build Foundation

## Repository layout proposal
```text
/pokox
  /docs
  /engine/{core,graphics,assets,physics,audio,animation,input}
  /runtime/{game,mute,network,server}
  /tools/{assetc,packagec,serverctl,debugger}
  /apps/{player,studio,mod}
  /backend/{workers,services}
  /tests/{unit,integration,fuzz,performance}
  /third_party
```

Use CMake for C++20 targets. Separate Debug, Development, Release and Server configurations. Pin dependencies, inventory licenses, run formatting/static analysis and build through CI.

## Targets
Engine libraries, windowed demo, headless runtime, asset compiler, package builder/verifier, dedicated server, test executables and debug tools.

## Exit gate
A clean checkout builds using documented steps; CI runs tests; no secrets exist in repository history; development and release flags are distinct; minimal windowed and headless demos run.
