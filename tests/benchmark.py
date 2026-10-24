"""Modest localhost echo baseline; verifies every response. Not a capacity test."""
import concurrent.futures
import json
import socket
import statistics
import sys
import threading
import time
from server_process import running_server

def run(port, clients, iterations=500, size=1024):
    barrier = threading.Barrier(clients + 1, timeout=10)
    def worker(index):
        payload = bytes([index]) * size
        samples = []
        with socket.create_connection(('127.0.0.1', port), timeout=5) as s:
            s.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
            def request():
                s.sendall(payload)
                result = bytearray()
                while len(result) < size:
                    data = s.recv(size - len(result))
                    if not data:
                        raise RuntimeError('unexpected EOF')
                    result.extend(data)
                if result != payload:
                    raise RuntimeError('echo mismatch')
            for _ in range(20):
                request()
            barrier.wait()
            for _ in range(iterations):
                start = time.perf_counter()
                request()
                samples.append((time.perf_counter() - start) * 1000)
        return samples
    with concurrent.futures.ThreadPoolExecutor(max_workers=clients) as pool:
        futures = [pool.submit(worker, i) for i in range(clients)]
        started = time.perf_counter()
        barrier.wait()
        started = time.perf_counter()
        samples = [v for future in futures for v in future.result()]
        elapsed = time.perf_counter() - started
    samples.sort()
    return dict(clients=clients, requests=clients * iterations, bytes_per_request=size,
                seconds=round(elapsed, 4), requests_per_second=round(len(samples) / elapsed),
                echo_payload_mib_per_second=round(len(samples) * size / elapsed / 2**20, 2),
                latency_p50_ms=round(statistics.median(samples), 3),
                latency_p95_ms=round(samples[int(.95 * (len(samples)-1))], 3))

with running_server(sys.argv[1]) as (_, port):
    results = [dict(repeat=repeat, **run(port, clients))
               for clients in (1, 8) for repeat in range(1, 4)]
print(json.dumps(results, indent=2))
