# Generals-Web. Setup Guide

*How to run C&C Generals Zero Hour in your browser.*

This guide walks you through getting the game running in your browser from scratch.

> You must own a legal copy of the game, it's included in [*C&C The Ultimate Collection* on Steam](https://store.steampowered.com/bundle/39394) and the EA App. No game assets are included with this project; your files stay on your machine (see [NOTICE](NOTICE)).

---

## What You Need

1. **The web build**: download from the [latest GitHub Release](https://github.com/TheodoryLabs/Generals-Web/releases/latest)
   *(note: release `v0.4.0-web` predates the working renderer, prefer the newest release, or build from source via [BUILDING.md](BUILDING.md))*
2. **The game asset files**: you must own a legal copy of C&C Generals Zero Hour
3. **Python 3**: to run the local server (comes pre-installed on macOS/Linux)

---

## Step 1. Get the Game Assets

The `.big` files contain all of the game's textures, models, maps, sounds, and data. They are **not included** in this repo, you need to get them from your own game installation.

### If you own the game on Steam
1. In Steam, right-click **C&C: The Ultimate Collection** → **Manage** → **Browse local files**
2. Navigate to the `Command & Conquer Generals Zero Hour` folder
3. The `.big` files are in the root of that folder

### If you have a disc/retail install
The `.big` files are in your Zero Hour installation directory (default: `C:\Program Files\EA Games\Command and Conquer Generals Zero Hour\`)

### Files you need to copy

Copy these files, these are the **required** ones for startup:

| File | Size | Contents |
|------|------|----------|
| `INIZH.big` | ~18 MB | Game configuration (INI scripts) |
| `INI.big` | ~7 MB | Base game configuration |
| `TexturesZH.big` | ~212 MB | Zero Hour textures |
| `Textures.big` | ~318 MB | Base game textures |
| `W3DZH.big` | ~181 MB | Zero Hour 3D models |
| `W3D.big` | ~176 MB | Base game 3D models |
| `WindowZH.big` | ~8 MB | UI layout files |
| `Window.big` | ~7.6 MB | Base UI layout files |
| `EnglishZH.big` | ~77 MB | English text + localization |
| `English.big` | ~2.6 MB | Base English text |
| `MapsZH.big` | ~38 MB | Zero Hour maps |
| `maps.big` | ~22 MB | Base maps |
| `TerrainZH.big` | ~8.3 MB | Terrain textures |
| `ShadersZH.big` | ~1 KB | Shader definitions |
| `shaders.big` | ~1.2 KB | Base shader definitions |
| `PatchINI.big` | ~52 KB | Patch overrides |
| `gensecZH.big` | ~769 KB | Security/checksum data |

> The audio archives (`AudioZH.big`, `MusicZH.big`, `SpeechZH.big`, `SpeechEnglishZH.big`) are mounted lazily after the menu loads, copy them too if you want the audio pipeline exercised as it comes online.

---

## Step 2. Set Up Your Folder

Create a folder anywhere on your computer. Download the four files from the [GitHub Release](https://github.com/TheodoryLabs/Generals-Web/releases/latest) and create an `assets/` subfolder with your `.big` files. Your folder should look exactly like this:

```
generals-web/
├── GeneralsZH.html       ← download from Release
├── GeneralsZH.js         ← download from Release
├── GeneralsZH.wasm       ← download from Release (size varies by release)
├── serve.py              ← download from Release
└── assets/
    └── (the 17 .big files from the table above)
```

> **Important:** The `.big` files must be inside the `assets/` subfolder, not the same folder as the HTML.

---

## Step 3. Start the Server

Open a terminal, navigate to your folder, and run:

```bash
cd /path/to/generals-web
python3 serve.py
```

You should see:
```
Starting THREADED RANGE server on http://localhost:8888
Press Ctrl+C to stop.
```

> **Why can't I just double-click the HTML file?**
> The game needs HTTP Range requests to stream assets out of `.big` files on demand. Browsers block this when loading from a local file path (`file://`). The server handles this correctly.

---

## Step 4. Open in Browser

Open **Chrome** or **Firefox** and go to:

```
http://localhost:8888/GeneralsZH.html
```

Open **DevTools** first (`F12` → Console tab), the console shows loading progress and any errors.

The game will:
1. Load and parse all `.big` archive headers (~1-3 seconds)
2. Mount ~16,500 files into the in-memory filesystem
3. Start the game engine and render the first frame

---

## Multiplayer (virtual LAN, experimental)

The browser has no raw UDP, so the game's LAN traffic travels over a virtual LAN. Once players
are connected they see each other under **Network → LAN**.

| Link ends with | How players connect | Server |
|---|---|---|
| `?p2p` | Browser-to-browser over WebRTC; players swap invite codes | none |
| `?p2p&relay=ws://<host>:8765&room=friday` | WebRTC, found automatically; the relay is the last-resort route | relay |
| `?relay=ws://<host>:8765&room=friday` | All traffic through the relay | relay |
| `?virtual_net` | Tabs on one machine (`BroadcastChannel`) | none |

Everyone in a game must use the same row.

### Playing with no server (`?p2p`)

A **P2P** panel appears in the top-right corner.

1. Any player in the game presses **Invite** and sends the code (copied for you) to the
   player joining, by any chat app.
2. The joining player presses **Join**, pastes the code, and sends back the reply code.
3. The inviting player presses **Accept reply** and pastes it.

The new player only has to pair with **one** member: that member introduces them to everyone
else over WebRTC, and the whole group links up within a few seconds. The panel shows
`P2P: direct / players`.

- **If a link between two players drops**, their traffic is forwarded through a player who
  can still reach both, and the direct link is retried automatically.
- **If a player leaves**, the rest keep playing; the leaver is forgotten after a minute.
- **If the only link of a two-player game drops for good**, swap codes again.

WebRTC asks Google's public STUN server for each player's public address; that is not a
server you run. Players on the same local network can pass `&stun=none`; `&stun=stun:host:port`
picks another. A few strict networks (some mobile carriers, corporate firewalls) block direct
WebRTC entirely: those players can still play if at least one other player reaches them, or
add a relay.

### Running a relay (optional)

On any host the players can reach (a small cloud VM is fine):

```bash
pip install "websockets>=10.1"
python3 scripts/lan_relay.py --port 8765
```

With `?p2p&relay=...` players in the same `room` find each other with no codes, and a player
nobody can reach directly still plays through the relay. A page served over `https` needs a
`wss://` relay (put it behind a TLS proxy).

Each player gets a random `10.0.0.x` address; pass `&ip=10.0.0.N` to pick one and avoid clashes.

---

## Troubleshooting

### Black screen / nothing loads
- Make sure `serve.py` is running and you're accessing `http://localhost:8888` (not `file://`)
- Check the browser console for error messages

### "File not found in .big archives: ..."
- A required `.big` file is missing from your `assets/` folder
- Check the console for which file it's looking for and copy it from your game installation

### "SharedArrayBuffer is not defined"
- The server's `Cross-Origin-Opener-Policy` headers aren't reaching the browser
- Make sure you're using the included `serve.py`: other basic servers won't set these headers
- Try Chrome if Firefox is having issues

### Out of memory / tab crash
- The game needs ~512 MB of browser memory
- Close other tabs and try again
- Chrome handles large WASM better than other browsers

### Server port already in use
- Edit `serve.py` and change `PORT = 8888` to another number (e.g. `8080`)
- Then go to `http://localhost:8080/GeneralsZH.html`

---

## System Requirements

| | Minimum |
|-|---------|
| Browser | Chrome 90+, Firefox 88+, Edge 90+ |
| RAM | 4 GB system RAM (browser uses ~512 MB) |
| OS | Windows, macOS, or Linux |
| Python | 3.6+ (for the server) |

Safari is **not supported**: it has incomplete WebAssembly Asyncify support.
