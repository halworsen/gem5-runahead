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

BENCHMARK="gcc_s_2"
CHECKPOINT=${CHECKPOINTS[$BENCHMARK]}
SPEC2017_DIR="/cluster/home/markuswh/gem5-runahead/spec2017"
RUNSCRIPT_DIR="$SPEC2017_DIR/runscripts"
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
    "--max-insts=101000000" # max 101M insts (warmup + simpoint interval)
    "--clock=3.2GHz"

    # Instantiate using the given checkpoint
    "--restore-checkpoint=$SPEC2017_DIR/logs/$BENCHMARK/m5out-gem5-spec2017-sp-chkpt-all/$CHECKPOINT"

    # Runahead options
    "--lll-threshold=3"
    "--rcache-size=2kB"
    "--lll-latency-threshold=100"
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
    --debug-flags=Runahead,O3CPUAll \
    --outdir $M5_OUT_DIR \
    $CONFIG_SCRIPT $CONFIG_PARAMS \
    > $SIMOUT_FILE
