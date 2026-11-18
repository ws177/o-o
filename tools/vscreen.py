# -*- coding: utf-8 -*-
"""
vscreen.py — 把板子的表情帧变成"虚拟屏幕"

数据源二选一：
    --tcp 192.168.137.239:3333     走 WiFi（推荐，不占串口）
    --port COM12                   走 USB 串口

然后浏览器打开 http://127.0.0.1:8777/
    /            远程模式（显示板子真实状态，点击把命令发回板子）
    /o-o         形状语言版渲染器（本地驱动）
"""
import argparse
import os
import socket
import sys
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import urlparse, parse_qs

try:
    import serial
except ImportError:
    serial = None

HERE = os.path.dirname(os.path.abspath(__file__))
FACE_HTML = os.path.join(HERE, "..", "preview", "face.html")
OO_HTML = os.path.join(HERE, "..", "preview", "o-o.html")

latest = {"line": b"", "ts": 0.0, "count": 0, "err": None, "src": ""}
lock = threading.Lock()
conn = None          # 当前数据连接（serial 或 socket）


def set_err(msg):
    with lock:
        latest["err"] = msg


def open_source(args):
    """建连接，失败就一直重试。"""
    while True:
        try:
            if args.tcp:
                host, port = args.tcp.rsplit(":", 1)
                s = socket.create_connection((host, int(port)), timeout=5)
                s.settimeout(0.2)
                with lock:
                    latest["src"] = "tcp %s" % args.tcp
                set_err(None)
                return s
            else:
                s = serial.Serial(args.port, args.baud, timeout=0.05)
                with lock:
                    latest["src"] = "serial %s" % args.port
                set_err(None)
                return s
        except Exception as e:
            set_err("连接失败: %s" % e)
            time.sleep(1.0)


def read_some(s, n):
    """统一 serial / socket 的读取。返回 b'' 表示暂时没数据。"""
    if hasattr(s, "read"):
        return s.read(n)
    try:
        return s.recv(n)
    except socket.timeout:
        return b""


def reader(args):
    global conn
    buf = b""
    while True:
        conn = open_source(args)
        buf = b""
        while True:
            try:
                data = read_some(conn, 512)
            except Exception as e:
                set_err("读取中断: %s" % e)
                break
            if not data:
                if hasattr(conn, "recv"):
                    # TCP 对端断开时 recv 返回 b''，重连
                    pass
                continue
            buf += data
            while b"\n" in buf:
                line, buf = buf.split(b"\n", 1)
                line = line.strip()
                if line.startswith(b"F,"):
                    with lock:
                        latest["line"] = line
                        latest["ts"] = time.time()
                        latest["count"] += 1
        try:
            conn.close()
        except Exception:
            pass
        time.sleep(0.5)


class Handler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, fmt, *args):
        pass

    def handle_one_request(self):
        """浏览器直接断开时不要炸掉整个服务"""
        try:
            super().handle_one_request()
        except (ConnectionResetError, BrokenPipeError, OSError):
            self.close_connection = True

    def do_GET(self):
        u = urlparse(self.path)
        if u.path in ("/", "/index.html"):
            if "remote=1" not in (u.query or ""):
                self.send_response(302)
                self.send_header("Location", "/?remote=1")
                self.send_header("Content-Length", "0")
                self.end_headers()
                return
            return self.serve_file(OO_HTML)
        if u.path == "/o-o":
            return self.serve_file(OO_HTML)
        if u.path == "/old":
            return self.serve_file(FACE_HTML)
        if u.path == "/frames":
            return self.serve_sse()
        if u.path == "/cmd":
            q = parse_qs(u.query)
            c = (q.get("c", [""])[0] or "")[:1]
            ok = False
            if c and conn is not None:
                try:
                    conn.write(c.encode()) if hasattr(conn, "write") else conn.send(c.encode())
                    ok = True
                except Exception:
                    ok = False
            body = b"ok" if ok else b"fail"
            self.send_response(200)
            self.send_header("Content-Type", "text/plain")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
            return
        if u.path == "/status":
            with lock:
                body = ("count=%d age=%.2fs src=%s err=%s" % (
                    latest["count"],
                    (time.time() - latest["ts"]) if latest["ts"] else -1,
                    latest["src"], latest["err"] or "none")).encode()
            self.send_response(200)
            self.send_header("Content-Type", "text/plain")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
            return
        self.send_error(404)

    def serve_file(self, path):
        try:
            with open(path, "rb") as f:
                body = f.read()
        except Exception as e:
            body = ("<h1>读不到文件</h1><pre>%s</pre>" % e).encode()
        self.send_response(200)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(body)

    def serve_sse(self):
        self.send_response(200)
        self.send_header("Content-Type", "text/event-stream; charset=utf-8")
        self.send_header("Cache-Control", "no-store")
        self.send_header("Connection", "keep-alive")
        self.end_headers()
        last = -1
        try:
            while True:
                with lock:
                    ln, cnt = latest["line"], latest["count"]
                if ln and cnt != last:
                    last = cnt
                    self.wfile.write(("data: %s\n\n" % ln[2:].decode("ascii", "ignore")).encode())
                    self.wfile.flush()
                else:
                    time.sleep(0.008)
        except (BrokenPipeError, ConnectionResetError, OSError):
            return


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--tcp", default="", help="host:port，走 WiFi")
    ap.add_argument("--port", default="COM12")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--http", type=int, default=8777)
    args = ap.parse_args()

    if not args.tcp and serial is None:
        print("没有 pyserial，只能用 --tcp")
        sys.exit(1)

    threading.Thread(target=reader, args=(args,), daemon=True).start()
    srv = ThreadingHTTPServer(("127.0.0.1", args.http), Handler)
    print("虚拟屏幕: http://127.0.0.1:%d/" % args.http)
    print("数据源: %s" % (args.tcp or args.port))
    sys.stdout.flush()
    srv.serve_forever()


if __name__ == "__main__":
    main()
