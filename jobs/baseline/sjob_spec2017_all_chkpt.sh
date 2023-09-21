#!/bin/sh
#SBATCH --job-name="gem5-spec2017"
#SBATCH --account=ie-idi
#SBATCH --mail-user=markus@halvorsenfamilien.com
#SBATCH --mail-type=ALL
#SBATCH --output=/dev/null                       # output is manually redirected
#SBATCH --array=1-28%7                           # run 7 benchmarks at a time
#SBATCH --partition=CPUQ
#SBATCH --nodes=1                                # 1 compute nodes
#SBATCH --cpus-per-task=2                        # 2 CPU cores
#SBATCH --mem=1500
#SBATCH --time=6-06:00:00
#SBATCH --signal=SIGINT                          # exit with SIGINT to let gem5 save stats 

ALL_BENCHMARKS=(
    "perlbench_s_0" "perlbench_s_1" "perlbench_s_2"
    "gcc_s_0" "gcc_s_1" "gcc_s_2"
    "bwaves_s_0" "bwaves_s_1"
    "mcf_s_0"
    "cactuBSSN_s_0"
    "lbm_s_0"
    "omnetpp_s_0"
    "wrf_s_0"
    "xalancbmk_s_0"
    "x264_s_0" "x264_s_1" "x264_s_2"
    "cam4_s_0"
    "pop2_s_0"
    "deepsjeng_s_0"
    "imagick_s_0"
    "leela_s_0"
    "nab_s_0"
    "exchange2_s_0"
    "fotonik3d_s_0"
    "roms_s_0"
    "xz_s_0" "xz_s_1"
)

SPEC2017_DIR=/cluster/home/markuswh/gem5-runahead/spec2017
RUNSCRIPT_DIR="$SPEC2017_DIR/scripts"
BENCHMARK=${ALL_BENCHMARKS[$SLURM_ARRAY_TASK_ID - 1]}
RUNSCRIPT="$RUNSCRIPT_DIR/$BENCHMARK.rcS"

# sanity check
if ! [[ -f "$RUNSCRIPT"  ]]; then
    echo "\"$BENCHMARK\" - invalid benchmark!"
    exit 1
fi

# create the log directory
LOG_DIR="$SPEC2017_DIR/logs/$BENCHMARK"
if ! [[ -d "$LOG_DIR" ]]; then
    mkdir -p $LOG_DIR
fi

M5_OUT_DIR="$LOG_DIR/m5out"
SIMOUT_FILE="$LOG_DIR/${SLURM_JOB_NAME}_simout.log"
SLURM_LOG_FILE="$LOG_DIR/${SLURM_JOB_NAME}_slurm.log"

# redirect all output to the slurm logfile
exec &> $SLURM_LOG_FILE

echo "--- loading modules ---"
module --quiet purge
module restore gem5
module list

cd /cluster/home/markuswh/gem5-runahead
source venv/bin/activate
echo "--- python packages ---"
pip freeze

echo "job: run SPEC2017 benchmark - $BENCHMARK"
echo "--- start job ---"

# check for checkpoint
CHECKPOINT_DIR=$(ls -d $M5_OUT_DIR/*cpt*)
CHECKPOINT_NUM=-1
echo $CHECKPOINT_DIR
if [[ -n "$CHECKPOINT_DIR" ]]; then
    # regex the checkpoint number out of the directory name
    CHECKPOINT_NUM=$(echo $CHECKPOINT_DIR | sed "s/^.\+cpt\.\([0-9]\+\)$/\1/")
    echo "checkpoint found - $CHECKPOINT_DIR"
else
    echo "no checkpoint found - starting with boot"
fi

CPU_TYPE="O3CPU"
FSPARAMS=(
    "--maxinsts=2000000000" # max 2 billion instructions
    "--cpu-type=$CPU_TYPE"
    "--cpu-clock=3.2GHz"
    "--bp-type=TAGE_SC_L_8KB"
    "--caches" "--l2cache"
    "--l1i_size=32kB" "--l1i_assoc=4"
    "--l1d_size=32kB" "--l1d_assoc=8"
    "--l1d-hwp-type=StridePrefetcher"
    "--l2_size=256kB" "--l2_assoc=8"
    "--l2-hwp-type=StridePrefetcher"
    "--l3_size=1MB" "--l3_assoc=16"
    "--mem-type=DDR3_1600_8x8"
    "--mem-size=2GB"
    "--mem-ranks=2" "--mem-channels=2"
    "--kernel=$SPEC2017_DIR/plinux"
    "--disk-image=$SPEC2017_DIR/x86-3.img"
    "--script=$RUNSCRIPT"
)

# use checkpoint if it was found
if [[ $CHECKPOINT_NUM != -1 ]]; then
    echo "using checkpoint @ $CHECKPOINT_NUM"
    # fs.py will find all checkpoints and sort them by tick number
    # --checkpoint-restore is a list index, not a checkpoint number
    # so this picks the first checkpoint
    FSPARAMS+=("--checkpoint-restore=1")
    FSPARAMS+=("--restore-with-cpu=$CPU_TYPE")
fi

cd /cluster/home/markuswh/gem5-runahead
source venv/bin/activate

./gem5/build/X86/gem5.opt --outdir $M5_OUT_DIR \
    $SPEC2017_DIR/configs/fs.py "${FSPARAMS[@]}" \
    > $SIMOUT_FILE
