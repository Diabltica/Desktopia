#!/bin/bash
# Builds ../docs/ui_preview.png from the screenshots of the last ./run.sh (ImageMagick "montage" required).
set -e
cd "$(dirname "$0")"
montage -font DejaVu-Sans -pointsize 13 -fill '#dddddd' -background '#1a1a1f' \
  -label 'Idle'                          shots/01_idle.ppm \
  -label 'Moving up (arrow)'             shots/02_moving_up.ppm \
  -label 'Moving down (arrow)'           shots/03_moving_down.ppm \
  -label 'Idle, ≥ 100 cm'                shots/10_idle_104cm.ppm \
  -label 'Long press: gauge 35 %'        shots/04_gauge_35.ppm \
  -label 'Long press: gauge 85 %'        shots/05_gauge_85.ppm \
  -label 'Preset saved'                  shots/06_success.ppm \
  -label 'Empty preset'                  shots/07_notice_nopreset.ppm \
  -label 'Fault: sensor lost'            shots/08_fault_sensor.ppm \
  -label 'Fault: wrong way'              shots/09_fault_wrongway.ppm \
  -label 'Unknown height'                shots/11_idle_no_height.ppm \
  -label 'HMI board: no link'            shots/13_no_link.ppm \
  -tile 4x -geometry +5+5 -gravity West ../docs/ui_preview.png
echo "../docs/ui_preview.png written"
