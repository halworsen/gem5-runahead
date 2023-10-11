import os

def parse_simpoints(args, highest_weight_only=False) -> list:
    simpoint_file = os.path.join(args.simpoint_checkpoints, 'simpoint.txt')
    weights_file = os.path.join(args.simpoint_checkpoints, 'weights.txt')
    simpoints = []
    weights = []

    with open(weights_file, 'r') as f:
        for line in f.readlines():
            if not line:
                continue
            weight_data = line.split()
            weight = float(weight_data[0])
            # simpoint weights are already sorted by ID so don't really need this
            # sp_id = int(weight_data[1])
            weights.append(weight)
    
    # find the simpoint ID corresponding to the highest weight
    highest_weight_sp = weights.index(max(weights))
    with open(simpoint_file, 'r') as f:
        for line in f.readlines():
            if not line:
                continue

            sp_data = tuple(map(int, line.split()))
            # Convert interval number to #insts
            # Account for booting.
            sp_insts = (sp_data[0] * args.simpoint_interval)
            if not highest_weight_only or (sp_data[1] == highest_weight_sp):
                simpoints.append({
                    'id': sp_data[1],
                    'interval': sp_data[0],
                    'weight': weights[sp_data[1]],
                    'insts': sp_insts,
                    'warmup': int(args.warmup_insts),
                })

    return sorted(simpoints, key=lambda sp: sp['insts'])
