"""Exercise the actual CLI, blocking EventLoop wakeup, and signal shutdown."""
import os
import socket
import subprocess
import sys
import tempfile
import time

with tempfile.TemporaryFile() as log:
    child = subprocess.Popen([sys.argv[1], '0', '--calc'],
                             stdout=subprocess.DEVNULL, stderr=log)
    try:
        deadline = time.monotonic() + 5
        while True:
            message = os.pread(log.fileno(), 65536, 0).decode(errors='replace')
            assert child.poll() is None, message
            if message.startswith('LISTENING ') and '\n' in message:
                port = int(message.splitlines()[0].split()[1])
                break
            assert time.monotonic() < deadline, 'startup timeout: ' + message
            time.sleep(.01)
        with socket.create_connection(('127.0.0.1', port), timeout=3) as client:
            client.sendall(b'CALC 1000000\nCALC 10\n')
            client.shutdown(socket.SHUT_WR)
            result = bytearray()
            while True:
                part = client.recv(1024)
                if not part:
                    break
                result.extend(part)
                assert len(result) <= 100, 'unexpected response growth'
            assert result == b'RESULT 78498\nRESULT 4\n', result
        child.terminate()
        assert child.wait(timeout=5) == 0, 'unclean SIGTERM exit'
    finally:
        if child.poll() is None:
            child.kill()
            child.wait(timeout=3)
        log.seek(0)
        message = log.read().decode(errors='replace')
        assert not any(marker in message for marker in
                       ['ERROR: AddressSanitizer', 'runtime error:', 'LeakSanitizer']), message
print('PASS --calc CLI, real worker wakeup, ordered responses, half-close, SIGTERM exit 0')
