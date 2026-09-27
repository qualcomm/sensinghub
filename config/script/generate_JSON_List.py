#!/usr/bin/env python
#===============================================================================
#
# @file generate_Json_List.py
#
# GENERAL DESCRIPTION
#   Accept look up directories for JSON files and create a list file with all found .json file names
#
#Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries. 
#SPDX-License-Identifier: BSD-3-Clause-Clear
#
#===============================================================================

import argparse
import platform
import os
import sys

def main():
    parser = argparse.ArgumentParser(
                        prog='generate_json_list',
                        description='accept look up directories for JSON files and create a list file with all found .json file names'                        )
    # build_root_default = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    parser.add_argument( '--directories', '-d',  dest='directories', required=True, help='Comma separated lookup paths')
    parser.add_argument( '--force', '-f',  dest='force', default = '', help='Comma separated explicit files')
    parser.add_argument( '--recursive', '-r',  dest='recursive', default = 0, help='Look for json resursively')
    parser.add_argument( '--output', '-o',  dest='output', default = 'json.lst', help='output file path')
    args = parser.parse_args()

    # checking if the current operating system is Windows. This script only work for windows
    if platform.system() != 'Windows':
        print('This script only work for windows')
        exit(1)

    # This has to be comma seperated, no space between two directories
    dir_list = (args.directories).split(',')
    json_filenames = []
    # CHRE should be disabled by default, so skipping it's inclusion in generated json.lst
    CHRE_JSON_FILENAME = "chre_dynamic_sensors.json"
    for dir_name in dir_list:
        if int(args.recursive) == True:
            for root, dirs, files in os.walk(dir_name):
                for filename in files:
                    if filename.endswith(".json") and (filename != CHRE_JSON_FILENAME):
                        json_filenames.append(filename)
        else:
            for filename in os.listdir(dir_name):
                if filename.endswith(".json") and (filename != CHRE_JSON_FILENAME):
                    json_filenames.append(filename)

    # This has to be comma seperated, no space between two files
    force_file_list = (args.force).split(',')
    for filename in force_file_list:
        if filename.endswith(".json") and (filename != CHRE_JSON_FILENAME):
            json_filenames.append(filename)

    with open(args.output, "w") as f:
        f.write("\n".join(json_filenames))

if __name__ == '__main__':
    args=len(sys.argv)
    main()
