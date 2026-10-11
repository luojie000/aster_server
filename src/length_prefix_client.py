import socket
import struct


sock = socket.socket()

sock.connect(
    ("127.0.0.1",8080)
)


msg = b"hello"


data = (
    struct.pack("!I",len(msg))
    +
    msg
)


sock.sendall(data)


sock.close()