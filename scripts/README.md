<p align="center">
  <h1 align="center">Script Documentation
  </h1>
</p>

<details open="open">
  <summary>Table of Contents</summary>
  <ol>
    <li><a href="#overview">Overview</a></li>
    <li><a href="#create-jobfile-script">Create Jobfile Script</a></li>
    <li><a href="#rollup-stats-script">Rollup Stats Script</a></li>
    <li><a href="#installation">Installation</a></li>
  </ol>
</details>

## Overview
This README contains documentation of additional scripts present in this directory. More specifically, we will discuss about `create_jobfile.pl` and `rollup.pl` scripts.

## Create Jobfile Script
`create_jobfile.pl` script helps to create the necessary experiment commandlines. The script requires three necessary arguments:
* `exe`: The full path of the executable to run
* `tlist`: Contains trace definitions
* `exp`: Contains knobs of the experiements to run

Some sample `tilst`, and `exp` can be found in `$PYTHIA_HOME/experiments` directory.

The additional arguments of the scripts are:

| Argument | Description | Default |
| -------- | ----------- | --------------|
| `local` | Set to 0 (or 1) to generate commandlines to run in slurm cluster (or in local machine). | 0 |
| `partition` | Default slurm partition name. | `slurm_part` |
| `ncores` | Number of cores requested by each slurm job to slurm controller. Typically set to 1, as ChampSim is inherently a single-core simulator. | 1 |
| `include_list` | IDs of only those cluster nodes that will be used to launch runs. Setting NULL will include all nodes by default. The default node hostname is set to `kratos`. | NULL |
| `exclude_list` | IDs of the cluster nodes to exclude from lauching runs. The default node hostname is set to `kratos`. | NULL |
| `extra` | Any extra configuration knobs to supply to slurm scheduler. | NULL |

Some example usage of the scripts are as follows:

1. Generate jobs for local machine
   
    ```bash
    perl ../scripts/create_jobfile.pl --exe $PYTHIA_HOME/bin/perceptron-multi-multi-no-ship-1core --tlist MICRO21_1C.tlist --exp MICRO21_1C.exp --local 1 > jobfile.sh
    ```
2. Generate jobs to run on all machines in slurm cluster parition named "develop", but excluding machines `kratos2` and `kratos4`
   
    ```bash
    perl ../scripts/create_jobfile.pl --exe $PYTHIA_HOME/bin/perceptron-multi-multi-no-ship-1core --tlist MICRO21_1C.tlist --exp MICRO21_1C.exp --local 0 --partition develop --exclude_list "2,4" > jobfile.sh
    ```

3. Generate jobs to run on all machines in slurm cluster parition named "develop" with less priority
   
    ```bash
    perl ../scripts/create_jobfile.pl --exe $PYTHIA_HOME/bin/perceptron-multi-multi-no-ship-1core --tlist MICRO21_1C.tlist --exp MICRO21_1C.exp --local 0 --partition develop --extra "--nice=200" > jobfile.sh
    ```

## Rollup Stats Script
`rollup.pl` script helps to rollup statistics from the ChampSim outputs of multiple experiments over multiple traces in a single CSV file. The output CSV file is dumped in pivot-table friendly way, so that the user can easily post-process the csv in their favourite number processor (e.g., Python Pandas, Microsoft Excel, etc.). 

For each trace and each experiment, the script populates a `filter` column. The filter column for all experiments of a trace X will be populated as `1` if and only if **_all_** statistic metircs from **_all_** experments are valid. This is done to quickly discard traces from the final pivot-table statistics for which at least one experiment has failed. 


`rollup.pl` requires three necessary arguments:
* `tlist`: Contains trace definitions
* `exp`: Contains knobs of the experiements to run
* `mfile`: Specifies stat names and reduction method to rollup

Some sample `tilst`, `exp`, and `mfile` can be found in `$PYTHIA_HOME/experiments` directory.

The additional arguments of the scripts are:
| Argument | Description | Default |
| -------- | ----------- | --------------|
| `ext` | Extension of the statistics file. | `out` |

Some example usages are:
1. Rollup from `.out` stat files:
   
    ```bash
      cd experiements_1C/
      perl ../../scripts/rollup.pl --tlist ../MICRO21_1C.tlist --exp ../MICRO21_1C.exp --mfile ../rollup_1C_base_config.mfile --ext "out" > rollup.csv
    ``` 