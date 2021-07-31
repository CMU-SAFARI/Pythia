#!/bin/bash

ROLLUP_SCRIPT=$PYTHIA_HOME/scripts/rollup.pl

echo "Rolling up statistics for 1C_base_config..."
cd experiments_1C/
$ROLLUP_SCRIPT --tlist ../MICRO21_1C.tlist --exp ../rollup_1C_base_config.exp --mfile ../rollup_1C_base_config.mfile > ../rollup_1C_base_config.csv
cd -

echo "Rolling up statistics for 1C_varying_DRAM_bw..."
cd experiments_1C/
$ROLLUP_SCRIPT --tlist ../MICRO21_1C.tlist --exp ../rollup_1C_varying_DRAM_bw.exp --mfile ../rollup_1C_varying_DRAM_bw.mfile > ../rollup_1C_varying_DRAM_bw.csv
cd -

echo "Rolling up statistics for 4C..."
cd experiments_4C/
$ROLLUP_SCRIPT --tlist ../MICRO21_4C.tlist --exp ../rollup_4C.exp --mfile ../rollup_4C.mfile > ../rollup_4C.csv
cd -
