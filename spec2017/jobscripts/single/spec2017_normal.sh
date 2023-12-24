#!/bin/sh
#SBATCH --job-name="spec2017-dbg"
#SBATCH --account=ie-idi
#SBATCH --mail-type=ALL
#SBATCH --output=/dev/null
#SBATCH --exclude=idun-02-45,idun-02-49
#SBATCH --partition=CPUQ
#SBATCH --nodes=1
#SBATCH --cpus-per-task=2
#SBATCH --mem=5000
#SBATCH --time=7-06:00:00
#SBATCH --signal=B:SIGINT@120

#
# Restore from a checkpoint then switch cores to the runahead CPU for simulation
#

declare -A CHECKPOINTS

CHECKPOINTS=(
    ["perlbench_s_0"]="cpt_10394048025897_sp-5_interval-132_insts-13200000000_warmup-1000000"
    ["perlbench_s_1"]="cpt_2748881699511_sp-2_interval-21_insts-2100000000_warmup-1000000"
    ["perlbench_s_2"]="cpt_6795593793953_sp-3_interval-80_insts-8000000000_warmup-1000000"
    ["gcc_s_1"]="cpt_25257149893251_sp-9_interval-344_insts-34400000000_warmup-1000000"
    ["gcc_s_2"]="cpt_8421685402194_sp-0_interval-101_insts-10100000000_warmup-1000000"
    ["mcf_s_0"]="cpt_8838793351635_sp-3_interval-122_insts-12200000000_warmup-1000000"
    ["cactuBSSN_s_0"]="cpt_22334708673673_sp-5_interval-323_insts-32300000000_warmup-1000000"
    ["omnetpp_s_0"]="cpt_2742825868472_sp-2_interval-21_insts-2100000000_warmup-1000000"
    ["wrf_s_0"]="cpt_3243120001002_sp-3_interval-14_insts-1400000000_warmup-1000000"
    ["xalancbmk_s_0"]="cpt_23259882747842_sp-2_interval-313_insts-31300000000_warmup-1000000"
    ["x264_s_0"]="cpt_8240200219449_sp-0_interval-113_insts-11300000000_warmup-1000000"
    ["x264_s_2"]="cpt_18329730381647_sp-0_interval-271_insts-27100000000_warmup-1000000"
    ["imagick_s_0"]="cpt_30896551699522_sp-2_interval-438_insts-43800000000_warmup-1000000"
    ["nab_s_0"]="cpt_3307569092009_sp-1_interval-29_insts-2900000000_warmup-1000000"
    ["exchange2_s_0"]="cpt_18364915843768_sp-9_interval-226_insts-22600000000_warmup-1000000"
    ["fotonik3d_s_0"]="cpt_35309091816902_sp-0_interval-473_insts-47300000000_warmup-1000000"
)

SPEC2017_DIR=/cluster/home/markuswh/gem5-runahead/spec2017
RUNSCRIPT_DIR="$SPEC2017_DIR/runscripts"
BENCHMARK=$1
CHECKPOINT=${CHECKPOINTS[$BENCHMARK]}
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
echo "job: simulate SPEC2017 benchmark at simpoint - $BENCHMARK"
echo "time: $(date)"
echo "--- start job ---"

FSPARAMS=(
    "--kernel=$SPEC2017_DIR/plinux"
    "--image=$SPEC2017_DIR/x86-3.img"
    "--script=$RUNSCRIPT"
    "--max-insts=101000000" # max 101M insts (warmup + simpoint interval)
    "--clock=3.2GHz"

    # Instantiate using the given checkpoint
    "--restore-checkpoint=$M5_OUT_DIR/../m5out-spec2017-sp-chkpt-all/$CHECKPOINT"

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

# Parse simulation statistics to JSON
echo "--- simulation end ---"
echo "parsing simulation statistics"

STAT_PARSE_SCRIPT=/cluster/home/markuswh/gem5-runahead/scripts/stats/statdump.py
PARSED_STATS_NAME=gem5stats.json

python $STAT_PARSE_SCRIPT \
    --format json \
    --out $M5_OUT_DIR/$PARSED_STATS_NAME \
    $M5_OUT_DIR/stats.txt

# Move simout and SLURM output
mv $SIMOUT_FILE $M5_OUT_DIR
mv $SLURM_LOG_FILE $M5_OUT_DIR
