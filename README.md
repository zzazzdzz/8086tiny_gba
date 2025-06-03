# 8086tiny_gba

[8086tiny](https://github.com/adriancable/8086tiny), but it runs on the GBA. It can run inside a Pokémon Emerald save file too.

This was a silly toy project, and it's not something I intend to actively maintain. 

# Why?

¯\\_(ツ)\_/¯

# Want to try it out?

Precompiled GBA ROMs, as well as the Pokémon Emerald save file, are available under *bin/*. 

# Building from source

You'll need the following:

- GNU ARM binutils installed, more exactly *arm-none-eabi-gcc*, *arm-none-eabi-as* and *arm-none-eabi-objcopy* (on Ubuntu, you can get them with *apt install gcc-arm-none-eabi*, which will install all necessary packages in one go).
- Python 3 installed and available in PATH as *python3* (on Ubuntu, you can just *apt install python3*).
- GNU build tools, most notably *make* (on Ubuntu, *apt install build-essential*).

Go to the source directory:

```
cd src
```

Compile the standalone ROM version:

```
make gba
```

Or the Pokémon Emerald save file version (aka JoyBus version):

```
make sav
```

## Included boot images

The emulated PC will boot from contents of *fd.img*, and will read BIOS code from *bios*. The included files are the same ones included with [8086tiny](https://github.com/adriancable/8086tiny).

## The disk drive emulator

The standalone ROM version includes the image files within the resulting ROM - however, JoyBus versions will require connecting to an emulated external disk drive over the GBA link port.

A software emulator is included, which will work for mGBA and VBA-M. Simply run `python3 joybus.py` to start it.
