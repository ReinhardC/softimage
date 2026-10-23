# XsiMcp: the Softimage plug-in that runs xsi_mcp_server (an MCP endpoint for driving
# Softimage). Softimage tears down a plug-in's script namespace between callbacks, so the
# server lives in an ordinary module (in sys.modules) and this file only starts it,
# stops it, and hands it Softimage's objects on each timer tick to run queued calls.
import os
import sys

null = None
false = 0
true = 1

HOST_OBJECTS = ('Application', 'XSIUtils', 'XSIMath', 'XSIFactory', 'XSIUIToolkit')


def XSILoadPlugin(in_reg):
    in_reg.Author = 'ReinhardC'
    in_reg.Name = 'XsiMcp'
    in_reg.Major = 1
    in_reg.Minor = 0
    in_reg.RegisterTimerEvent('XsiMcpTimer', 25, 0)
    # The server module sits in Application\xsi_mcp, beside (not in) the scanned Plugins.
    plugins = in_reg.OriginPath.rstrip('\\/')
    if plugins.lower().endswith('.py'):
        plugins = os.path.dirname(plugins)
    lib = os.path.join(os.path.dirname(plugins), 'xsi_mcp')
    try:
        if not os.path.isfile(os.path.join(lib, 'xsi_mcp_server.py')):
            raise RuntimeError('xsi_mcp_server.py is not in %s; run install.ps1' % lib)
        if lib not in sys.path:
            sys.path.insert(0, lib)
        import xsi_mcp_server
        if not xsi_mcp_server.running():
            reload(xsi_mcp_server)  # pick up edits when the plug-in is reloaded
        url, connection = xsi_mcp_server.start()
        _log(lib, 'serving MCP at %s (token in %s)' % (url, connection))
    except Exception:
        import traceback
        _log(lib, 'could not start:\n' + traceback.format_exc(), True)
    return true


def _log(folder, message, error=False):
    # To Softimage's log and to plugin.log beside the server, which outlives a crash.
    try:
        Application.LogMessage('XsiMcp: ' + message, 4 if error else 32)  # siError, siInfo
    except Exception:
        pass
    try:
        import time
        with open(os.path.join(folder, 'plugin.log'), 'a') as f:
            f.write('%s pid %d: %s\n' % (time.strftime('%Y-%m-%d %H:%M:%S'), os.getpid(), message))
    except Exception:
        pass


def XSIUnloadPlugin(in_reg):
    server = sys.modules.get('xsi_mcp_server')
    if server:
        server.stop()
    return true


def XsiMcpTimer_OnEvent(in_ctxt):
    server = sys.modules.get('xsi_mcp_server')
    if server and server.pending():
        g = globals()
        server.drain(dict((name, g[name]) for name in HOST_OBJECTS if name in g))
    return false
