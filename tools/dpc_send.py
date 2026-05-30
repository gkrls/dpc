#!/usr/bin/env python3
"""Fire mock DPC packets at the soft switch -- stands in for the workers.

Layout must match dpc/proto.h (struct Header, 30 bytes, big-endian on the wire):
  sessid u32, operid u32, seqnum u32, bitmap u32, offset u32,
  slotid u16, n u8, flags u8, counts u16, quants u32
If your real proto.h differs, adjust HDR and make_packet only.

Examples:
  python3 mock_send.py --port 4242                       # one packet
  python3 mock_send.py --senders 6 --values 256          # six "workers", full payload
  python3 mock_send.py --slot 5 --sess 1 --senders 3     # three workers into slot 5
"""
import argparse
import socket
import struct
import time

HDR = "!IIIII H B B H I"  # sessid operid seqnum bitmap offset | slotid n flags counts quants


def make_packet(sess, seq, slot, n, flags, bitmap, values):
    hdr = struct.pack(HDR, sess, 0, seq, bitmap, 0, slot, n, flags, 0, 0)
    payload = struct.pack("!%di" % len(values), *values)  # int32 lanes
    return hdr + payload


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--port", type=int, default=4242)
    ap.add_argument("--sess", type=int, default=1)
    ap.add_argument("--slot", type=int, default=0)
    ap.add_argument("--n", type=int, default=6, help="world size hint in the header")
    ap.add_argument("--values", type=int, default=8, help="int32 lanes in the payload")
    ap.add_argument("--senders", type=int, default=1, help="simulate N distinct workers")
    args = ap.parse_args()

    for w in range(args.senders):
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)  # new socket -> distinct src port
        bitmap = 1 << w                  # each worker sets its own participation bit
        vals = [(w + 1)] * args.values   # worker w contributes all (w+1)s -> easy to eyeball sums
        pkt = make_packet(args.sess, seq=1, slot=args.slot, n=args.n,
                          flags=0x01, bitmap=bitmap, values=vals)
        s.sendto(pkt, (args.host, args.port))
        print("sent worker=%d bitmap=0x%x bytes=%d" % (w, bitmap, len(pkt)))
        s.close()
        time.sleep(0.05)


if __name__ == "__main__":
    main()
