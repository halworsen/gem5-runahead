#!/bin/bash
# for use by launch.json
# submits the given jobscript to SLURM and waits for it to start to extract the job ID and worker node
# assumes it's running a gdbserver and extracts the gdbserver port and process PID

JOB_SCRIPT=$1
SLURM_LOG_FILE=$HOME/gem5-runahead/jobs/testlogs/gem5-test_slurm.log

# queue job & wait for start
JOB_ID=$(sbatch $JOB_SCRIPT | sed 's/Submitted batch job \([0-9]\+\)$/\1/')
STATE=$(squeue -j $JOB_ID -u markuswh --Format=State --noheader | tr -d ' ')
while [[ "$STATE" != "RUNNING" ]]; do
    sleep 2
    STATE=$(squeue -j $JOB_ID -u markuswh --Format=State --noheader | tr -d ' ')
done
NODE=$(squeue -j $JOB_ID -u markuswh --Format=NodeList --noheader | tr -d ' ')

# wait for gdb server to spin up, then extract the port
PORT=$(cat $SLURM_LOG_FILE | sed -n 's/Listening on port \([0-9]\+\)/\1/gp' | head -n 1)
while [[ -z "$PORT" ]]; do
    sleep 5
    PORT=$(cat $SLURM_LOG_FILE | sed -n 's/Listening on port \([0-9]\+\)/\1/gp' | head -n 1)
done
PID=$(cat $SLURM_LOG_FILE | sed -n 's/Process .* created; pid = \([0-9]\+\)/\1/gp' | head -n 1)

echo "$NODE:$PORT|$NODE:$PORT|Target|Job $JOB_ID is $STATE on $NODE (PID $PID)"
