#!/bin/bash
# for use by tasks.json
# runs the given jobscript and extracts job ID, worker node, gdb port and process PID
# everything is recorded in a .env file and echo'd verbosely

JOB_SCRIPT=$1
SLURM_LOG_FILE=$HOME/gem5-runahead/jobs/testlogs/gem5-test_slurm.log
VSCODE_ENV=$HOME/gem5-runahead/.vscode/.gem5_env

# queue job & wait for start
JOB_ID=$(sbatch $JOB_SCRIPT | sed 's/Submitted batch job \([0-9]\+\)$/\1/')
echo "Submitted job $JOB_ID"
echo "JOB_ID=$JOB_ID" > $VSCODE_ENV

STATE=$(squeue -j $JOB_ID -u markuswh --Format=State --noheader | tr -d ' ')
while [[ "$STATE" != "RUNNING" ]]; do
    echo "Waiting for job to start... (state: $STATE)"
    sleep 5
    STATE=$(squeue -j $JOB_ID -u markuswh --Format=State --noheader | tr -d ' ')
done
JOB_NODE=$(squeue -j $JOB_ID -u markuswh --Format=NodeList --noheader | tr -d ' ')
echo "Job $JOB_ID running on $JOB_NODE"
echo "JOB_NODE=$JOB_NODE" >> $VSCODE_ENV

# wait for gdb server to spin up, then extract the port
PORT=$(cat $SLURM_LOG_FILE | sed -n 's/Listening on port \([0-9]\+\)/\1/gp' | head -n 1)
while [[ -z "$PORT" ]]; do
    echo "Waiting for gdbserver port..."
    sleep 5
    PORT=$(cat $SLURM_LOG_FILE | sed -n 's/Listening on port \([0-9]\+\)/\1/gp' | head -n 1)
done
PID=$(cat $SLURM_LOG_FILE | sed -n 's/Process .* created; pid = \([0-9]\+\)/\1/gp' | head -n 1)
echo "Job $JOB_ID is running on $JOB_NODE (pid $PID)"
echo "JOB_PID=$PID" >> $VSCODE_ENV
echo "GDB_PORT=$PORT" >> $VSCODE_ENV
