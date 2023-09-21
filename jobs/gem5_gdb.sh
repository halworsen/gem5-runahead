#!/bin/sh
#SBATCH --job-name="gem5-gdb"
#SBATCH --account=share-ie-idi
#SBATCH --mail-type=ALL
#SBATCH --output=/dev/null                       # output is manually redirected
#SBATCH --partition=CPUQ
#SBATCH --nodes=1                                # 1 compute node
#SBATCH --cpus-per-task=2                        # 2 cores
#SBATCH --mem=1500
#SBATCH --time=06:00:00
#SBATCH --signal=SIGINT                          # exit with SIGINT to let gem5 save stats 

RUNAHEAD_DIR="$HOME/gem5-runahead"
TEST_SCRIPT="$RUNAHEAD_DIR/gem5-extensions/configs/test/test_re.py"

# create the log directory
LOG_DIR="$RUNAHEAD_DIR/jobs/testlogs"
if ! [[ -d "$LOG_DIR" ]]; then
    mkdir -p "$LOG_DIR"
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

cd $RUNAHEAD_DIR
source venv/bin/activate
echo "--- python packages ---"
pip freeze

SIZE=2
echo
echo "job: test run of gem5 (SE mode) - $SIZE x $SIZE matrix multiplication"
echo "time: $(date)"
echo "--- start job ---"

# Debug functions:
# schedBreak(tick) - schedule SIGTRAP at tick
# setDebugFlag("flag") - set a debug flag
# clearDebugFlag("flag") - clear a debug flag
# eventqDump() - print all events in the event queue
# takeCheckpoint(tick) - write a checkpoint at tick
# SimObject::find("system.qualified.name") - use FQN to get a pointer to the specified simobject
#
# use schedBreak(<tick>) when connected to target
GDBSERVER=$HOME/gdb-13.2/gdbserver/gdbserver
$GDBSERVER localhost:34612 \
    ./gem5/build/X86/gem5.debug --debug-break=1000 \
    --debug-start=0 \
    --debug-flags=Runahead,O3CPUAll,X86,TLB \
    --outdir $M5_OUT_DIR \
    $TEST_SCRIPT --size=$SIZE \
    > $SIMOUT_FILE
