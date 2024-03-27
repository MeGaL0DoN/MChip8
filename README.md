# MChip8

MChip8 is a Chip 8 emulator made in C++ using OpenGL, GLFW, and ImGUI.

## Build

Currently only windows build using visual studio is supported, however all libraries used in the emulator are cross-platform, so it should be possible to build on Linux and MacOS.

## Overview

### Usage:

Use File->load to load game ROM. File->Reload or ESC to restart current ROM. Press TAB to put game on pause. Looks/CPU frequency can be changed in settings. 
Default keyboard layout is: 
| 1 | 2 | 3 | 4 |
| --- | --- | --- | --- |
| Q | W | E | R |
| A | S | D | F |
| Z | X | C | V |

Layout can be changed by editing data/keyConfig.ini - you should enter the key scancodes on the right side. 

![Screenshot 2024-03-14 180236](https://github.com/MeGaLoDoN228/MChip8/assets/62940883/deef2005-45af-4075-9c2e-8d42e336dec8)
Or with pixel borders
![Screenshot 2024-03-14 175903](https://github.com/MeGaLoDoN228/MChip8/assets/62940883/b1eb167e-f683-4abc-bdd9-2e745621d1ce)


### Some games running:

![Screenshot 2024-03-13 222024](https://github.com/MeGaLoDoN228/MChip8/assets/62940883/af314df9-388c-4dd3-b9a3-91c16e26336d)

![Screenshot 2024-03-13 222142](https://github.com/MeGaLoDoN228/MChip8/assets/62940883/4b5c22dc-b8a5-4e8b-9f3a-bf88baa1df65)

![Screenshot 2024-03-13 222243](https://github.com/MeGaLoDoN228/MChip8/assets/62940883/6c244415-35c1-4182-83ff-cac74d5e32f2)

![Screenshot 2024-03-13 222349](https://github.com/MeGaLoDoN228/MChip8/assets/62940883/aa4b6571-e8ac-4b82-b23c-39909aa8599c)

## Version History

* 1.0
    * Initial Release
* 1.1
    * Added GUI quirks configuration
* 1.1.1
    * Fixed ImGUI interface to be resolution-independent
* 1.1.2
    * Fixed importing of ROMs with non-ASCII characters in the path

## License

This project is licensed under the MIT License - see the LICENSE.md file for details
