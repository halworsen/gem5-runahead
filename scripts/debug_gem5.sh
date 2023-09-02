#!/bin/bash
# for commandline use
# starts the gem5_gdb job and attaches GDB to it

GEM5=$HOME/gem5-runahead/gem5/build/X86/gem5.debug
JOB_SCRIPT=$HOME/gem5-runahead/jobs/gem5_gdb.sh
SLURM_LOG_FILE=$HOME/gem5-runahead/jobs/testlogs/gem5-test_slurm.log
GDB=$HOME/gdb-13.2/gdb/gdb

# queue job & wait for start
JOB_ID=$(sbatch $JOB_SCRIPT $1 | sed 's/Submitted batch job \([0-9]\+\)$/\1/')
echo "Job ID is $JOB_ID"

STATE=$(squeue -j $JOB_ID -u markuswh --Format=State --noheader | tr -d ' ')
while [[ "$STATE" != "RUNNING" ]]; do
    echo "Waiting for job to start... (state: $STATE)"
    sleep 5
    STATE=$(squeue -j $JOB_ID -u markuswh --Format=State --noheader | tr -d ' ')
done
NODE=$(squeue -j $JOB_ID -u markuswh --Format=NodeList --noheader | tr -d ' ')
echo "Job running on $NODE"

# wait for gdb server to spin up, then extract the port
PORT=$(cat $SLURM_LOG_FILE | sed -n 's/Listening on port \([0-9]\+\)/\1/gp' | head -n 1)
while [[ -z "$PORT" ]]; do
    echo "Waiting for gdbserver to start..."
    sleep 5
    PORT=$(cat $SLURM_LOG_FILE | sed -n 's/Listening on port \([0-9]\+\)/\1/gp' | head -n 1)
done
PID=$(cat $SLURM_LOG_FILE | sed -n 's/Process .* created; pid = \([0-9]\+\)/\1/gp' | head -n 1)
echo "gdbserver running on $NODE:$PORT. Starting gdb and connecting..."

# run gdb and attach to the job's gdbserver
$GDB $GEM5 -ex "target remote $NODE:$PORT"

scancel $JOB_ID
echo "Cancelled job $JOB_ID"
