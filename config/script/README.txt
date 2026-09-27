==============================================================================
  Script to Generate JSON List (json.lst)
==============================================================================

==============================================================================
  Requirment
==============================================================================

1. Python 2.7.17 or up

==============================================================================
  Introduction
==============================================================================

This script is use to list all the JSON file name in a file. It accept a
directory, then it look for all the JSON file recursively (-r) and list them
to a file.

Curretly, it can be use only in windows

==============================================================================
  Usage
==============================================================================

Note:
There should be no space between comma seperated directories/files.

python generate_JSON_List.py -d <comma separated lookup paths> -f <comma separated explicit files> -r <recursive> -o <foutput file path>

Ex: python generate_JSON_List.py -d Y:\2023\registry\config\common,Y:\2023\registry\config\pineapple -f qsh_camera.json,sns_cal.json -r 1 -o Y:\2023\registry\json.lst
