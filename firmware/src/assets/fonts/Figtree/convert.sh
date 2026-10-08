#!/bin/sh
# Regenerates the Figtree fonts used by the UI theme (firmware/src/display/theme).
# Figtree is by Erik Kennedy, under the SIL Open Font License (OFL.txt).
#
# The large sizes only carry the characters they show, to keep flash use down.
set -e
cd "$(dirname "$0")"
conv() { npx --yes lv_font_conv@1.5.2 --bpp 4 --no-compress --format lvgl "$@"; }

TEXT="--range 32-126,192-255 --symbols °·–’…"

conv --font Figtree-Medium.ttf   --size 13 $TEXT -o figtree_500_13.c
conv --font Figtree-SemiBold.ttf --size 12 $TEXT -o figtree_600_12.c
conv --font Figtree-SemiBold.ttf --size 17 $TEXT -o figtree_600_17.c
conv --font Figtree-SemiBold.ttf --size 23 $TEXT -o figtree_600_23.c
conv --font Figtree-SemiBold.ttf --size 38 --symbols "0123456789%°-. " -o figtree_600_38.c
conv --font Figtree-SemiBold.ttf --size 54 --symbols "0123456789:-– " -o figtree_600_54.c
conv --font Figtree-SemiBold.ttf --size 64 --symbols "0123456789%Of- " -o figtree_600_64.c
conv --font Figtree-SemiBold.ttf --size 76 --symbols "0123456789°-. " -o figtree_600_76.c
