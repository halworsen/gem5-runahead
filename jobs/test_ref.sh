#!/bin/sh
#SBATCH --job-name="gem5-test-ref"
#SBATCH --account=share-ie-idi
#SBATCH --mail-type=ALL
#SBATCH --output=/dev/null                       # output is manually redirected
#SBATCH --partition=CPUQ
#SBATCH --nodes=1                                # 1 compute node
#SBATCH --cpus-per-task=4                        # 4 cores
#SBATCH --mem=2000
#SBATCH --time=06:00:00
#SBATCH --signal=SIGINT                          # exit with SIGINT to let gem5 save stats 

RUNAHEAD_DIR="$HOME/gem5-runahead"
TEST_SCRIPT="$RUNAHEAD_DIR/gem5-extensions/configs/test/test_re.py"

# create the log directory
LOG_DIR="$RUNAHEAD_DIR/jobs/testlogs"
if ! [[ -d "$LOG_DIR" ]]; then
    mkdir -p "$LOG_DIR"
fi

M5_OUT_DIR="$LOG_DIR/m5out-${SLURM_JOB_NAME}"
SIMOUT_FILE="$LOG_DIR/${SLURM_JOB_NAME}_simout.log"
SLURM_LOG_FILE="$LOG_DIR/${SLURM_JOB_NAME}_slurm.log"

# redirect all output to the slurm logfile
exec &> $SLURM_LOG_FILE

echo "--- loading modules ---"
module --quiet purge
module restore gem5
module list

cd $HOME/gem5-runahead
source venv/bin/activate
echo "--- python packages ---"
pip freeze

SIZE=$1
RANDOM=1
echo
echo "job: test run of gem5 (SE mode) - $SIZE x $SIZE matrix multiplication"
echo "time: $(date)"
echo "--- start job ---"

./gem5/build/X86/gem5.fast --outdir $M5_OUT_DIR \
    $TEST_SCRIPT --size=$SIZE --random=$RANDOM --no-l3 --no-runahead \
    > $SIMOUT_FILE
