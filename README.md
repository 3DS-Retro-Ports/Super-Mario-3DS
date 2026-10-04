# Super Mario 64 - Enhanced 3DS Port

A custom, high-performance port of Super Mario 64 for the Nintendo 3DS family of systems, focused on modern controls, a full 3D free camera, mod support, and broad optimization across all hardware revisions.

---

## Key Features

### Modernized Control Scheme
Designed to fully utilize the New 3DS hardware, Circle Pad Pro, and original Old 3DS systems:

* **Movement:** C-Stick (Main analog stick).
* **360° Free Camera Control:**
  * **New 3DS / Circle Pad Pro:** C-Pad (Right analog stick).
  * **Old 3DS / Old 2DS:** Touch Screen drag controls for full camera rotation.
* **Remapped Buttons:**
  * **B** -> Jump (Original A).
  * **Y** -> Grab/ Attack (B).
  * **R / L** -> Crouch / Z-Trigger.
  * **ZL / ZR** -> L / R functions.

---

## Free Camera Engine
* Full 360-degree control using the C-Pad or Touch Screen, removing the original fixed Lakitu camera restrictions.
* Improved camera collision handling to prevent clipping through walls and map geometry.

---

## Massive Optimization for Old 3DS
(NOT TESTED) Built with the target of maintaining 60 FPS performance on original Old 3DS and Old 2DS hardware:

* **Optimized Rendering Backend:** Direct conversion of Display Lists to Citro3D to minimize GPU overhead on the PICA200.
* **Efficient VRAM Management:** Dynamic texture and geometry loading to keep memory usage minimal.
* **New 3DS Enhancements:** Automatic unlock of the 804 MHz CPU clock, L2 cache enable, and multithreaded physics/loading offloaded to Core 1.

---

## sm64coopdx Mod Support
* Architecture structured for compatibility with scripts and assets from the sm64coopdx ecosystem.
* Prepared for custom model integration, character skins, and extended features.

---

## Build Requirements

* **devkitPro** with `devkitARM`.
* Libraries: `libctru`, `citro3d`, `citro2d`.
* Original *Super Mario 64 (USA)* ROM in `.z64` format for asset extraction during the build process.

---

## Credits & Acknowledgments

* **n64decomp / sm64ex:** For the original source code decompilation and refactoring work.
* **sm64coopdx Team:** For the mod engine foundation and extended features.
* **3DS Homebrew Community:** For open-source tooling and libraries (`libctru` / `citro3d`).
