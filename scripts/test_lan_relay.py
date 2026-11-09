#!/usr/bin/env python3
# Part of Generals-Web. Derived from EA's C&C Generals Zero Hour source (GPL-3.0).
# SPDX-License-Identifier: GPL-3.0-only
"""Self-check for lan_relay.py: frames reach the rest of the room, never the sender or other rooms."""

import asyncio
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import websockets  # noqa: E402

import lan_relay  # noqa: E402

PORT = 18765


async def main():
    server = asyncio.ensure_future(lan_relay.serve("127.0.0.1", PORT))
    await asyncio.sleep(0.3)
    url = f"ws://127.0.0.1:{PORT}"
    a = await websockets.connect(url + "/one")
    b = await websockets.connect(url + "/one")
    c = await websockets.connect(url + "/two")
    await asyncio.sleep(0.1)

    await a.send(b"\x01hello")
    assert await asyncio.wait_for(b.recv(), 1) == b"\x01hello"
    await a.send('{"type": "hello"}')  # WebRTC signaling is text
    assert await asyncio.wait_for(b.recv(), 1) == '{"type": "hello"}'
    for ws in (a, c):  # sender and the other room hear nothing
        try:
            await asyncio.wait_for(ws.recv(), 0.3)
            raise AssertionError("frame leaked")
        except asyncio.TimeoutError:
            pass

    await b.close()
    await asyncio.sleep(0.1)
    await c.send(b"\x02solo")  # alone in its room: no error
    for ws in (a, c):
        await ws.close()
    await asyncio.sleep(0.1)
    assert lan_relay.rooms == {}, lan_relay.rooms
    server.cancel()
    print("lan_relay ok")


asyncio.run(main())
