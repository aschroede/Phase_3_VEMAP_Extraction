#!/bin/bash

# Test all command line options

# Test MAP
examples/example_map -i ./factorgraphformats/hailfinder_fix.fg -o ./hailfinder_h5_map_ve.txt -M -H 0,1,2,5,6 -E 43,44,45,46,47,48,49,50,51,52,53,54,55 -e 1,6,1,1,0,1,0,2,3,3,1,1,4
# Test Junction Tree
examples/example_map -i ./factorgraphformats/hailfinder_fix.fg -o ./hailfinder_h5_map_jt.txt -J -H 0,1,2,5,6 -E 43,44,45,46,47,48,49,50,51,52,53,54,55 -e 1,6,1,1,0,1,0,2,3,3,1,1,4
# Run Variable Elimination Query
examples/example_map -i ./factorgraphformats/hailfinder_fix.fg -o ./hailfinder_h5_ve.txt -V -H 0,1 -E 43,44,45,46,47,48,49,50,51,52,53,54,55 -e 1,6,1,1,0,1,0,2,3,3,1,1,4
# Run MAP with a different graph
examples/example_map -i ./factorgraphformats/alarm_fix.fg -o ./alarm_fix_map_ve.txt -M -H 1 -E 0,2,3 -e 0,1,1
# Verify JT approach gets same answer
examples/example_map -i ./factorgraphformats/alarm_fix.fg -o ./alarm_fix_map_jt.txt -J -H 1 -E 0,2,3 -e 0,1,1
# Run VE on this graph as well
examples/example_map -i ./factorgraphformats/alarm_fix.fg -o ./alarm_fix_ve.txt -V -H 1 -E 0,2,3 -e 0,1,1


