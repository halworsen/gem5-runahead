import m5
from datetime import datetime
from simpoints import parse_simpoints
import os

def sim_fs_normal(root, args, switch_core=True):
    '''
    Boot, optionally switch cores, then resume simulation
    '''
    print('Performing boot...')
    exit_event = m5.simulate()
    tick = m5.curTick()
    cause = exit_event.getCause()

    if cause == 'm5_exit instruction encountered':
        print(f'Boot completed @ {datetime.now()}')
        print(f'Sim tick: t{tick}')

        # get the stats as a dict (idk why this function is called to_json)
        simstats = m5.stats.gem5stats.get_simstat(root).to_json()
        boot_insts = int(simstats['system']['processor']['cores0']['core']\
                        ['exec_context.thread_0']['numInsts']['value'])
        print(f'Boot finished in {boot_insts} instructions')

        print('Resetting simulation statistics...')

        m5.stats.dump()
        m5.stats.reset()

        if switch_core:
            print('Switching processor core...')
            root.system.processor.switch()
    # Something went wrong
    else:
        print(f'Unexpected exit occured @ t{tick}')
        print(f'Exit cause: {cause}')
        exit(1)

    print('Resuming simulation at workload...')
    exit_event = m5.simulate()
    tick = m5.curTick()
    cause = exit_event.getCause()

    return (exit_event, tick, cause)

def sim_fs_from_checkpoint(root, args):
    '''
    Simulate on a detailed core starting at a given checkpoint. Includes a lot of sanity checking
    '''
    start_tick = m5.curTick()
    print(f'Restoring state from checkpoint at tick {start_tick}')

    # We have to simulate with the atomic CPU for a very short period
    # If we don't, the runahead CPU model will try to startup AND take over from the simple CPU at once
    # If that happens in the wrong order (random), things will break explicitly
    restart_period = 100
    print(f'Simulating for {restart_period} (restart/initialization period)')
    exit_event = m5.simulate(restart_period)
    tick = m5.curTick()

    simstats = m5.stats.gem5stats.get_simstat(root).to_json()
    insts = int(simstats['system']['processor']['cores0']['core']['exec_context.thread_0']['numInsts']['value'])
    print(f'Simulated {insts} instructions in {tick - start_tick} ticks (init period)')

    root.system.processor.switch()

    # But things can also break silently. In some cases, checkpoint restores leave the
    # detailed CPU model in a stall. Forever. So we simulate in smaller chunks and regularly
    # check if the CPU has stalled
    prev_insts = 0
    prev_tick = tick
    # The last sim interval is used for any sim periods beyond the length of this list
    sim_intervals = [5000000000]
    it = sim_intervals.__iter__()
    while True:
        try:
            sim_interval = next(it)
        except StopIteration:
            sim_interval = sim_intervals[-1]

        if sim_interval <= 0:
            break

        print(f'Simulating for {sim_interval} ticks')
        exit_event = m5.simulate(sim_interval)
        tick = m5.curTick()
        cause = exit_event.getCause()

        simstats = m5.stats.gem5stats.get_simstat(root).to_json()
        insts = int(simstats['system']['processor']['cores1']['core']['committedInsts']['0']['value'])
        print(f'Simulated {insts - prev_insts} instructions in {tick - prev_tick} ticks - {cause}')
        print(f'Progress: {insts}/{args.max_insts} insts ({(insts/args.max_insts)*100:.2f}%)')

        if cause == 'a thread reached the max instruction count':
            break

        if insts == prev_insts:
            print('The simulation stalled. Try again.')
            exit(1)

        prev_insts = insts
        prev_tick = tick

    print('Finished simulating!')
    print(f'Simulated a total of {insts} insts in {tick - start_tick} ticks')

    return (exit_event, tick, cause)


def sim_fs_simpoint_checkpoints(root, args):
    '''
    Boot, then simulate until the start of each simpoint and take checkpoints
    Checkpoints are never taken before boot has finished
    Although the system should be setup in the same way as a detailed run, we stay on the simple core
    '''
    simpoints = parse_simpoints(args, highest_weight_only=True)
    boot_complete = False

    exit_event = None
    tick = -1
    cause = 'N/A'

    i = 0
    while i < len(simpoints):
        sp = simpoints[i]
        print(f'Simulating until simpoint #{sp["id"]} ({sp["insts"] - sp["warmup"]} insts)...')
        exit_event = m5.simulate()
        tick = m5.curTick()
        cause = exit_event.getCause()

        simstats = m5.stats.gem5stats.get_simstat(root).to_json()
        processor_stats = simstats['system']['processor']
        # either cores or cores0, dunno why it changes
        cores_key = filter(lambda k: 'cores' in k, processor_stats.keys()).__next__()
        insts_simulated = int(processor_stats[cores_key]['core']['exec_context.thread_0']['numInsts']['value'])
        print(f'Simulation exited after {insts_simulated} insts')

        # Check if we exited because boot is complete
        if not boot_complete and cause == 'm5_exit instruction encountered':
            print(f'Boot completed @ {datetime.now()}')
            print('Resetting simulation statistics...')
            m5.stats.reset()
            boot_complete = True
        elif cause == 'simpoint starting point found':
            i += 1
            # If boot was not finished yet, skip the simpoint. We don't care about simulating boot
            if not boot_complete:
                print('Boot was not completed yet! Ignoring simpoint.')
                continue

            # Take the simpoint checkpoint
            cpt_name = f'cpt_{tick}_sp-{sp["id"]}_interval-{sp["interval"]}_insts-{sp["insts"]}_warmup-{sp["warmup"]}'
            print(f'Reached simpoint #{sp["id"]}, taking checkpoint ({cpt_name}).')

            checkpoint_dir = os.path.join(m5.options.outdir, cpt_name)
            m5.checkpoint(checkpoint_dir)
        else:
            print('Unexpected exit while taking simpoint checkpoints!')
            break

    return (exit_event, tick, cause)