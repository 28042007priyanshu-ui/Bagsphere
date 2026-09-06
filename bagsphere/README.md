# BagSphere — College NFC Lost-Bag Recovery

BagSphere is a local, dependency-free NFC lost-bag recovery demo.

## Stack
- **C**: FIFO queue for incoming finder-location reports (`reports_queue.c`)
- **C++**: HTTP server, persistence, routing, bag registration and dashboard logic (`server.cpp`)
- **HTML/CSS/JavaScript**: browser UI and phone location permission
- No Python, Node.js, framework, or database service is required.

## Run

### Windows
Install a C/C++ compiler such as MinGW-w64 and make sure `g++` and `gcc` are on PATH.

Double-click `start.bat`, or run:

```bat
start.bat
```

Then open:

http://localhost:8000

### Linux/macOS
Install GCC/G++ and run:

```bash
chmod +x start.sh
./start.sh
```

Then open:

http://localhost:8000

The server creates its local data files under `data/`.

## NFC deployment note

Browser geolocation is normally available only in secure contexts. `localhost` is treated as a secure context by modern browsers for development, but a finder using a different phone/network cannot reach your computer's localhost.

For real NFC use:
1. Deploy the same C++ server to a public host.
2. Put it behind a public **HTTPS** domain.
3. Register a bag there.
4. Write that public bag URL to the NFC tag.

Example public tag URL:

`https://bagsphere.example/bag/BG-A1B2C3D4`

The local `localhost:8000` setup is intended for development/demo use.

## Privacy
The public finder page exposes only the information needed to identify and contact the owner. Finder coordinates are stored as latitude/longitude and timestamps; no home address is collected by BagSphere.

## My Bag (user page)
Visit `http://localhost:8000/my` and enter the Bag ID and owner token you were given when you registered (the same pair used in the dashboard link). It takes you straight to your private dashboard, so you don't need to keep the original link around.

## Admin panel
Visit `http://localhost:8000/admin` to sign in with the admin password and manage every registered bag from one screen: search, view full profile + finder-reported locations, open a bag's owner dashboard, and delete a bag (which also removes its finder reports).

Default admin password: `bagsphere123`

To change it, set the `BAGSPHERE_ADMIN_PASSWORD` environment variable before starting the server, e.g.:

```bash
BAGSPHERE_ADMIN_PASSWORD=my-secret-password ./start.sh
```

On Windows (cmd):
```bat
set BAGSPHERE_ADMIN_PASSWORD=my-secret-password
start.bat
```

Admin sessions are kept in memory and reset whenever the server restarts.


## Windows compiler troubleshooting

If Windows shows errors such as:

- `libgmp-10.dll was not found`
- `libisl-23.dll was not found`
- `libmpc-3.dll was not found`

your MSYS2 compiler installation is incomplete or its package database is partially updated. Open **MSYS2 UCRT64** (not ordinary CMD) and run:

```bash
pacman -Syu
```

If it tells you to close the terminal, close it, reopen **MSYS2 UCRT64**, and run:

```bash
pacman -Syu
pacman -S --needed mingw-w64-ucrt-x86_64-gcc
```

Then close MSYS2 and double-click `start.bat` in BagSphere. The launcher now starts the compiler through the MSYS2 environment and also statically links the C/C++ runtime into the BagSphere executable where possible.
