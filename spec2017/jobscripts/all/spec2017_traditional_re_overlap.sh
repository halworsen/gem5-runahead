#!/bin/sh
#SBATCH --job-name="gem5-spec2017-bench-traditional-re-overlap"
#SBATCH --account=ie-idi
#SBATCH --mail-type=ALL
#SBATCH --output=/dev/null
#SBATCH --array=1-16
#SBATCH --exclude=idun-02-45,idun-02-49
#SBATCH --partition=CPUQ
#SBATCH --nodes=1
#SBATCH --cpus-per-task=2
#SBATCH --mem=6000
#SBATCH --time=7-06:00:00
#SBATCH --exclude=idun-02-45
#SBATCH --signal=B:SIGINT@120

#
# Restore from a checkpoint then switch cores to the runahead CPU for simulation
#

ALL_BENCHMARKS=(
    "cactuBSSN_s_0"
    "exchange2_s_0"
    "fotonik3d_s_0"
    "gcc_s_1" "gcc_s_2"
    "imagick_s_0"
    "mcf_s_0"
    "nab_s_0"
    "omnetpp_s_0"
    "perlbench_s_0" "perlbench_s_1" "perlbench_s_2"
    "wrf_s_0"
    "x264_s_0"
    "x264_s_2"
    "xalancbmk_s_0"
)

declare -A CHECKPOINTS
CHECKPOINTS=(
    ["perlbench_s_0"]="cpt_26723810289983_sp-1_interval-369_insts-36900000000_warmup-1000000"
    ["perlbench_s_1"]="cpt_33681194937967_sp-1_interval-462_insts-46200000000_warmup-1000000"
    ["perlbench_s_2"]="cpt_31705186092754_sp-2_interval-430_insts-43000000000_warmup-1000000"
    ["gcc_s_1"]="cpt_17042282256812_sp-5_interval-226_insts-22600000000_warmup-1000000"
    ["gcc_s_2"]="cpt_13527511076677_sp-6_interval-175_insts-17500000000_warmup-1000000"
    ["mcf_s_0"]="cpt_19962348413297_sp-4_interval-303_insts-30300000000_warmup-1000000"
    ["cactuBSSN_s_0"]="cpt_3967868295569_sp-8_interval-42_insts-4200000000_warmup-1000000"
    ["omnetpp_s_0"]="cpt_11595386405228_sp-0_interval-147_insts-14700000000_warmup-1000000"
    ["wrf_s_0"]="cpt_1843422166344_sp-2_interval-9_insts-900000000_warmup-1000000"
    ["xalancbmk_s_0"]="cpt_6663676485929_sp-3_interval-78_insts-7800000000_warmup-1000000"
    ["x264_s_0"]="cpt_17538057313183_sp-6_interval-262_insts-26200000000_warmup-1000000"
    ["x264_s_2"]="cpt_19291726768026_sp-1_interval-286_insts-28600000000_warmup-1000000"
    ["imagick_s_0"]="cpt_8960565880504_sp-1_interval-114_insts-11400000000_warmup-1000000"
    ["nab_s_0"]="cpt_17177317375364_sp-0_interval-242_insts-24200000000_warmup-1000000"
    ["exchange2_s_0"]="cpt_35840173871729_sp-2_interval-456_insts-45600000000_warmup-1000000"
    ["fotonik3d_s_0"]="cpt_35309091816902_sp-0_interval-473_insts-47300000000_warmup-1000000"
)

SPEC2017_DIR=/cluster/home/markuswh/gem5-runahead/spec2017
RUNSCRIPT_DIR="$SPEC2017_DIR/runscripts"
BENCHMARK=${ALL_BENCHMARKS[$SLURM_ARRAY_TASK_ID - 1]}
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
echo "node: $(hostname)"
echo "time: $(date)"
echo "--- start job ---"

FSPARAMS=(
    "--kernel=$SPEC2017_DIR/plinux"
    "--image=$SPEC2017_DIR/x86-3.img"
    "--script=$RUNSCRIPT"
    "--max-insts=101000000" # max 101M insts (warmup + simpoint interval)
    "--clock=3.2GHz"

    # Instantiate using the given checkpoint
    "--restore-checkpoint=$M5_OUT_DIR/../m5out-gem5-spec2017-sp-chkpt-all/$CHECKPOINT"

    # Runahead options
    "--lll-threshold=3"
    "--rcache-size=2kB"
    "--lll-latency-threshold=250"
    "--overlapping-runahead"
    "--runahead-exit-policy=Eager"

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
