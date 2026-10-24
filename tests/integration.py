import concurrent.futures
import socket
import struct
import subprocess
import sys
import time
from server_process import running_server

def connect():
    return socket.create_connection(('127.0.0.1', port), timeout=5)

def echo(payload, half_close=False, slow=False):
    with connect() as s:
        if slow:
            s.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 32768)
        with concurrent.futures.ThreadPoolExecutor() as pool:
            def send():
                s.sendall(payload)
                if half_close:
                    s.shutdown(socket.SHUT_WR)
            sender = pool.submit(send)
            if slow:
                time.sleep(.2)
            received = bytearray()
            while len(received) < len(payload):
                chunk = s.recv(65536)
                if not chunk:
                    break
                received.extend(chunk)
            sender.result(timeout=10)
        assert received == payload, (len(received), len(payload))
        if half_close:
            assert s.recv(1) == b''

with running_server(sys.argv[1]) as (process, port):
    echo(b'hello')
    with connect() as idle:
        idle.shutdown(socket.SHUT_WR)
        assert idle.recv(1) == b''
    with connect() as persistent:
        for payload in [b'a', bytes(range(256)), b'again' * 100]:
            persistent.sendall(payload)
            received = bytearray()
            while len(received) < len(payload):
                part = persistent.recv(len(payload) - len(received))
                assert part, 'unexpected EOF'
                received.extend(part)
            assert received == payload
    print('PASS simple echo, empty half-close, persistent connection', flush=True)
    echo(b'half-close regression\x00' * 300, half_close=True)
    print('PASS half-close preserves echo', flush=True)
    echo(bytes(range(256)) * 32768, half_close=True, slow=True)
    print('PASS 8 MiB binary echo with slow reader', flush=True)
    with concurrent.futures.ThreadPoolExecutor(max_workers=8) as pool:
        list(pool.map(lambda i: echo(bytes([i]) * 32000), range(32)))
    print('PASS concurrent clients', flush=True)
    for _ in range(50):
        with connect() as s:
            s.sendall(b'x' * 8192)
            s.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, struct.pack('ii', 1, 0))
    echo(b'alive after reset', half_close=True)
    assert process.poll() is None
    print('PASS resets and reconnect', flush=True)
