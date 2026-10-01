# Arena preview

Compiles and **runs** `AHArena.cpp` outside Unreal, against a small stand-in for
the slice of the engine it uses, and prints the valley it generates as a map.

It exists because the two failures that keep costing a whole round trip are a
type-deduction error MSVC only reports at build time, and a layout bug you can
only see by looking at the map. Both are cheap to catch here and expensive to
catch in the editor.

```
g++ -std=c++17 -I. -Wall -Wextra -Wshadow -Wno-unused-parameter \
    -o harness harness.cpp ../../Source/AshenHollow/Private/AHArena.cpp shimdefs.cpp
./harness 20000
```

`-Wshadow` is GCC's version of MSVC's C4456, which is an error in this project,
so a clean build here means the real build will not trip over a shadowed local.

The shim is faithful on the parts that bite and crude everywhere else:

* `FMath::Min/Max/Clamp/Abs` deduce one type, so mixing `float` and `double`
  fails here exactly as it fails in Unreal.
* `Atan2/Sqrt/Cos/Sin` are separate `float` and `double` overloads, so a mixed
  pair is ambiguous here exactly as it is there.
* `FloorToInt` / `RoundToInt` return `int32` for a float and `int64` for a
  double.
* `FVector` holds doubles, as it has since UE5 went large-world.
* `FRandomStream` is Unreal's own generator, number for number, so a seed here
  builds the same valley the game builds from that seed.

It is not an engine. Anything `AHArena.cpp` starts using that the shim does not
have has to be added to `CoreMinimal.h` -- which is a feature, because it keeps
the generator from quietly growing a dependency on the world.

What the harness checks, over as many seeds as you give it:

* every stretch of road is joined to the arrival cell
* every world has at least one camp
* every house stands on a street
* nothing solid lands on the arrival point
* nothing solid lands inside a camp's fighting ring
* the same seed builds the same valley twice

Non-zero exit means one of those failed. The same invariants are asserted in
`Private/Tests/AHArenaTests.cpp`, which is what runs in the editor.
