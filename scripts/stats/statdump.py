'''
Convert series of gem5 simulation statistics into pickled format
'''

import re
import argparse
import os
import pickle
import json
import math

# Statistic section header/footer
STATS_HEADER = '---------- Begin Simulation Statistics ----------'
STATS_FOOTER = '---------- End Simulation Statistics   ----------'
# Captures statistic name
STAT_NAME_PAT = re.compile(r'^(?P<name>[\[\]\w.+-]+)(?:::)?(?:\s+)?')
# Captures distribution buckets
STAT_BUCKET_PAT = re.compile(r'^::(?P<bucket>[\[\]\w.+-]+)\s+')
# Captures statistic values
STAT_VALUE_PAT = re.compile(r'^(?P<values>(?:(?:[0-9e.%+-]+|nan|inf)\s+)+)')
# Captures statistic unit
UNIT_PAT = re.compile(r'.+?(?P<unit>\([\w\s/\(\)]+\))$')
# Captures statistic description
DESCRIPTION_PAT = re.compile(r'^#\s(?P<desc>.+)')


def load_stats_dump(file: str) -> dict:
    '''
    Utility function to read a pickled gem5 statistics dump.

    Returns:
        A dictionary of stored statistics
    '''
    if not os.path.exists(file):
        raise ValueError('file does not exist')
    if not file.split(os.path.extsep)[-1] == 'pkl':
        raise ValueError('file is not a pickle')

    data = {}
    with open(file, 'rb') as file:
        data = pickle.load(file)

    return data


def parse_line(line: str, fractionalize: bool, line_n: int) -> dict:
    '''
    Parse one line of statistic, returning a (nested) dict containing
    its name, value(s), description, bucket (if a dist/histogram), and unit.

    The values are always in the order: count, percentage, cumulative.
    Same as in the stats.txt file.

    The line parsing works edges-in, first capturing unit and stat name,
    then the actual statistic numbers.
    '''
    data = {
        'statistic': '',
        'description': '',
        'values': [],
        'bucket': 'N/A',
        'unit': None,
    }
    work_line = line

    # match the statistic's unit (if there is one)
    match = UNIT_PAT.match(work_line)
    if not match:
        raise ValueError('malformed statistic line:\n'
                         f'[{line_n}] {line}\n-> "{work_line}"')

    unit = match.group(1)
    unit = unit.replace('(', '').replace(')', '')
    data['unit'] = unit
    work_line = work_line[:match.start(1)]

    # match statistic name
    match = STAT_NAME_PAT.match(work_line)
    if not match:
        raise ValueError('malformed statistic line:\n'
                         f'[{line_n}] {line}\n-> "{work_line}"')

    data['statistic'] = match.group(1)
    work_line = work_line[match.end(1):]

    # match bucket/dist subname (if any)
    match = STAT_BUCKET_PAT.match(work_line)
    if match:
        data['bucket'] = match.group(1)
        work_line = work_line[match.end():]

    work_line = work_line.strip()

    # match statistic values
    match = STAT_VALUE_PAT.match(work_line)
    if not match:
        raise ValueError('malformed statistic line:\n'
                         f'[{line_n}] {line}\n-> "{work_line}"')
    data['values'] = match.group(1).split()

    # type conversion from string
    for i, value in enumerate(data['values']):
        converted = None
        if '%' in value:
            if fractionalize:
                converted = float(value[:-1]) / 100
            else:
                converted = value
        elif value == 'inf':
            converted = math.inf
        elif value == 'nan':
            converted = math.nan
        else:
            converted = float(value)
        data['values'][i] = converted

    work_line = work_line[match.end():]

    # match the statistic description
    description_match = DESCRIPTION_PAT.match(work_line)
    if not description_match:
        raise ValueError('malformed statistic line:\n'
                         f'[{line_n}] {line}\n-> "{work_line}"')

    data['description'] = description_match.group(1).strip()

    # unroll into a nested dict
    path = data['statistic'].split('.')
    if data['bucket'] != 'N/A':
        path += [data['bucket']]

    nested_data = {}
    _nest = nested_data
    for i, name in enumerate(path):
        _nest[name] = {}
        if i == len(path) - 1:
            _nest[name] = data
            break
        _nest = _nest[name]

    return nested_data


def main(opts: argparse.Namespace) -> None:
    if not os.path.exists(opts.statfile):
        print('Stats file path is invalid!')
        exit(1)

    lines = []
    with open(opts.statfile, 'r') as file:
        lines = file.readlines()

    stats_data = {
        'sectionAmount': 0,
        'sectionNames': [],
        'sections': []
    }
    cur_section = {'sectionName': 'invalid', 'stats': {}}
    cur_section_num = 0
    for n, line in enumerate(lines):
        line = line.strip()
        if not line:
            continue

        if line == STATS_HEADER:
            section_name = f'section{cur_section_num}'
            if opts.sections:
                section_name = opts.sections[cur_section_num]

            cur_section = {
                'sectionName': section_name,
                'stats': {},
            }
        elif line == STATS_FOOTER:
            stats_data['sectionAmount'] += 1
            stats_data['sectionNames'] += [cur_section['sectionName']]
            stats_data['sections'] += [cur_section]
            cur_section_num += 1
        else:
            data = parse_line(line, opts.convert_percentage, n + 1)
            # Merge the section stats with the data from the line
            _data = data
            _section_data = cur_section['stats']
            key = list(_data.keys())[0]
            while len(_data.keys()) == 1:
                if key in _section_data:
                    _section_data = _section_data[key]
                    _data = _data[key]
                    key = list(_data.keys())[0]
                else:
                    _section_data[key] = _data[key]
                    break

    exts = {'pickle': 'pkl', 'json': 'json'}
    out_path = f'{opts.out}.{exts[opts.format]}'
    if opts.format == 'pickle':
        with open(out_path, 'wb+') as file:
            pickle.dump(stats_data, file)
    elif opts.format == 'json':
        with open(out_path, 'w+') as file:
            json.dump(stats_data, file)


parser = argparse.ArgumentParser()
parser.add_argument(
    'statfile', metavar='file',
    type=str,
    help='path to stats.txt',
)
parser.add_argument(
    '--out', metavar='path',
    type=str,
    default='gem5stats',
    help='file to output the parsed statistics data to. '
         'default: gem5stats',
)
parser.add_argument(
    '--format', metavar='format',
    type=str,
    default='json',
    help='statistics dump format. '
         'valid types: pickle, json. '
         'default: json',
)
parser.add_argument(
    '--sections', metavar='names',
    type=str,
    help='comma-separated string of names to '
         'describe each statistic section by',
)
parser.add_argument(
    '--no-convert-pct',
    dest='convert_percentage',
    action='store_false',
    help='don\'t convert percentages into fractional values, e.g. 50pct instead of 0.5.',
)
parser.set_defaults(convert_percentage=True)

if __name__ == '__main__':
    opts = parser.parse_args()
    if opts.sections:
        opts.sections = opts.sections.split(',')

    main(opts)
