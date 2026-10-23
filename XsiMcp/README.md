# XsiMcp

An MCP endpoint inside Softimage, so an MCP client such as Claude Code can run Python in
Softimage and see the result. Written and tested with Softimage 2015 R2-SP2 (Python 2.7).

- `xsi_execute`: runs Python 2.7 on Softimage's main thread with `Application`,
  `XSIUtils`, `XSIMath`, `XSIFactory`, `XSIUIToolkit` and `c` (win32com constants). The
  namespace persists between calls. Returns what was printed and the value of a final
  expression; Softimage objects come back by `FullName`. Edits are undoable as usual.
- `xsi_capture`: `window` (the main window, scaled to `max_width`, default 1600) or a
  viewport `A`-`D` through `CaptureViewport`. Window captures copy the screen
  (`source: screen`, the default: the window must be visible, floating windows over it
  are included) or ask the main window to draw itself (`source: print`: works when
  covered, viewports included, floating windows left out).

## Install

Any of these:

- Drag and drop `XsiMcp.xsiaddon` from the project's releases into a Softimage window,
  then restart Softimage.
- Connect Softimage to this repository's `Workgroup` (File > Plug-in Manager >
  Workgroups).
- Copy the add-on from this repository into your Softimage user folder:

  ```powershell
  XsiMcp/install.ps1                               # the newest Softimage_* user folder
  XsiMcp/install.ps1 -Version Softimage_2015_R2-SP2
  ```

Then start Softimage; its log shows `XsiMcp: serving MCP at http://127.0.0.1:47824/mcp
(token in ...)`. In a running Softimage, File > Plug-in Manager > Update All loads it.
To pick up edited server code in a Softimage already serving, reload the plug-in from
the Script Editor. Never unload or reload XsiMcp through `xsi_execute`: the call runs
inside XsiMcp's own timer event, and unloading the plug-in frees that event while it is
running, which crashes Softimage (in `xsievents.dll`).

```python
p = r'<add-on folder>\Application\Plugins\XsiMcp.py'
Application.UnloadPlugin(p); Application.LoadPlugin(p)
```

The add-on is two files: the plug-in `Application/Plugins/XsiMcp.py` and the server
module `Application/xsi_mcp/xsi_mcp_server.py`. The server is its own module because
Softimage tears down a plug-in's script namespace between callbacks: a server thread
using the plug-in's globals would find them gone. Calls run on the main thread from a
timer event, because the object model is not thread-safe.

To build the release package, run `XsiMcp/build_addon.py` in Softimage's Script Editor
(Python): `execfile(r'<repository>\XsiMcp\build_addon.py')`. It writes
`release/XsiMcp.xsiaddon` (ignored by git) from the add-on files in `Workgroup`.

## Connect

The endpoint is `http://127.0.0.1:47824/mcp` (`XSI_MCP_PORT` changes the port). It
listens on loopback only, checks `Host` and `Origin`, and needs `Authorization: Bearer
<token>`. The token is generated once and kept in `connection.json` beside the server
module (or set `XSI_MCP_TOKEN`), so a client registered once keeps working after
restarts. To register it with Claude Code (Python 3; the token is not shown):

```bash
python XsiMcp/probe.py register
```

`connection.json` is deliberately not in AppData: Windows redirects AppData writes of
processes started by packaged apps (the Claude desktop app, and a Softimage it
launches) into a private copy, so the two sides would read different tokens.

Only one Softimage can serve the port; a second one logs that it could not start. Load
errors also go to `plugin.log` beside the server module.

## Try it without a client

```bash
python XsiMcp/probe.py list
python XsiMcp/probe.py exec "Application.CreatePrim('Octahedron', 'MeshSurface')"
python XsiMcp/probe.py capture window shot.png
python XsiMcp/probe.py capture B viewB.png
```

## Another PC

Install it there and register that PC's Claude Code as above; each Softimage has its own
token. To drive a Softimage on another PC from this one, tunnel the port over SSH
(`ssh -N -L 47824:127.0.0.1:47824 user@other-pc`, with no Softimage serving here) and
register with that PC's token. Do not make the endpoint listen on the network:
`xsi_execute` runs any Python it is sent.

## Notes

- Do not unload, reload or update XsiMcp (UnloadPlugin, Plug-in Manager updates)
  through `xsi_execute` itself: it crashes Softimage. Do that from the Script Editor or
  the Plug-in Manager.
- A call waits up to `timeout` seconds (default 120). A modal dialog in Softimage blocks
  the main thread, so calls wait until it closes.
- `xsi_execute` runs arbitrary code in Softimage, which is the point; the token and the
  loopback, `Host` and `Origin` checks keep it local to this user.
