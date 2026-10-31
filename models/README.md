# 3D-printed parts

Hotline uses seven 3D-printed parts. They hold the new electronics in the original phone. This folder contains the STL file for each part and two Bambu Studio project files with the print settings.

## Parts

| File | Where it goes | What it does |
| --- | --- | --- |
| `Top Panel Main.stl` | In the existing square hole in the top of the case, behind the dial | Holds the mode toggle switch (6 mm hole) and the two LED holders. Two screws go through it into the Top Panel Cap. |
| `Top Panel Cap.stl` | Inside the case, under the Top Panel Main | Clamp plate. The two screws pull it against the inside of the case, so the panel is tight. No new holes are needed in the case. |
| `Board Mount.stl` | On the metal base of the phone | Holds the ProtoMate board with the QT Py. |
| `Back Port Body.stl` | On the metal cord bracket at the back of the base | Holds the female end of the USB-C extension cable (the USB-C socket). |
| `Back Port Cover.stl` | On the back of the Back Port Body | Stops the USB-C socket from sliding out backwards. The slot lets the cable through. |
| `Microphone Mount.stl` | In the handset mouthpiece, in place of the original microphone capsule | Holds the MAX4466 microphone amplifier board. |
| `Microphone Cover.stl` | Behind the mouthpiece grille | A disc that holds the electret microphone capsule. |

The parts fit an original GPO 746.

### Fixings

All the fixings are M2 x 8 mm self-tapping hex socket head screws (1.5 mm hex key).

- **Board Mount to the base:** make sure that the screw tips do not go through the plastic, and keep bare wire ends and solder joints away from the metal base.
- **ProtoMate to the Board Mount:** keep the screw heads away from the pads and the tracks.
- **Top Panel Main to the Top Panel Cap:** two screws.

## Printing

The project files are set up for a Bambu Lab H2C and Bambu PLA Matte. The handset parts are black, and all the other parts are red.

| Project file | Nozzle | Layer height | Plates |
| --- | --- | --- | --- |
| `PRINT SETTINGS - 0.2mm.3mf` | 0.2 mm | 0.10 mm | 1: Top Panel Main and Top Panel Cap (red) |
| `PRINT SETTINGS - 0.4mm.3mf` | 0.4 mm | 0.20 mm | 1: Board Mount (red)<br>2: Microphone Mount and Microphone Cover (black)<br>3: Back Port Body and Back Port Cover (red) |

Plates 2 and 3 need support. The support interface is white PETG (Bambu PETG Basic). PETG does not bond well to PLA, so the support comes off cleanly. If you have only one material, print the support in PLA and expect a rougher surface under the overhangs.

Other settings:

| Setting | 0.2 mm file | 0.4 mm file |
| --- | --- | --- |
| Walls | 4 | 3 |
| Infill | 15 %, grid | 15 %, gyroid |
| Top and bottom layers | 7 and 5 | 5 and 3 |

The project files open in Bambu Studio and OrcaSlicer. For a different slicer, use the STL files with the settings above.
