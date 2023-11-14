#!/bin/sh
#SBATCH --job-name="spec2017-sp-chkpt-all"
#SBATCH --account=ie-idi
#SBATCH --mail-type=ALL
#SBATCH --output=/dev/null
#SBATCH --array=1-16%8
#SBATCH --partition=CPUQ
#SBATCH --nodes=1
#SBATCH --cpus-per-task=2
#SBATCH --mem=6000
#SBATCH --time=6-06:00:00
#SBATCH --signal=B:SIGINT@120

#
# Take checkpoints at the simpoints for a given benchmark
#

# commented out = broken
ALL_BENCHMARKS=(
    # "bwaves_s_0" "bwaves_s_1"
    "cactuBSSN_s_0"
    # "cam4_s_0"
    # "deepsjeng_s_0"
    "exchange2_s_0"
    "fotonik3d_s_0"
    #"gcc_s_0"
    "gcc_s_1" "gcc_s_2"
    "imagick_s_0"
    # "lbm_s_0"
    # "leela_s_0"
    "mcf_s_0"
    "nab_s_0"
    "omnetpp_s_0"
    "perlbench_s_0" "perlbench_s_1" "perlbench_s_2"
    # "pop2_s_0"
    # "roms_s_0"
    "wrf_s_0"
    "x264_s_0"
    #"x264_s_1"
    "x264_s_2"
    "xalancbmk_s_0"
    # "xz_s_0" "xz_s_1"
)

SPEC2017_DIR=/cluster/home/markuswh/gem5-runahead/spec2017
RUNSCRIPT_DIR="$SPEC2017_DIR/runscripts"
BENCHMARK=${ALL_BENCHMARKS[$SLURM_ARRAY_TASK_ID - 1]}
RUNSCRIPT="$RUNSCRIPT_DIR/$BENCHMARK.rcS"

# sanity check
if ! [[ -f "$RUNSCRIPT"  ]]; then
    echo "$BENCHMARK - invalid benchmark!"
    exit 1
fi

# create the log directory
LOG_DIR="$SPEC2017_DIR/logs/$BENCHMARK"
if ! [[ -d "$LOG_DIR" ]]; then
    mkdir -p "$LOG_DIR"
fi

M5_OUT_DIR="$LOG_DIR/m5out-${SLURM_JOB_NAME}"
CHECKPOINT_DIR="$LOG_DIR/checkpoints"
SIMPOINT_DIR="$LOG_DIR/simpoints"
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

echo
echo "job: take simpoint checkpoints for SPEC2017 benchmark - $BENCHMARK"
echo "time: $(date)"
echo "--- start job ---"

FSPARAMS=(
    "--kernel=$SPEC2017_DIR/plinux"
    "--image=$SPEC2017_DIR/x86-3.img"
    "--script=$RUNSCRIPT"
    "--max-insts=50000000000" # max 50B instructions
    "--clock=3.2GHz"

    # 100M inst simpoint interval
    "--simpoint-interval=100000000"
    "--simpoint-checkpoints=$M5_OUT_DIR/../m5out-spec2017-simpoint/simpoint"
    # 1M inst warmup
    "--warmup-insts=1000000"

    # Runahead options
    "--no-runahead"

    # Cache & memory
    "--l1i-size=32kB" "--l1i-assoc=4"
    "--l1d-size=32kB" "--l1d-assoc=8"
    "--l2-size=256kB" "--l2-assoc=8"
    "--l3-size=6MB" "--l3-assoc=12"
    "--mem-size=3GB"

    # Pipeline stage widths
    "--fetch-width=4" "--decode-width=4" "--rename-width=4"
    "--issue-width=4" "--writeback-width=8" "--commit-width=8"

    # Issue/load/store queue sizes
    "--iq-size=97" "--lq-size=64" "--sq-size=60"

    # Physical registers
    "--int-regs=180" "--fp-regs=180" "--vec-regs=96"

    # Functional units
    "--int-alus=3" "--int-mds=1"
    "--fp-alus=1" "--fp-mds=1"
    "--mem-ports=2"
)

PARAMS="${FSPARAMS[@]}"
echo "spec2017.py parameters:"
echo "$PARAMS"
echo

./gem5/build/X86/gem5.fast --outdir $M5_OUT_DIR \
    $SPEC2017_DIR/configs/spec2017.py $PARAMS \
    > $SIMOUT_FILE
