'''
quick and dirty script for reading simouts with the RunaheadChains debug flag
and turning them into a new file listing all unique runahead chains
'''

import sys
import os
import re

logs_dir = '/cluster/home/markuswh/gem5-runahead/spec2017/logs'
bench = sys.argv[-1]
dbg_path = os.path.join(logs_dir, bench, 'm5out-spec2017-dbg')
simout = os.path.join(dbg_path, 'spec2017-dbg_simout.log')
chains = os.path.join(dbg_path, 'chains.log')

line_pat = re.compile(r'\d+:\ssystem\.processor\.cores1\.core\.rob:\s(.+)')
entry_pat = re.compile(r'Chain entry #\d+:\s+(.+)')
all_chains = []
with open(simout, 'r+') as f:
    chain = ''
    for line in f.readlines():
        match = line_pat.match(line)
        if not match:
            continue

        content = match.group(1)
        if 'Final dependence chain' in content:
            if chain != '':
                all_chains.append(chain)
                chain = ''
            continue
        chain += (content + '\n')
    
all_chains = set(all_chains)
with open(chains, 'w+') as f:
    for n, chain in enumerate(all_chains):
        insts = chain.count('\n')
        f.write(f'Unique chain #{n+1} ({insts} insts):\n')
        f.writelines(chain)
        f.write('\n')
