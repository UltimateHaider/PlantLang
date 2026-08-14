#!/usr/bin/env python3
# v0.48.33 — client driver for the LISTEN regression tests.
# Modes (argv): request PORT PATH HEADERVAL BODY | malformed PORT
# 'request'  sends a POST with an X-Probe header + body and prints
#            the status line and payload of the reply.
# 'malformed' sends a non-HTTP line, then prints the reply status.
# Connects to 127.0.0.1 with retries so the server process has time
# to open its listening socket.
import socket
import sys
import time


def connect(port):
    last = None
    for _ in range(50):
        try:
            s = socket.create_connection(("127.0.0.1", port), timeout=2)
            s.settimeout(3)
            return s
        except OSError as e:
            last = e
            time.sleep(0.1)
    raise last


def recv_all(s):
    data = b""
    while True:
        try:
            chunk = s.recv(4096)
        except socket.timeout:
            break
        if not chunk:
            break
        data += chunk
    return data


def main():
    mode = sys.argv[1]
    port = int(sys.argv[2])
    s = connect(port)
    if mode == "request":
        path = sys.argv[3]
        hval = sys.argv[4]
        body = sys.argv[5] if len(sys.argv) > 5 else ""
        req = ("POST %s HTTP/1.1\r\nHost: 127.0.0.1\r\nX-Probe: %s\r\n"
               "Content-Length: %d\r\n\r\n%s" % (path, hval, len(body), body))
        s.sendall(req.encode())
        text = recv_all(s).decode(errors="replace")
        s.close()
        status = text.split("\r\n", 1)[0] if "\r\n" in text else text
        payload = text.split("\r\n\r\n", 1)[1] if "\r\n\r\n" in text else ""
        print("client status: " + status.strip())
        print("client body: " + payload)
    elif mode == "malformed":
        s.sendall(b"ZZZ\r\n\r\n")
        text = recv_all(s).decode(errors="replace")
        s.close()
        status = text.split("\r\n", 1)[0] if "\r\n" in text else text
        print("client status: " + status.strip())


if __name__ == "__main__":
    main()