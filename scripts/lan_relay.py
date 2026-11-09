#!/usr/bin/env python3
# Part of Generals-Web. Derived from EA's C&C Generals Zero Hour source (GPL-3.0).
# SPDX-License-Identifier: GPL-3.0-only
"""
Virtual-LAN relay for browser multiplayer.

Every browser that connects to ws://HOST:PORT/<room> joins one virtual LAN: each frame it
sends is forwarded to everyone else in the same room. The relay never parses frames; the
page drops packets not addressed to its own virtual IP. Binary frames are game packets;
text frames are WebRTC signaling (and fallback packets) for pages opened with &p2p.

    pip install websockets
    python3 scripts/lan_relay.py [--host 0.0.0.0] [--port 8765]

Players then open GeneralsZH.html?relay=ws://HOST:8765&room=NAME      (all traffic via relay)
               or GeneralsZH.html?relay=ws://HOST:8765&room=NAME&p2p  (browser-to-browser WebRTC)
"""

import argparse
import asyncio

import websockets

rooms = {}  # room path -> set of connected sockets


async def handle(ws):
    room = getattr(ws, "path", None) or ws.request.path
    peers = rooms.setdefault(room, set())
    peers.add(ws)
    try:
        async for frame in ws:
            websockets.broadcast(peers - {ws}, frame)
    except websockets.ConnectionClosed:
        pass  # a tab closed without a goodbye; nothing to report
    finally:
        peers.discard(ws)
        if not peers:
            rooms.pop(room, None)


async def serve(host, port):
    async with websockets.serve(handle, host, port, max_size=64 * 1024):
        await asyncio.Future()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--host", default="0.0.0.0")
    parser.add_argument("--port", type=int, default=8765)
    args = parser.parse_args()
    print(f"LAN relay on ws://{args.host}:{args.port}/<room>")
    asyncio.run(serve(args.host, args.port))
