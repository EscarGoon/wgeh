# Inspo Generator v6

A decoration engine for the Geometry Dash editor (Geode mod). It decorates your actual layout, builds animated structures, and rolls full backgrounds. Everything is made of normal objects and triggers, so you can edit or take apart anything it makes.

## Editor toolbar

| Button | What it does |
|---|---|
| **Decorate** | Decorates the layout you selected. Opens a window to pick style, colors, detail and options |
| **Inspire** | Generates animated structures + a background around the camera |
| **BG Dice** | Rolls a random background around the camera |
| **Reroll** | New shapes for the last batch, same colors |
| **Recolor** | Same shapes, new colors |
| **< > ^ v** | Move the last batch |
| **Delete** | Remove the last batch (and un-hide the layout it decorated) |
| **...** | Settings |

## Decorating a layout
1. Swipe-select the part of your layout you want decorated (blocks and spikes).
2. Press **Decorate**.
3. Pick a **Style**, **Colors** (Auto picks colors that suit the style), **Detail** and **Motion**, and the toggles:
   - **Outer glow**: soft glow around the platforms
   - **Edge details**: vines, icicles, crystals, cables, banners, drips, torches…
   - **Spike glow**: glow behind selected spikes
   - **Hide layout in game**: your layout blocks stay visible and solid in the editor, but are hidden in-game so only the decoration shows
4. Press **Decorate**, or **Dice** for a random style and colors.

The mod reads the exact shape of the selection: outlines, inner insets, corners, tops, undersides and walls. Each style decorates those parts differently:

| Style | Look |
|---|---|
| **Cyber** | Futuristic tech glow built on the panel engine: every platform is split into machined panels (bevels, seams, chamfers, screws, brackets) filled with detailed modules, then wrapped in stacked glowing outlines, inner glow, recessed light slots, corner brackets, runway lights, spinning half-gears and hanging fins, all pulsing to the beat |
| **Industrial** | Heavy sci-fi industry: riveted plates, hazard-stripe bands, fans, pipes and valves, vents, warning beacons, hanging pipework (amber "hazard" palette) |
| **Nine Circles** | Concentric bars around every shape and nested triangles around spikes that switch on and off in sequence (inward or outward) to the beat, nested diamonds, zigzag teeth, strobing accent colors |
| Neon | Dark fill, bright outlines with a travelling light wave, inset lines, corner gems, glow dots, neon drips |
| Tech | Panels with seams, beveled edges, glowing circuit traces, chips, rivets, sagging cables |
| Crystal | Faceted gem interior, glowing insets, crystal clusters on top, hanging shards, sparkles |
| Castle | Brickwork with varied bricks, capstones, corbels, banners, flickering wall torches |
| Nature | Layered soil, grass and flowers, moss, hanging vines with glowing berries, fireflies |
| Void | Minimal black shapes with rippling concentric outlines and drifting squares |
| Lava | Cooled rock with glowing cracks, hot rims, drips and rising embers |
| Ice | Frosted ice with cracks, shine streaks, snow caps, icicles and glints |

### The panel engine
Cyber, Tech, Industrial, Neon and Nine Circles split every platform into machined panels and fill them with modules from a library of 17:
ports, spinning fans, screens with a moving scan line, power cells, chips, vents with glow, hazard stripes, light grids, hex patches, pipes and valves, chevron rows, equalizers, glowing cores, trusses, data bars, light strips and nested diamonds. Large panels get nested sub-panels. Thin 1-block platforms get light bars, chevrons and end caps instead. Lights blink in sequence and chase along edges to the beat.

v6 adds 8 more modules: 7-segment digit displays, gauges with swinging needles, spinning radar sweeps, speaker grilles, cooling fins, warning signs, barcodes and capacitor banks (25 in total).

### Breaking the silhouette
With **Edge details** on, Cyber, Tech, Industrial and Neon also add:
- **Armor shoulders**: angled plates wrapping convex corners, or a single end cap on thin platforms
- **Wall plates**: raised, vented armor on walls
- **Thrusters**: nozzles under floating platforms with flickering exhaust plumes
- **Support trusses**: dark lattice supports behind floating platforms on the B2 layer, with blinking lights
- **Seam conduits**: glowing lines in the gaps between panels, pulsing along the platform

Decoration sits on the B1 layer (supports on B2), so it's always behind the player.

## Backgrounds
**BG Dice** (and Inspire) pick from 15 background types:
- **space**: rolls one of the three below
- **planets**: banded ringed gas giants with storms, shadowed sides, atmosphere glow and front/back rings, plus moons
- **blackhole**: event horizon, wide accretion disk and lensed light wrapping around it, pulsing to the beat
- **eclipse**: black sun with a spinning corona that flashes to the beat
- **warp**: hyperspace tunnel, with nested frames rushing outward to the beat, radial light streaks and a blazing vanishing point
- **circles**: concentric rings lit in sequence, spinning saw rings, radial burst, rotating triangles
- **cyber**: megastructure skyline with lit windows, blinking antennas, holo rings, hyperspeed streaks, searchlights
- **night**, **synthwave**, **nebula**, **aurora**, **abyss** (underwater), **geometric**, **sunset**, **clouds**

All space types add streaky nebulae, dense twinkling stars, shooting stars and an asteroid belt. Each has its own set pieces (striped sun and perspective grid, spiral galaxy, aurora ribbons, light rays and swaying kelp, spinning shapes, layered forests, drifting clouds) plus parallax layers. Choose a fixed type in settings, or leave it on Random.

## Structures (Inspire)
**Futuristic:** reactor (hex housing, spinning containment rings, energy pylons, pulsing core), cyber tower (chamfered segments with machined module panels on every face, data streams, chevrons, fins, beacon, halo rings, holo panels), space station (spinning habitat ring, trusses, solar arrays, modules, nav lights) and a giant blade (bevelled blade, glowing rune fuller, gem guard, aura).
**Classic:** citadel, crystals, floating island, mech core, portal, orbital, tree, colonnade.
Set "Structure type" to *futuristic* or *classic* to roll only from that set.

## Beat sync
Set **Song BPM** in settings. Outline flashes, glow swells, the Nine Circles sequence, rune chases, beacons and the eclipse all run on that tempo.

## Colors
Scenes use channels 970–992 and decorations use 940–962 (both configurable), so they don't overwrite each other. Group IDs always start above the highest group already used in your level.

## Building
Push this folder to a GitHub repo; the included workflow builds it. Download the `.geode` from the run's **Build Output** artifact and put it in `geode/mods`. Targets Geode 5.11 / GD 2.2081.

## Notes
- **Reroll / Recolor** after Decorate re-decorate the same selection.
- **Delete** after Decorate removes the decoration and makes the layout visible in-game again.
- Very large selections produce many objects. Use detail 1 for big areas.
- If decoration shapes look textured, change "Solid square object ID" in settings.

## tools/
`preview.cpp`, `preview_deco.cpp` and `render.py` run the generators outside the game and render PNG previews, for tweaking without launching GD.

## Themes
cyber (blue/black/white), voltage (purple + cyan), crimson (red/black), frost (cold blue/purple), hazard (gunmetal + amber), neon, sunset, ice, toxic, lava, mono, cosmic, royal, sakura, abyss.
