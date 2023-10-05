#!/bin/sh
#SBATCH --job-name="gem5-spec2017"
#SBATCH --account=ie-idi
#SBATCH --mail-user=markus@halvorsenfamilien.com
#SBATCH --mail-type=ALL
#SBATCH --output=/dev/null
#SBATCH --partition=CPUQ
#SBATCH --nodes=1
#SBATCH --cpus-per-task=2
#SBATCH --mem=3000
#SBATCH --time=7-06:00:00
#SBATCH --signal=B:SIGINT@120

SPEC2017_DIR=/cluster/home/markuswh/gem5-runahead/spec2017
RUNSCRIPT_DIR="$SPEC2017_DIR/scripts"
BENCHMARK=$1
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

M5_OUT_DIR="$LOG_DIR/m5out"
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
echo "job: simulate SPEC2017 benchmark - $BENCHMARK"
echo "time: $(date)"
echo "--- start job ---"

FSPARAMS=(
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

PARAMS="${FSPARAMS[@]}"
echo "spec2017.py parameters:"
echo "$PARAMS"
echo

./gem5/build/X86/gem5.fast --outdir $M5_OUT_DIR \
    $SPEC2017_DIR/configs/spec2017.py $PARAMS \
    > $SIMOUT_FILE
