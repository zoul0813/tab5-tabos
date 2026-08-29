#!/usr/bin/env python3

from __future__ import annotations

import argparse
import socket
import threading

TCP_PAYLOAD_SIZE = 1537
TCP_HANDSHAKE = b"TN01"


def tcp_server(bind: str, port: int, ready: threading.Event, stopped: threading.Event) -> None:
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as server:
        server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        server.bind((bind, port))
        server.listen()
        server.settimeout(0.5)
        print(f"TCP echo listening on {bind}:{port}", flush=True)
        ready.set()
        while not stopped.is_set():
            try:
                connection, peer = server.accept()
            except socket.timeout:
                continue
            with connection:
                print(f"TCP accepted {peer[0]}:{peer[1]}", flush=True)
                total = 0
                try:
                    connection.sendall(TCP_HANDSHAKE)
                    print("TCP handshake sent", flush=True)
                    while total < TCP_PAYLOAD_SIZE:
                        data = connection.recv(TCP_PAYLOAD_SIZE - total)
                        if not data:
                            raise ConnectionError(f"peer closed after {total} of {TCP_PAYLOAD_SIZE} bytes")
                        connection.sendall(data)
                        total += len(data)
                        print(f"TCP received {len(data)} bytes ({total}/{TCP_PAYLOAD_SIZE})", flush=True)
                except OSError as error:
                    print(f"[FAIL] TCP {peer[0]}:{peer[1]}: {error}", flush=True)
                    continue
                print(f"[PASS] TCP echoed {total} bytes for {peer[0]}:{peer[1]}", flush=True)


def udp_server(bind: str, port: int, ready: threading.Event, stopped: threading.Event) -> None:
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as server:
        server.bind((bind, port))
        server.settimeout(0.5)
        print(f"UDP echo listening on {bind}:{port}", flush=True)
        ready.set()
        while not stopped.is_set():
            try:
                data, peer = server.recvfrom(65535)
            except socket.timeout:
                continue
            server.sendto(data, peer)
            print(f"[PASS] UDP echoed {len(data)} bytes for {peer[0]}:{peer[1]}", flush=True)


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="TCP/UDP echo server for TabOS network testing")
    parser.add_argument("--bind", default="0.0.0.0", help="IPv4 address to bind (default: all interfaces)")
    parser.add_argument("--tcp-port", type=int, default=39001)
    parser.add_argument("--udp-port", type=int, default=39002)
    return parser.parse_args()


def main() -> int:
    arguments = parse_arguments()
    for port in (arguments.tcp_port, arguments.udp_port):
        if port < 1 or port > 65535:
            raise SystemExit("ports must be between 1 and 65535")

    stopped = threading.Event()
    tcp_ready = threading.Event()
    udp_ready = threading.Event()
    threads = [
        threading.Thread(
            target=tcp_server,
            args=(arguments.bind, arguments.tcp_port, tcp_ready, stopped),
            daemon=True,
        ),
        threading.Thread(
            target=udp_server,
            args=(arguments.bind, arguments.udp_port, udp_ready, stopped),
            daemon=True,
        ),
    ]
    for thread in threads:
        thread.start()
    tcp_ready.wait()
    udp_ready.wait()
    print("Ready; protocol TN01 fixed-length v1; press Ctrl+C to stop.", flush=True)
    try:
        while all(thread.is_alive() for thread in threads):
            stopped.wait(1.0)
    except KeyboardInterrupt:
        pass
    finally:
        stopped.set()
        for thread in threads:
            thread.join(timeout=1.0)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
