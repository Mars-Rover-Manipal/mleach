# wcdm-2020
Code for WCDM-2020

## Intructions
Copy the ns-3-dev dir to ns-3-allinone and replace existing version if exists
Build the current version
`cd ns-3-dev`
`./waf configure --build-profile=debug`
`./waf`
Then make `anim` and `trace/pcap` directories
`mkdir anim trace\pcap`
Run Simulation
`./waf --run "scratch/main"`
For Editing Simulation Parameters
`./waf --run "scratch/main --PrintHelp"`
