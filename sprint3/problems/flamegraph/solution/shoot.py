import argparse
import subprocess
import time
import random
import shlex
import os
import signal

RANDOM_LIMIT = 1000
SEED = 123456789
random.seed(SEED)

AMMUNITION = [
    'localhost:8080/api/v1/maps/map1',
    'localhost:8080/api/v1/maps'
]

SHOOT_COUNT = 100
COOLDOWN = 0.1

# Путь к каталогу FlameGraph (лежит рядом со скриптом)
FLAMEGRAPH_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'FlameGraph')
STACKCOLLAPSE_PERF = os.path.join(FLAMEGRAPH_DIR, 'stackcollapse-perf.pl')
FLAMEGRAPH_PL = os.path.join(FLAMEGRAPH_DIR, 'flamegraph.pl')


def start_server():
    parser = argparse.ArgumentParser()
    parser.add_argument('server', type=str)
    return parser.parse_args().server


def run(command, output=None):
    process = subprocess.Popen(shlex.split(command), stdout=output, stderr=subprocess.DEVNULL)
    return process


def stop(process, wait=False):
    if process.poll() is None and wait:
        process.wait()
    process.terminate()


def shoot(ammo):
    hit = run('curl ' + ammo, output=subprocess.DEVNULL)
    time.sleep(COOLDOWN)
    stop(hit, wait=True)


def make_shots():
    for _ in range(SHOOT_COUNT):
        ammo_number = random.randrange(RANDOM_LIMIT) % len(AMMUNITION)
        shoot(AMMUNITION[ammo_number])
    print('Shooting complete')


# Запускаем сервер
server = run(start_server())

# Запускаем perf record для процесса сервера
print(f'Starting perf record for PID {server.pid}')
perf_record = run(f'perf record -o perf.data -p {server.pid} -g')
# Даём perf время на инициализацию
time.sleep(0.5)

# Обстреливаем сервер запросами
make_shots()

# Останавливаем perf record (посылаем SIGINT, чтобы он корректно завершил запись)
perf_record.send_signal(signal.SIGINT)
perf_record.wait()
print('perf record finished')

# Останавливаем сервер
stop(server)
time.sleep(1)

# Строим флеймграф по собранным данным
print('Generating flamegraph...')

# perf script -> out.perf
with open('out.perf', 'w') as f:
    subprocess.run(['perf', 'script', '-i', 'perf.data'], stdout=f, stderr=subprocess.PIPE, check=True)

# stackcollapse-perf.pl -> out.folded
with open('out.folded', 'w') as f:
    subprocess.run([STACKCOLLAPSE_PERF, 'out.perf'], stdout=f, stderr=subprocess.PIPE, check=True)

# flamegraph.pl -> graph.svg
with open('graph.svg', 'w') as f:
    subprocess.run([FLAMEGRAPH_PL, 'out.folded'], stdout=f, stderr=subprocess.PIPE, check=True)

print('Flamegraph saved to graph.svg')
print('Job done')