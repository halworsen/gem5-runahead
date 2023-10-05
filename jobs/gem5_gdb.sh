#!/bin/sh
#SBATCH --job-name="gem5-gdb"
#SBATCH --account=share-ie-idi
#SBATCH --mail-type=ALL
#SBATCH --output=/dev/null                       # output is manually redirected
#SBATCH --partition=CPUQ
#SBATCH --nodes=1
#SBATCH --cpus-per-task=2
#SBATCH --mem=2000
#SBATCH --time=06:00:00
#SBATCH --signal=B:SIGINT@120                    # exit with SIGINT to let gem5 save stats 

BENCHMARK="gcc_s_0"
SPEC2017_DIR="/cluster/home/markuswh/gem5-runahead/spec2017"
RUNSCRIPT_DIR="$SPEC2017_DIR/scripts"
RUNSCRIPT="$RUNSCRIPT_DIR/$BENCHMARK.rcS"
RUNAHEAD_DIR="$HOME/gem5-runahead"

# create the log directory
LOG_DIR="$RUNAHEAD_DIR/debug/logs"
if ! [[ -d "$LOG_DIR" ]]; then
    mkdir -p "$LOG_DIR"
fi

M5_OUT_DIR="$LOG_DIR/m5out-${SLURM_JOB_NAME}"
SIMOUT_FILE="$LOG_DIR/${SLURM_JOB_NAME}_simout.log"
SLURM_LOG_FILE="$LOG_DIR/${SLURM_JOB_NAME}_slurm.log"

# redirect all output to the slurm logfile
exec &> $SLURM_LOG_FILE

# Parse workload option
while getopts "w:" flag
do
    case "${flag}" in
        w) WORKLOAD="${OPTARG}";
    esac
done

if [[ -z "$WORKLOAD" ]]; then
    echo "Workload is not specified! Must be \"test\" or \"spec2017\""
    exit 1
fi

# Params if running the matrix multiplication test program
TEST_PARAMS=(
    "--size=2"
    "--random=1"
)

# Params if running the SPEC2017 benchmarks
SPEC_PARAMS=(
    "--kernel=$SPEC2017_DIR/plinux"
    "--image=$SPEC2017_DIR/x86-3.img"
    "--script=$RUNSCRIPT"
    "--max-insts=2000000000" # max 2 billion instructions
    "--clock=3.2GHz"

    # 500mil insts
    "--simpoint-interval=500000000"

    # Cache & memory
    "--l1i-size=32kB" "--l1i-assoc=4"
    "--l1d-size=32kB" "--l1d-assoc=8"
    "--l2-size=256kB" "--l2-assoc=8"
    "--l3-size=6MB" "--l3-assoc=12"
    "--mem-size=2GB"
)

# Setup config script/params for the workload
case "$WORKLOAD" in
    "test")
        CONFIG_SCRIPT="$RUNAHEAD_DIR/gem5-extensions/configs/test/test_re.py"
        CONFIG_PARAMS="${TEST_PARAMS[@]}"
        ;;
    "spec2017")
        CONFIG_SCRIPT="$SPEC2017_DIR/configs/spec2017.py"
        CONFIG_PARAMS="${SPEC_PARAMS[@]}"
        ;;
esac

echo "--- loading modules ---"
module --quiet purge
module restore gem5
module list

cd $RUNAHEAD_DIR
source venv/bin/activate
echo "--- python packages ---"
pip freeze

echo
echo "job: test run of gem5 - $WORKLOAD"
echo "time: $(date)"
echo "config script: $CONFIG_SCRIPT"
echo "config params: $CONFIG_PARAMS"
echo "--- start job ---"

# Debug functions provided by gem5:
# schedBreak(tick) - schedule SIGTRAP at tick
# setDebugFlag("flag") - set a debug flag
# clearDebugFlag("flag") - clear a debug flag
# eventqDump() - print all events in the event queue
# takeCheckpoint(tick) - write a checkpoint at tick
# SimObject::find("system.qualified.name") - use FQN to get a pointer to the specified simobject
#
# use schedBreak(<tick>) when connected to target
GDBSERVER=$HOME/gdb-13.2/gdbserver/gdbserver
$GDBSERVER localhost:34617 \
    ./gem5/build/X86/gem5.debug --debug-break=1000 \
    --outdir $M5_OUT_DIR \
    $CONFIG_SCRIPT $CONFIG_PARAMS \
    > $SIMOUT_FILE
    # --debug-start=1910959500 \
    # --debug-flags=Runahead,O3CPUAll,X86,TLB \
