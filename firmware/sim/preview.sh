#!/bin/bash
# Builds ../docs/ui_preview.png from the screenshots of the last ./run.sh, and ../docs/anim_*.gif from the films of
# the last SIM_FILM=1 ./run.sh when there are any (ImageMagick "montage" and "convert" required).
set -e
cd "$(dirname "$0")"
montage -font DejaVu-Sans -pointsize 13 -fill '#dddddd' -background '#1a1a1f' \
  -label 'At rest'                       shots/01_idle.ppm \
  -label 'Up (chevrons, ring filling)'   shots/02_moving_up.ppm \
  -label 'Down'                          shots/03_moving_down.ppm \
  -label 'Going to preset 2'             shots/16_goto.ppm \
  -label 'Preset reached (ding)'         shots/17_arrived.ppm \
  -label 'Long press: gauge 35 %'        shots/04_gauge_35.ppm \
  -label 'Long press: gauge 85 %'        shots/05_gauge_85.ppm \
  -label 'Preset saved'                  shots/06_success.ppm \
  -label 'Asleep (on preset 1)'          shots/18_asleep.ppm \
  -label 'Unknown height'                shots/11_idle_no_height.ppm \
  -label 'Empty preset'                  shots/07_notice_nopreset.ppm \
  -label 'Fault: sensor lost'            shots/08_fault_sensor.ppm \
  -label 'Fault: wrong way'              shots/09_fault_wrongway.ppm \
  -label 'Fault: overcurrent'            shots/14_fault_overcurrent.ppm \
  -label 'HMI board: no link'            shots/13_no_link.ppm \
  -tile 5x -geometry +5+5 -gravity West ../docs/ui_preview.png
echo "../docs/ui_preview.png written"
for dir in shots/anim_*; do
  [ -d "$dir" ] || continue
  name=$(basename "$dir")
  convert -delay 8 -loop 0 "$dir"/*.ppm -layers Optimize "../docs/$name.gif"   # one picture per 80 ms step
  echo "../docs/$name.gif written"
done
