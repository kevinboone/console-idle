#!/bin/bash
# See README.md for more information about configuration
./console-idle  --dev /dev/input/event0 --dev=/dev/input/mice -t 5 -- jpegtofb --sleep 5 --landscape --randomize /home/kevin/processed_photos/screensaver/*.jpg 
