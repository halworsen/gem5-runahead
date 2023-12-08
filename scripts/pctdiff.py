''' Find the percentage difference in stats for two different simulation runs '''

import os
import sys
import json
import queue

out_file = '/cluster/home/markuswh/gem5-runahead/pctdiff.txt'
out_file = open(out_file, 'w+')

def read_stats(bench: str, run_name: str) -> dict:
    stats_path = os.path.join(
        '/cluster/home/markuswh/gem5-runahead/spec2017/logs',
        bench,
        run_name,
        'gem5stats.json')
    with open(stats_path, 'r') as f:
        data = json.load(f)
    return data['sections'][0]['stats']

stats_a = read_stats(sys.argv[1], sys.argv[2])
stats_b = read_stats(sys.argv[1], sys.argv[3])

out_file.write(f'Percentage stat differences: {sys.argv[3]}\'s difference to {sys.argv[2]}\n\n')

# just BFS it
stat_queue = queue.Queue()
for k in stats_a.keys():
    stat_queue.put((stats_a, stats_b, k))

line_len = 150
while not stat_queue.empty():
    data_a, data_b, key = stat_queue.get()
    data = data_a[key]

    if 'statistic' in data and 'values' in data:
        stat_name = data['statistic']
        if 'bucket' in data:
            stat_name += f'::{data["bucket"]}'
        try:
            data_b = data_b[key]
            relative = data_b['values'][0] / data['values'][0]
            line = stat_name + f'{relative*100:.2f}%'.rjust(line_len - len(stat_name))
            out_file.write(f'{line}\n')
        except KeyError:
            line = stat_name + f'{data["values"][0]}'.rjust(line_len - len(stat_name))
            out_file.write(f'{line} (Not present in {sys.argv[3]})\n')
            continue
        except ZeroDivisionError:
            line = stat_name + f'{data["values"][0]} -> {data_b["values"][0]}'.rjust(line_len - len(stat_name))
            out_file.write(f'{line} (Div0)\n')
    else:
        for k in data.keys():
            stat_queue.put((data_a[key], data_b[key], k))
        

out_file.close()
