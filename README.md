# SamAteMyRAM

**Altman.exe** is a tiny Windows desktop toy. A hungry cartoon mascot eats real RAM from this process, gets fatter as it goes, and gives every byte back when you ask.

It is an unofficial parody and a voluntary memory stress test. It is not affiliated with Sam Altman or OpenAI.

## What it does

Press **OFFER SACRIFICE**. The app commits memory in chunks on a background thread and touches each page so Windows actually backs it. Task Manager shows this process's memory climbing. The mascot chews floating RAM sticks and its belly grows with the real allocation, not a timer.

It stops on its own before it tries to freeze the machine:

- **4 GB safe** (default): leave at least 4 GB of available physical RAM.
- **1 GB experimental**: opt-in, with a warning. On larger machines the reserve rises with RAM (about 6% of physical memory, at least 1 GB, at most 8 GB). The 1 GB figure is a floor, not a promise.
- It also stops if Windows reports low memory, commit charge gets tight, an allocation fails, or a chunk takes too long because the system is stalling.

**STOP THE SACRIFICE** pauses allocation and keeps what was already taken. **RECLAIM YOUR RAM**, **EMERGENCY RELEASE**, Esc (while the window is focused), or **Ctrl+Shift+F12** (even if the window is busy) frees it. Closing the window frees it too. Nothing is registered to start with Windows, and a reboot does not resume a sacrifice.

Speed is Slow (~128 MB/s), Normal (~512 MB/s), or Fast (~1 GB/s). Sound can be muted. The About dialog repeats the disclaimer.

## Safety

This program only allocates memory it owns. It does not touch other processes, the page file configuration, startup entries, or the network. It does not need administrator rights.

Do not leave a sacrifice running unattended. Experimental mode can make the PC sluggish. If the window ever feels stuck, press **Ctrl+Shift+F12**.

## Build

Windows 10 or 11, 64-bit. The finished program is one file, `Altman.exe`. System libraries it uses (Direct2D, DirectWrite, WinMM) ship with Windows.

### Script, with the local compiler kit

If `tools\w64devkit` is present:

```powershell
.\build.ps1
```

The script configures a Release build and copies `Altman.exe` to this folder.

### CMake yourself

With MinGW-w64 (g++ and windres on `PATH`):

```powershell
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

With Visual Studio 2022:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

The executable is `build\Altman.exe` (MinGW) or `build\Release\Altman.exe` (Visual Studio).

### Check the allocator without eating the machine

```powershell
.\Altman.exe --self-test
```

That commits roughly 64 MB, checks that stopping does not free it, reclaims it, then checks the emergency path. It writes `self-test-result.txt` and does not open the window. If the PC is already inside the 4 GB reserve, the test skips instead of pushing further.

## Remove it

Exit the app (reclaim first if a sacrifice is running). Delete this folder. There is no installer, service, or startup entry to undo.

## License

Source in this repository is released under the MIT License. See `LICENSE`.
