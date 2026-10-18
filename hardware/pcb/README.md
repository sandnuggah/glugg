# Glugg carrier board (rev A)

A 60 × 50 mm, 2-layer, through-hole PCB that replaces the hand-wired protoboard. It carries exactly the verified protoboard circuit (see `CLAUDE.md`, Wiring), so the firmware is unchanged.

- `glugg.kicad_pro` / `.kicad_sch` / `.kicad_pcb`: the KiCad 9 project. `glugg.kicad_sym` and `glugg.pretty/` hold the two Zero boards' symbols, the 74HCT32 as one block, and the Zero-on-female-headers footprint.
- `fab/`: what to order and build from.
  - `glugg-rev-A-gerbers.zip`: Gerbers and drill files for a PCB maker (2 layers, 1.6 mm).
  - `glugg-rev-A-bom.csv`: parts with Electrokit article numbers.
  - `glugg-schematic.pdf`, `glugg-top.png`, `glugg-bottom.png`: for review.
  - `glugg-1to1-print.pdf`: print at 100 % (no "fit to page") and lay the real parts on it before ordering.

Checks: KiCad ERC 0 violations; DRC with schematic parity 0 violations, 0 unconnected, 0 parity issues.

Both Zero boards sit USB-C up at the top edge; the S3's USB-C is the power input and console, as before. The LED panel connects to J1 (5V, DIN, GND) at the bottom edge.
