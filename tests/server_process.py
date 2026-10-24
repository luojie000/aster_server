"""Start only our own child, with an OS-assigned port and bounded startup wait."""
from contextlib import contextmanager
import subprocess
import tempfile
import time

@contextmanager
def running_server(binary):
    with tempfile.TemporaryFile(mode='w+b') as errors:
        process = subprocess.Popen([binary, '0'], stdout=subprocess.DEVNULL, stderr=errors)
        try:
            deadline = time.monotonic() + 5
            while time.monotonic() < deadline:
                # pread avoids changing the file offset shared with the child.
                import os
                message = os.pread(errors.fileno(), 65536, 0).decode(errors='replace')
                if process.poll() is not None:
                    raise RuntimeError('server exited at startup: ' + message)
                if message.startswith('LISTENING ') and '\n' in message:
                    port = int(message.splitlines()[0].split()[1])
                    break
                time.sleep(.01)
            else:
                raise RuntimeError('startup timeout: ' + message)
            yield process, port
            if process.poll() is not None:
                raise RuntimeError('server exited unexpectedly')
        finally:
            if process.poll() is None:
                process.terminate()
            try:
                process.wait(timeout=3)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
            errors.seek(0)
            message = errors.read().decode(errors='replace')
            if 'ERROR: AddressSanitizer' in message or 'runtime error:' in message or 'LeakSanitizer' in message:
                raise RuntimeError(message)
