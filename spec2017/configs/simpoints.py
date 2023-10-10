def parse_simpoints(args) -> list:
    simpoint_file = args.simpoint_checkpoints
    simpoints = []
    with open(simpoint_file, 'r') as f:
        for line in f.readlines():
            if not line:
                continue

            sp_data = tuple(map(int, line.split()))
            # Convert interval number to #insts
            # Account for booting.
            sp_insts = (sp_data[0] * args.simpoint_interval)
            simpoints.append({
                'id': sp_data[1],
                'interval': sp_data[0],
                'insts': sp_insts,
                'warmup': int(args.warmup_insts),
            })

    return sorted(simpoints, key=lambda sp: sp['insts'])
