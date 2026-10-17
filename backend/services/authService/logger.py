import sys
import re

COLORS = {
    'DEBUG': '\033[90m',    # серый
    'INFO':  '\033[32m',    # зелёный
    'WARNING': '\033[33m',  # жёлтый
    'ERROR': '\033[31m',    # красный
}
RESET = '\033[0m'

for line in sys.stdin:
    line = line.strip()
    if not line.startswith('tskv'):
        print(line)
        continue

    # парсим поля
    fields = {}
    for part in line.split('\t'):
        if '=' in part:
            key, _, value = part.partition('=')
            fields[key] = value

    level = fields.get('level', 'INFO')
    timestamp = fields.get('timestamp', '')
    module = fields.get('module', '')
    text = fields.get('text', '')

    color = COLORS.get(level, '')
    print(f"{color}[{level}]{RESET} {timestamp} {module}: {text}")