# MegaZeux libxmp fork.

This is a fork of libxmp generated specifically for MegaZeux
from this branch using the file `mzx-gen.sh`: https://github.com/AliceLR/libxmp/tree/mzx-gen-libxmp

The script that generated this forked
[libxmp 4.7.0](https://github.com/libxmp/libxmp/releases/tag/libxmp-4.7.0)
and applied the following branches as patches:

* [mzx-integration-hacks](https://github.com/AliceLR/libxmp/tree/mzx-integration-hacks):
  adds some defines to the start of xmp.h to hack various things to act
  the way MZX would prefer, since this copy of libxmp needs to build static
  against MZX in the MZX build system.
* [mzx-remove-extra-formats](https://github.com/AliceLR/libxmp/tree/mzx-remove-extra-formats):
  removes some of the more obscure formats supported by libxmp that were not
  likely to have been used. This means effectively everything that wasn't
  supported by the MegaZeux fork of libmodplug except for things that could
  be mistaken for other formats (misc. formats for MOD, MED derivatives for MED).
  This keeps the MZX supported formats list roughly the same between libmodplug,
  mikmod, and libxmp and helps reduce executable size for embedded platforms.

This patch automatically copies only the files required by MegaZeux, so manual file
removal should no longer be necessary.

See README and docs/COPYING.LIB for more libxmp information and the libxmp license.
See the upstream repository for the original libxmp, which you should probably use
instead of this hacked up copy for most purposes: https://github.com/libxmp/libxmp/
