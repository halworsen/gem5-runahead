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
    Simulate on a detailed core starting at a given checkpoint
    '''
    print('Restoring state from checkpoint. Switching to runahead CPU')

    root.system.processor.switch()

    exit_event = m5.simulate()
    tick = m5.curTick()
    cause = exit_event.getCause()

    simstats = m5.stats.gem5stats.get_simstat(root).to_json()
    insts = int(simstats['system']['processor']['cores1']['core']['committedInsts']['0']['value'])
    print(f'Simmulated {insts} instructions in {tick} ticks')

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