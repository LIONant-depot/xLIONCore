# xLIONCore

The engine's core shared library (`LIONCore.dll`) - the ECS plus whatever other foundational game
systems (physics, etc.) get added as siblings to xECSV2 here over time. `xECSV2` itself stays a pure
ECS source depot; xLIONCore is what compiles it (and, later, other engine-system depots) into one
DLL that xLION and a future Game.dll both link against and share the same type registry through.

Formalizes the engine's 3-DLL split: Game.dll (scripting), xLIONCore/LIONCore.dll (ECS + game
systems), and a future Graphics.dll (rendering systems only - render component types live here too).

## Layout

- `Build/dependency/CMakeLists.txt` - consumed automatically when another project (xLION) fetches
  this depot via `FetchAndPopulate`, same as every other LIONant depot.
- `CMakeLists.txt` (this folder's root) - standalone buildable project, generates its own solution
  the same way `dependencies/xGPU` does, for building/testing xLIONCore on its own.
