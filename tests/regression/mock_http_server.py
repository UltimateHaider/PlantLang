#!/usr/bin/env python3
"""Mock HTTP server for the HARVEST regression tests (v0.48.32).

Binds 127.0.0.1:41234 and answers per request path so the .expected
files stay deterministic:

  /get      200, body "hello mock:<X-Test header or 'none'>",
            response header X-Mock: yes
  /post     200, body "POST:<request body>", header X-Mock: post
  /slow     sleeps 3s, then 200 "slow" (used for timeout tests)
  /empty    200, empty body
  /status404 404 "not found"
  /bad      malformed response: no HTTP status line
  /readback keep-alive MAP-mode flow (v0.48.34): 200 "hello mock",
            then pushes "server-push", reads the client's follow-up
            payload, and answers "push-ack:<payload>"
  *         404, empty body

Reads the request head plus Content-Length bytes of body, then
replies and closes the connection (clients use Connection: close).
"""
import socket
import time

PORT = 41234


class _Done(Exception):
    pass


srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
srv.bind(("127.0.0.1", PORT))
srv.listen(8)

while True:
    conn, _ = srv.accept()
    try:
        data = b""
        while b"\r\n\r\n" not in data:
            chunk = conn.recv(4096)
            if not chunk:
                break
            data += chunk
        head, _, rest = data.partition(b"\r\n\r\n")
        lines = head.split(b"\r\n")
        parts = lines[0].decode("latin-1").split(" ")
        path = parts[1] if len(parts) > 1 else "/"

        clen = 0
        for ln in lines[1:]:
            if ln.lower().startswith(b"content-length:"):
                try:
                    clen = int(ln.split(b":", 1)[1].strip())
                except ValueError:
                    clen = 0
        while len(rest) < clen:
            chunk = conn.recv(4096)
            if not chunk:
                break
            rest += chunk
        body = rest[:clen].decode("latin-1")

        xtest = "none"
        for ln in lines[1:]:
            if ln.lower().startswith(b"x-test:"):
                xtest = ln.split(b":", 1)[1].strip().decode("latin-1")
                break

        if path == "/readback":
            payload = b"hello mock"
            conn.sendall(
                b"HTTP/1.1 200 OK\r\n"
                b"X-Mock: readback\r\n"
                + ("Content-Length: %d\r\nConnection: keep-alive\r\n\r\n" % len(payload)).encode()
                + payload
            )
            time.sleep(0.3)
            conn.sendall(b"server-push")
            conn.settimeout(3)
            try:
                follow = conn.recv(4096)
            except socket.timeout:
                follow = b""
            conn.settimeout(None)
            conn.sendall(b"push-ack:" + follow)
            time.sleep(0.4)
            conn.close()
            raise _Done()
        elif path == "/get":
            payload = ("hello mock:" + xtest).encode()
            resp = (
                b"HTTP/1.1 200 OK\r\n"
                b"X-Mock: yes\r\n"
                + ("Content-Length: %d\r\n\r\n" % len(payload)).encode()
                + payload
            )
        elif path == "/post":
            payload = ("POST:" + body).encode()
            resp = (
                b"HTTP/1.1 200 OK\r\n"
                b"X-Mock: post\r\n"
                + ("Content-Length: %d\r\n\r\n" % len(payload)).encode()
                + payload
            )
        elif path == "/slow":
            time.sleep(3)
            resp = b"HTTP/1.1 200 OK\r\nContent-Length: 4\r\n\r\nslow"
        elif path == "/empty":
            resp = b"HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n"
        elif path == "/status404":
            resp = b"HTTP/1.1 404 Not Found\r\nContent-Length: 9\r\n\r\nnot found"
        elif path == "/bad":
            resp = b"THIS IS NOT HTTP\r\n\r\nwhatever"
        else:
            resp = b"HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n"
        conn.sendall(resp)
    except Exception:
        pass
    finally:
        conn.close()