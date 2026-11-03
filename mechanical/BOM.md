# Bill of materials: slide-in motor supports and jaw coupling

Everything needed to fit the drill motor on **one desk**: printed parts, screws and inserts, the bought coupler, the parts of the
desk that are reused, and the tools. Dimensions in mm. Assembly order: [README](README.md#assembly) and pages 11 to 16 of the report.

## 1. Printed parts

Bambu Lab A1 mini, 0.4 mm nozzle. Slicer settings of each material in [section 6](#6-slicer-settings).

| ID | Part | Qty | Material | File to print | On the bed | Walls / infill | Volume (cm3) |
|---|---|---|---|---|---|---|---|
| A | Clip plate 71 x 15 x 6, two sample-4 slots (pitch 56), two M3 counterbores | 2 | PETG Matte | `stl/clip_plates_x2.stl` (both plates) | flat, bottom face down | 5 walls, 25 % | 2 x 5.21 |
| B | Plot: shoulder dia 6.0 x 9.6, head dia 10 x 2.5, countersunk bore for a dia 3 wood screw | 4 | PETG Matte | `stl/plots_x4.stl` (all four) | head down | 4 walls (solid) | 4 x 0.34 |
| C | **TPU cushion** (silent block): ring washer dia 12 / 5.8 x 1.5, wheel pattern | 8 | TPU 85A | `stl/cushions_x8.stl` (all eight) | flat | 2 walls, 3 top / bottom | 8 x 0.09 |
| D | Ring support motor_mount: ring dia 48.5 / 54 x 15, side slots and side windows, pad with 2 insert pockets | 1 | PETG | `stl/ring_support_motor_mount_print.stl` | ring axis vertical, x = 0 rim down | 4 walls, 25 % | 5.99 |
| E | Ring support front mount: ring dia 48 / 54 x 12, four slots at 45 degrees (the 6.2 wide one is the key), pad with 2 insert pockets | 1 | PETG | `stl/ring_support_front_mount_print.stl` | ring axis vertical | 4 walls, 25 % | 5.70 |
| F | **Drill hub**, dia 33 x 37: drill adapter V4 (threaded bore for the drill spindle) and coupling hub A (3 jaws) in one part | 1 | PETG | `solidworks/DrillAdapter+hub.STL` | threaded end down, jaws up | 4 walls | 9.83 |
| G | Coupling hub B (flange side), dia 33 x 13, 3 jaws | 1 | PETG | `stl/hub_b_flange_side.stl` | base down | 4 walls | 4.89 |
| H | Coupling spider, dia 33 x 8.5 | 1 + 1 spare | TPU 85A | `stl/spider_tpu.stl` | flat, lobes up | 4 walls, 100 % | 2.58 |

Support: under the dia 33 base of F, which overhangs the dia 22 flange of the adapter by 5.4 mm, and under the dia 22.2 x 1 recess of G.
Nothing else needs support.

**Filament** (solid volume of the STL, so an upper bound: the parts at 25 % infill use less):

| Material | Parts | Volume | Mass at 100 % infill |
|---|---|---|---|
| PETG / PETG Matte | A, B, D to G | 38.2 cm3 | about 48 g |
| TPU 85A | C and H, spare spider included | 5.9 cm3 | about 7 g |

Densities assumed: PETG 1.27, TPU 1.2 g/cm3.

### Variants and files not to print

| File | Use |
|---|---|
| `stl/clip_plate_mirrored.stl` | print 2 of it **instead of A** only if your coupler is on the other side |
| `stl/cushion_solid.stl` | stiff cushion, 8 instead of C; sits on a resonance near 1 kHz, keep it as a spare |
| `stl/cushions_x8.stl` at 2 / 3 / 4 walls | 3 sets of C to find the softest cushion that holds, with the ADXL345 (README, *Silent blocks*) |
| `stl/clip_plate.stl`, `stl/plot.stl`, `stl/cushion_wheel.stl` | one part alone (A, B, C): to test a single print first |
| `stl/ring_support_*_desk_frame.stl` | D and E in the desk frame, for CAD only (not in print orientation) |
| `stl/clip_cut_tool.stl` | cutting tool to put the clip into your own SolidWorks plates, not a part |

## 2. Screws and heat-set inserts

| ID | Item | Qty | Where | Notes |
|---|---|---|---|---|
| 1 | Wood screw dia 3 x 25, countersunk, head dia 6 (DIN 7997 or chipboard screw) | 4 | plots B into the plank | pilot dia 2.0-2.2 x 14, about 13 mm in the wood. Plank under 15 mm thick: dia 3 x 20 |
| 2 | M3 heat-set insert, **short**: L 3.0 at most, for a dia 4.0 x 3.2 pocket | 4 | ring supports D and E, 2 per pad | flush with the pad face |
| 3 | M3 x 5 socket head cap screw (ISO 4762 / DIN 912) | 4 | clip plates A onto the pads | dia 5.8 x 3.2 counterbore; 0.3 N.m at most. M3 x 6 would reach the bottom of the pocket |
| 4 | M3 heat-set insert, for a dia 4.0 x 4.2 pocket (L 4.2 at most) | 4 | hub G | on the flange side |
| 5 | M3 x 6 socket head cap screw (ISO 4762 / DIN 912) | 4 | flange coupler onto hub G | through the 2 mm flange, 4 mm in the insert |

The CAD models the cap screw heads as dia 5 x 3; an ISO 4762 M3 head (dia 5.5 x 3) also fits the dia 5.8 counterbores of A.

**In total**: 8 M3 heat-set inserts (4 short + 4), 8 M3 cap screws (4 x 5, 4 x 6), 4 wood screws dia 3 x 25.

**Washers: none.** The TPU cushions C are the washers of the plots. The cap screw heads of A sit in dia 5.8 counterbores, where an M3
washer (dia 7, DIN 125) does not fit; the heads of screws 5 bear on the steel flange of the coupler; the wood screws are countersunk.

## 3. Bought part

| Item | Qty | Notes |
|---|---|---|
| Rigid flange coupler, bore 6.35: flange dia 22 x 2, boss dia 10 x 10, 4 holes dia 3 on dia 16 | 1 | AliExpress item 1005007347931821, variant B (6.35 mm). Comes with its 2 M3 set screws. Model: `step/flange_coupler_6.35mm.step` |

## 4. Parts of the desk and of the previous build (not supplied)

| Item | Notes |
|---|---|
| Drill motor (motor + gearbox), about 0.3 kg | its body goes into the rings D (dia 48.5) and E (dia 48); its spindle is screwed into the drill hub F |
| Desk crank shaft dia 6.35 (IKEA Skarsta / Trotten) | takes the flange coupler |
| Wood plank under the desk top | at least 15 mm thick. Its underside is assumed 36.0 above the shaft axis and sets the cushion thickness: measure it first (README, *Silent blocks*) |

## 5. Tools

* Soldering iron with a heat-set insert tip, about 230 degrees C.
* Drill with a dia 2.0-2.2 bit and a depth stop at 14 mm, for the 4 pilot holes.
* Hex keys 2.5 mm (M3 cap screws) and 1.5 mm (M3 set screws of the coupler); screwdriver bit for the wood screws (PZ1 or TX10, depends on the screws).
* Calipers, to check the first prints (plot dia 6.0 +/- 0.05, slot 6.6, mouth 5.4, cushion bore 5.8).
* Filament dryer: TPU 70 degrees C for 8 h, PETG Matte dried too.
* For the vibration test only: an ADXL345 accelerometer board.

## 6. Slicer settings

| Setting | PETG Matte (A, B, D, E, F, G) | TPU 85A (C, H) |
|---|---|---|
| Feeding | AMS lite or external spool | external spool only, spool raised |
| Drying | yes (PETG Matte absorbs moisture) | 70 degrees C for 8 h, then dry storage |
| Nozzle / bed | 255 degrees C (230-260) / 70 degrees C (65-75) | 225 degrees C (215 if it strings) / 35 degrees C, no glue |
| Layer / width / speed | 0.2 / 0.42 mm / standard, fan 30-50 % | 0.2 / 0.42 mm / 20 mm/s (15 for the first layer), fan 100 % |
| Support | none, except under the base of F and the recess of G | none; retraction 0.8 mm at 25 mm/s; 2 mm3/s at most |
| First print | one clip plate: it must click like sample 4 | one cushion on a plot: firm friction fit |
