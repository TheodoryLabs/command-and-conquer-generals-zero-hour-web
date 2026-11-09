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

The browser has no raw UDP, so the game's LAN traffic travels over a virtual LAN.

- **Two tabs on one machine**: open both with `?virtual_net`. They talk over a `BroadcastChannel`.
- **Different machines**: run the relay on a host every player can reach (a small cloud VM is fine):

  ```bash
  pip install websockets
  python3 scripts/lan_relay.py --port 8765
  ```

  Every player then opens one of:

  | Link ends with | Game traffic goes |
  |---|---|
  | `?relay=ws://<relay-host>:8765&room=friday` | through the relay |
  | `?relay=ws://<relay-host>:8765&room=friday&p2p` | browser-to-browser over WebRTC; the relay only introduces players, and carries traffic for any pair that can't connect directly |

  Everyone in a room must use the same mode. Everyone in the same `room` is on the same LAN
  and sees each other under **Network → LAN**. WebRTC uses Google's public STUN server;
  pass `&stun=stun:host:port` to use another.
  Each player gets a random `10.0.0.x` address; pass `&ip=10.0.0.N` to pick one and avoid clashes.
  A page served over `https` needs a `wss://` relay (put it behind a TLS proxy).

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
