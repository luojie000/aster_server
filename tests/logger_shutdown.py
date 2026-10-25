"""Check signal shutdown drains logs and never emits client-controlled payloads."""
import os
import signal
import socket
import subprocess
import sys
import tempfile
import time

binary = sys.argv[1]
payload = b'PRIVATE_CLIENT_PAYLOAD\x00\n[ERROR] forged-message\xff'
for sig in (signal.SIGTERM, signal.SIGINT):
    with tempfile.TemporaryFile() as errors, tempfile.TemporaryFile() as output:
        p = subprocess.Popen([binary, '0'], stdout=output, stderr=errors)
        try:
            deadline = time.monotonic() + 5
            while True:
                data = os.pread(errors.fileno(), 65536, 0)
                if b'\n' in data:
                    assert data.startswith(b'LISTENING '), data
                    port = int(data.splitlines()[0].split()[1])
                    break
                assert p.poll() is None, data
                assert time.monotonic() < deadline, 'startup timed out'
                time.sleep(.01)
            with socket.create_connection(('127.0.0.1', port), timeout=3) as peer:
                peer.sendall(payload)
                received = b''
                while len(received) < len(payload):
                    chunk = peer.recv(len(payload) - len(received))
                    assert chunk
                    received += chunk
                assert received == payload
                # Signal with a live connection, not only an idle server.
                p.send_signal(sig)
                assert p.wait(timeout=5) == 0
            errors.seek(0)
            data = errors.read()
            output.seek(0)
            stdout = output.read()
            assert b'connection accepted fd=' in data, data
            assert f'shutdown requested signal={sig.value}'.encode() in data, data
            assert data.endswith(b'[INFO] server stopped\n'), data
            assert b'PRIVATE_CLIENT_PAYLOAD' not in data + stdout
            assert b'forged-message' not in data + stdout
            assert stdout == b'', stdout
            print(f'PASS {sig.name}: live connection, logs drained, no payload output')
        finally:
            if p.poll() is None:
                p.kill()
                p.wait()

for arg in ('invalid', '-1', '65536'):
    result = subprocess.run([binary, arg], capture_output=True, timeout=5)
    assert result.returncode == 1, result
    assert b'aster_server:' in result.stderr, result
    assert b'LISTENING' not in result.stderr, result
print('PASS fatal startup diagnostics')
