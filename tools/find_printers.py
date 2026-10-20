#!/usr/bin/env python3
"""Lists the Bambu Lab printers on your local network: name, model, serial number and IP.

Bambu printers announce themselves with SSDP multicast every few seconds; this just listens.
It needs no account and no access code. (The access code is a separate step, see
tools/fetch_access_codes.py.)

Usage:  python tools/find_printers.py [--seconds 40]
Needs only the Python standard library.
"""
import argparse
import socket
import struct
import time

MODELS = {  # DevModel code -> marketing name (only the ones that are confirmed)
    "N1": "A1 mini",
    "N2S": "A1",
    "C11": "P1P",
    "C12": "P1S",
    "BL-P001": "X1 Carbon",
    "BL-P002": "X1",
}


def local_ip():
    """The address of the interface that reaches the LAN (no packet is actually sent)."""
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        s.connect(("10.255.255.255", 1))
        return s.getsockname()[0]
    finally:
        s.close()


def listen_sockets(iface):
    socks = []
    for port in (1990, 2021):
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        try:
            s.bind(("", port))
            mreq = struct.pack("4s4s", socket.inet_aton("239.255.255.250"), socket.inet_aton(iface))
            s.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, mreq)
        except OSError as e:
            print("Could not listen on UDP %d: %s" % (port, e))
            continue
        s.settimeout(0.5)
        socks.append(s)
    return socks


def parse(text):
    fields = {}
    for line in text.splitlines():
        if ":" in line:
            key, _, value = line.partition(":")
            fields[key.strip()] = value.strip()
    return fields


def main():
    ap = argparse.ArgumentParser(description="Find Bambu Lab printers on the local network.")
    ap.add_argument("--seconds", type=int, default=40, help="how long to listen (default 40)")
    args = ap.parse_args()

    iface = local_ip()
    print("Listening %d s on %s for printer announcements..." % (args.seconds, iface))
    socks = listen_sockets(iface)
    found = {}
    end = time.time() + args.seconds
    while time.time() < end and socks:
        for s in socks:
            try:
                data, addr = s.recvfrom(4096)
            except socket.timeout:
                continue
            f = parse(data.decode("utf-8", "replace"))
            if "DevModel.bambu.com" in f:
                found[f.get("USN", addr[0])] = (addr[0], f)

    if not found:
        print("No printers heard. Are they on, and on the same network as this computer?")
        return
    print("\n%-18s %-10s %-17s %-15s %-8s %s" % ("Name", "Model", "Serial", "IP", "Mode", "Firmware"))
    for serial, (ip, f) in sorted(found.items(), key=lambda kv: kv[1][1].get("DevName.bambu.com", "")):
        code = f.get("DevModel.bambu.com", "?")
        print("%-18s %-10s %-17s %-15s %-8s %s" % (
            f.get("DevName.bambu.com", "?")[:18], MODELS.get(code, code), serial, ip,
            f.get("DevConnect.bambu.com", "?"), f.get("DevVersion.bambu.com", "?")))


if __name__ == "__main__":
    main()
