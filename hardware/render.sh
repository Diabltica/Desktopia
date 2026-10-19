#!/bin/bash
# Renders the README images of every KiCad project in this folder into <project>/images/:
#   schematic.svg       the schematic, with its drawing sheet
#   pcb_front.svg       front copper, solder mask, silkscreen and outline
#   pcb_back.svg        the same for the back side, mirrored (seen from below)
#   3d_front.png        3D view from above, 3d_back.png from below, 3d_iso.png isometric
#
# A project is only rendered again when its sources change (KiCad files, shared library, this script): the
# fingerprint of the last render is kept in images/.sources. The 3D raytracer is not bit-for-bit reproducible,
# so this keeps the images (and the commits of the CI) unchanged when the boards are unchanged. FORCE=1 renders
# everything anyway.
#
# Runs in the GitHub workflow .github/workflows/hardware-images.yml (kicad/kicad image), and locally:
#   hardware/render.sh                                        # kicad-cli on the PATH
#   KICAD_CLI="/c/Program Files/KiCad/10.0/bin/kicad-cli.exe" hardware/render.sh
set -euo pipefail
cd "$(dirname "$0")"
CLI=${KICAD_CLI:-kicad-cli}

PCB_FRONT="F.Cu,F.Mask,F.Silkscreen,Edge.Cuts"
PCB_BACK="B.Cu,B.Mask,B.Silkscreen,Edge.Cuts"

# Content hash of the inputs. git hash-object applies the line-ending normalization, so Windows and Linux agree.
fingerprint() {  # <project dir>
  { "$CLI" version
    find "$1" -maxdepth 1 \( -name '*.kicad_*' ! -name '*.kicad_prl' -o -name '*-lib-table' \)
    find kicad_library -type f ! -name '*.bak'
    echo render.sh
  } | sort | while read -r f; do
    if [ -f "$f" ]; then echo "$f $(git hash-object "$f")"; else echo "$f"; fi
  done | git hash-object --stdin
}

for pro in */*.kicad_pro; do
  dir=$(dirname "$pro")
  name=$(basename "$pro" .kicad_pro)
  out="$dir/images"
  sum=$(fingerprint "$dir")
  if [ "${FORCE:-0}" != 1 ] && [ "$(cat "$out/.sources" 2>/dev/null)" = "$sum" ]; then
    echo "== $name: unchanged"
    continue
  fi
  echo "== $name"
  mkdir -p "$out"

  "$CLI" sch export svg -o "$out" "$dir/$name.kicad_sch"
  mv -f "$out/$name.svg" "$out/schematic.svg"

  "$CLI" pcb export svg --mode-single --page-size-mode 2 --exclude-drawing-sheet \
    -l "$PCB_FRONT" -o "$out/pcb_front.svg" "$dir/$name.kicad_pcb"
  "$CLI" pcb export svg --mode-single --page-size-mode 2 --exclude-drawing-sheet --mirror \
    -l "$PCB_BACK" -o "$out/pcb_back.svg" "$dir/$name.kicad_pcb"
  sed -i '/^<title>SVG Image created as /d' "$out"/*.svg        # drop the export date

  render() {  # <png> <view args...>
    local file=$1; shift
    "$CLI" pcb render --quality high --background opaque --width 1600 --height 900 "$@" \
      -o "$out/$file" "$dir/$name.kicad_pcb"
  }
  render 3d_front.png --side top
  render 3d_back.png --side bottom
  render 3d_iso.png --side top --rotate "-45,0,-30" --perspective --zoom 0.9

  echo "$sum" > "$out/.sources"
done
