# xsi_mcp_server: an MCP server inside Softimage 2015 (Python 2.7), for driving it from an
# MCP client such as Claude Code. Streamable HTTP, POST only, JSON replies, on loopback
# with a bearer token. Requests are answered on a server thread; the work itself runs on
# Softimage's main thread when the XsiMcp plug-in's timer calls drain(), because the
# object model is not thread-safe. The plug-in passes Softimage's objects (Application,
# XSIUtils, ...) to every drain: its own namespace does not outlive a callback.
import win32com.client
from win32com.client import constants as c

import ast
import base64
import BaseHTTPServer
import ctypes
import json
import os
import Queue
import shutil
import SocketServer
import struct
import sys
import tempfile
import threading
import time
import traceback
import zlib
from ctypes import wintypes

PORT = int(os.environ.get('XSI_MCP_PORT', '47824'))
PROTOCOLS = ('2025-06-18', '2025-03-26', '2024-11-05')

_state = {'server': None, 'thread': None, 'token': None}
_jobs = Queue.Queue()
_host = {}       # Softimage's objects, as of the current drain
_namespace = {}  # xsi_execute's globals, kept between calls


def running():
    return _state['server'] is not None


def pending():
    return not _jobs.empty()


def drain(host):
    """Runs the queued calls; on Softimage's main thread only."""
    _host.clear()
    _host.update(host)
    _drain()


# --- Server -----------------------------------------------------------------------------

def _url():
    return 'http://127.0.0.1:%d/mcp' % PORT


def _connection_file():
    # Beside this module, not in AppData: Windows redirects AppData writes of processes
    # started by packaged apps (such as the Claude desktop app) into a private copy, so
    # a Softimage started from there and one started normally would not see one file.
    return os.path.join(os.path.dirname(os.path.abspath(__file__)), 'connection.json')


def _load_token():
    # The token persists so a client registered once keeps working across restarts.
    token = os.environ.get('XSI_MCP_TOKEN')
    path = _connection_file()
    if not token:
        try:
            with open(path) as f:
                token = json.load(f).get('token')
        except Exception:
            token = None
    if not token:
        token = os.urandom(32).encode('hex')
    try:
        with open(path, 'w') as f:
            json.dump({'url': _url(), 'token': token, 'pid': os.getpid()}, f, indent=2)
    except Exception:
        pass
    return str(token)


def _same(a, b):
    # Constant time; Python 2.7.3 has no hmac.compare_digest.
    if len(a) != len(b):
        return False
    r = 0
    for x, y in zip(a, b):
        r |= ord(x) ^ ord(y)
    return r == 0


class _Server(SocketServer.ThreadingMixIn, BaseHTTPServer.HTTPServer):
    daemon_threads = True
    allow_reuse_address = False


class _Handler(BaseHTTPServer.BaseHTTPRequestHandler):
    protocol_version = 'HTTP/1.1'

    def log_message(self, *args):
        pass

    def _reply(self, code, body=None, headers=()):
        data = '' if body is None else json.dumps(body)
        self.send_response(code)
        if body is not None:
            self.send_header('Content-Type', 'application/json')
        for k, v in headers:
            self.send_header(k, v)
        self.send_header('Content-Length', str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def _refuse(self, code, message, headers=()):
        # Close after a refusal: a client may have sent a body this handler did not read.
        self.close_connection = 1
        self._reply(code, {'error': message}, list(headers) + [('Connection', 'close')])

    def do_GET(self):
        self._refuse(405, 'POST only', [('Allow', 'POST')])

    do_DELETE = do_GET
    do_PUT = do_GET

    def do_POST(self):
        try:
            body = self.rfile.read(int(self.headers.get('Content-Length') or 0))
        except ValueError:
            return self._refuse(400, 'bad Content-Length')
        if self.path.split('?')[0] != '/mcp':
            return self._refuse(404, 'not found')
        loopback = ('127.0.0.1:%d' % PORT, 'localhost:%d' % PORT)
        if self.headers.get('Host', '') not in loopback:
            return self._refuse(421, 'wrong host')
        origin = self.headers.get('Origin')
        if origin and origin not in ['http://' + h for h in loopback]:
            return self._refuse(403, 'wrong origin')
        token = _state['token']
        if not token or not _same(self.headers.get('Authorization', ''), 'Bearer ' + token):
            return self._refuse(401, 'missing or wrong token')
        try:
            msg = json.loads(body)
        except ValueError:
            return self._reply(400, _error(None, -32700, 'Parse error'))
        if not isinstance(msg, dict):
            return self._reply(400, _error(None, -32600, 'One JSON-RPC message per request'))
        if 'id' not in msg:
            return self._reply(202)  # a notification
        self._reply(200, _dispatch(msg))


def start():
    """Starts serving if not already; returns (url, connection file)."""
    if not running():
        _start()
    return _url(), _connection_file()


def stop():
    _stop()


def _start():
    _state['token'] = _load_token()
    server = _Server(('127.0.0.1', PORT), _Handler)
    thread = threading.Thread(target=server.serve_forever, name='XsiMcp')
    thread.daemon = True
    thread.start()
    _state['server'], _state['thread'] = server, thread


def _stop():
    server = _state['server']
    _state['server'] = None
    if server:
        server.shutdown()
        server.server_close()
    while True:
        try:
            job = _jobs.get_nowait()
        except Queue.Empty:
            break
        job['result'] = _text('XsiMcp was unloaded before this call ran.', True)
        job['event'].set()


# --- JSON-RPC ---------------------------------------------------------------------------

INSTRUCTIONS = (
    'Drives a running Softimage 2015. xsi_execute runs Python 2.7 on its main thread with '
    'the object model (Application, XSIUtils, XSIMath, XSIFactory, XSIUIToolkit, c for '
    'constants); commands are Application methods. xsi_capture returns an image of the '
    'main window or a viewport.')

TOOLS = [
    {
        'name': 'xsi_execute',
        'description': (
            "Run Python 2.7 on Softimage's main thread. The namespace persists between calls "
            'and holds Application, XSIUtils, XSIMath, XSIFactory, XSIUIToolkit, win32com and '
            "c (win32com constants, e.g. c.siVerbose). Commands are Application methods, e.g. "
            "Application.CreatePrim('Octahedron', 'MeshSurface'). Returns what the code printed "
            'and the value of a final expression (Softimage objects by FullName, collections as '
            "lists). Scene edits go to Softimage's undo history like any script."),
        'inputSchema': {
            'type': 'object',
            'properties': {
                'code': {'type': 'string', 'description': 'Python 2.7 source.'},
                'timeout': {'type': 'number', 'description': 'Seconds to wait for the result (default 120).'},
            },
            'required': ['code'],
        },
    },
    {
        'name': 'xsi_capture',
        'description': (
            "An image of Softimage: 'window' is the main window (menus, MCP, property editors, "
            "viewports); 'A' to 'D' render that viewport through CaptureViewport."),
        'inputSchema': {
            'type': 'object',
            'properties': {
                'target': {'type': 'string', 'enum': ['window', 'A', 'B', 'C', 'D'], 'description': "Default 'window'."},
                'max_width': {'type': 'integer', 'description': "Window captures: scale down to this width (default 1600)."},
                'source': {'type': 'string', 'enum': ['screen', 'print'], 'description': (
                    "Window captures: 'screen' copies the window's area of the screen (it must be "
                    "on top); 'print' asks the window to draw itself even when covered. Default 'screen'.")},
            },
        },
    },
]


def _result(mid, result):
    return {'jsonrpc': '2.0', 'id': mid, 'result': result}


def _error(mid, code, message):
    return {'jsonrpc': '2.0', 'id': mid, 'error': {'code': code, 'message': message}}


def _text(text, is_error=False):
    return {'content': [{'type': 'text', 'text': text}], 'isError': bool(is_error)}


def _dispatch(msg):
    mid, method, params = msg.get('id'), msg.get('method'), msg.get('params') or {}
    if method == 'initialize':
        version = params.get('protocolVersion')
        return _result(mid, {
            'protocolVersion': version if version in PROTOCOLS else PROTOCOLS[0],
            'capabilities': {'tools': {}},
            'serverInfo': {'name': 'xsi-mcp', 'version': '1.0'},
            'instructions': INSTRUCTIONS,
        })
    if method == 'ping':
        return _result(mid, {})
    if method == 'tools/list':
        return _result(mid, {'tools': TOOLS})
    if method == 'tools/call':
        func = _TOOL_FUNCS.get(params.get('name'))
        if func is None:
            return _error(mid, -32602, 'Unknown tool: %s' % params.get('name'))
        return _result(mid, _on_main(func, params.get('arguments') or {}))
    return _error(mid, -32601, 'Method not found: %s' % method)


def _on_main(func, args):
    try:
        timeout = min(max(float(args.get('timeout', 120)), 1.0), 3600.0)
    except (TypeError, ValueError):
        timeout = 120.0
    job = {'func': func, 'args': args, 'event': threading.Event(), 'result': None}
    _jobs.put(job)
    if not job['event'].wait(timeout):
        return _text('No result after %g s. The call is still queued or running on '
                     "Softimage's main thread (a modal dialog blocks it too)." % timeout, True)
    return job['result']


def _drain():
    while True:
        try:
            job = _jobs.get_nowait()
        except Queue.Empty:
            return
        try:
            job['result'] = job['func'](job['args'])
        except Exception:
            job['result'] = _text(traceback.format_exc(), True)
        job['event'].set()


# --- xsi_execute ------------------------------------------------------------------------

def _globals_for_code():
    if not _namespace:
        _namespace.update({'c': c, 'win32com': win32com, '__name__': '__mcp__'})
    _namespace.update(_host)  # this callback's objects; earlier ones may be gone
    return _namespace


class _Tee(object):
    LIMIT = 1 << 20

    def __init__(self, forward):
        self.forward, self.parts, self.size = forward, [], 0

    def write(self, s):
        if not isinstance(s, unicode):
            s = str(s).decode('utf-8', 'replace')
        if self.size < self.LIMIT:
            self.parts.append(s)
            self.size += len(s)
        try:
            self.forward.write(s)
        except Exception:
            pass

    def flush(self):
        pass

    def text(self):
        t = u''.join(self.parts)
        return t if self.size < self.LIMIT else t + u'\n[output truncated]'


def _describe(v, depth=0):
    if v is None or isinstance(v, (bool, int, long, float)):
        return v
    if isinstance(v, basestring):
        return v
    if depth > 4:
        return repr(v)
    if isinstance(v, (list, tuple)):
        return [_describe(x, depth + 1) for x in v[:1000]]
    if isinstance(v, dict):
        return dict((unicode(k), _describe(x, depth + 1)) for k, x in v.items())
    try:
        return unicode(v.FullName)
    except Exception:
        pass
    try:
        n = v.Count
        return [_describe(v.Item(i), depth + 1) for i in range(min(n, 1000))]
    except Exception:
        pass
    return repr(v)


def _execute(args):
    code = args.get('code') or ''
    ns = _globals_for_code()
    out = _Tee(sys.stdout)
    saved = sys.stdout, sys.stderr
    sys.stdout = sys.stderr = out
    value, failure = None, None
    try:
        tree = ast.parse(code, '<mcp>', 'exec')
        last = None
        if tree.body and isinstance(tree.body[-1], ast.Expr):
            last = ast.Expression(tree.body.pop().value)
        exec compile(tree, '<mcp>', 'exec') in ns
        if last is not None:
            value = eval(compile(last, '<mcp>', 'eval'), ns)
    except Exception:
        failure = traceback.format_exc()
    finally:
        sys.stdout, sys.stderr = saved
    parts = []
    if out.text():
        parts.append(out.text().rstrip('\n'))
    if value is not None:
        parts.append(u'=> ' + json.dumps(_describe(value)))
    if failure:
        parts.append(failure.rstrip('\n'))
    return _text(u'\n'.join(parts) or u'(no output)', failure is not None)


# --- xsi_capture ------------------------------------------------------------------------

def _capture(args):
    target = args.get('target') or 'window'
    if target == 'window':
        data = _grab_window(int(args.get('max_width') or 1600), args.get('source') or 'screen')
        return {'content': [{'type': 'image', 'data': base64.b64encode(data), 'mimeType': 'image/png'}],
                'isError': False}
    if target in ('A', 'B', 'C', 'D'):
        data, mime = _capture_viewport(target)
        return {'content': [{'type': 'image', 'data': base64.b64encode(data), 'mimeType': mime}],
                'isError': False}
    return _text("target is 'window' or a viewport letter A to D", True)


def _capture_viewport(letter):
    Application = _host['Application']
    options = Application.Dictionary.GetObject('ViewportCapture').NestedObjects
    params = dict((p.ScriptName, p) for p in options)
    wanted = ('Filename', 'Start Frame', 'End Frame')
    keys = {}
    for want in wanted:
        match = [k for k, p in params.items() if want.replace(' ', '').lower() in
                 (k.lower(), p.Name.replace(' ', '').lower())]
        if not match:
            raise RuntimeError('ViewportCapture has no %r parameter; it has %s' % (want, sorted(params)))
        keys[want] = match[0]
    saved = dict((k, params[k].Value) for k in keys.values())
    folder = tempfile.mkdtemp(prefix='xsi-mcp-')
    try:
        frame = Application.GetValue('PlayControl.Current')
        params[keys['Filename']].Value = os.path.join(folder, 'capture.png')
        params[keys['Start Frame']].Value = frame
        params[keys['End Frame']].Value = frame
        Application.CaptureViewport('ABCD'.index(letter) + 1, False)
        files = [os.path.join(folder, f) for f in os.listdir(folder)]
        if not files:
            raise RuntimeError('CaptureViewport wrote no file')
        path = max(files, key=os.path.getsize)
        with open(path, 'rb') as f:
            data = f.read()
        ext = os.path.splitext(path)[1].lower()
        mime = {'.png': 'image/png', '.jpg': 'image/jpeg', '.jpeg': 'image/jpeg'}.get(ext, 'image/png')
        return data, mime
    finally:
        for k, v in saved.items():
            try:
                params[k].Value = v
            except Exception:
                pass
        shutil.rmtree(folder, True)


class _BITMAPINFOHEADER(ctypes.Structure):
    _fields_ = [('biSize', wintypes.DWORD), ('biWidth', wintypes.LONG), ('biHeight', wintypes.LONG),
                ('biPlanes', wintypes.WORD), ('biBitCount', wintypes.WORD),
                ('biCompression', wintypes.DWORD), ('biSizeImage', wintypes.DWORD),
                ('biXPelsPerMeter', wintypes.LONG), ('biYPelsPerMeter', wintypes.LONG),
                ('biClrUsed', wintypes.DWORD), ('biClrImportant', wintypes.DWORD)]


def _gdi():
    u, g = ctypes.windll.user32, ctypes.windll.gdi32
    H = ctypes.c_void_p
    for f, res, argt in [
            (u.GetDC, H, [H]), (u.ReleaseDC, ctypes.c_int, [H, H]),
            (u.GetWindowRect, wintypes.BOOL, [H, ctypes.POINTER(wintypes.RECT)]),
            (u.PrintWindow, wintypes.BOOL, [H, H, wintypes.UINT]),
            (u.IsWindowVisible, wintypes.BOOL, [H]), (u.GetWindow, H, [H, wintypes.UINT]),
            (u.GetWindowThreadProcessId, wintypes.DWORD, [H, ctypes.POINTER(wintypes.DWORD)]),
            (g.CreateCompatibleDC, H, [H]), (g.CreateCompatibleBitmap, H, [H, ctypes.c_int, ctypes.c_int]),
            (g.SelectObject, H, [H, H]), (g.DeleteObject, wintypes.BOOL, [H]), (g.DeleteDC, wintypes.BOOL, [H]),
            (g.BitBlt, wintypes.BOOL, [H] + [ctypes.c_int] * 4 + [H, ctypes.c_int, ctypes.c_int, wintypes.DWORD]),
            (g.StretchBlt, wintypes.BOOL, [H] + [ctypes.c_int] * 4 + [H] + [ctypes.c_int] * 4 + [wintypes.DWORD]),
            (g.SetStretchBltMode, ctypes.c_int, [H, ctypes.c_int]),
            (g.SetBrushOrgEx, wintypes.BOOL, [H, ctypes.c_int, ctypes.c_int, ctypes.c_void_p]),
            (g.GetDIBits, ctypes.c_int, [H, H, wintypes.UINT, wintypes.UINT, ctypes.c_void_p, ctypes.c_void_p, wintypes.UINT])]:
        f.restype, f.argtypes = res, argt
    return u, g


def _main_window(u):
    pid = os.getpid()
    best = [None, 0]
    proc = ctypes.WINFUNCTYPE(wintypes.BOOL, ctypes.c_void_p, ctypes.c_void_p)

    def visit(hwnd, _):
        owner = wintypes.DWORD()
        u.GetWindowThreadProcessId(hwnd, ctypes.byref(owner))
        if owner.value == pid and u.IsWindowVisible(hwnd) and not u.GetWindow(hwnd, 4):  # GW_OWNER
            r = wintypes.RECT()
            u.GetWindowRect(hwnd, ctypes.byref(r))
            area = (r.right - r.left) * (r.bottom - r.top)
            if area > best[1]:
                best[0], best[1] = hwnd, area
        return True

    ctypes.windll.user32.EnumWindows(proc(visit), None)
    if not best[0]:
        raise RuntimeError('no visible Softimage window')
    return best[0]


def _grab_window(max_width, source):
    u, g = _gdi()
    hwnd = _main_window(u)
    r = wintypes.RECT()
    u.GetWindowRect(hwnd, ctypes.byref(r))
    w, h = r.right - r.left, r.bottom - r.top
    tw, th = (w, h) if w <= max_width else (max_width, max(1, h * max_width // w))
    screen = u.GetDC(None)
    src_dc, dst_dc = g.CreateCompatibleDC(screen), g.CreateCompatibleDC(screen)
    src_bmp, dst_bmp = g.CreateCompatibleBitmap(screen, w, h), g.CreateCompatibleBitmap(screen, tw, th)
    old_src, old_dst = g.SelectObject(src_dc, src_bmp), g.SelectObject(dst_dc, dst_bmp)
    try:
        if source == 'print':
            ok = u.PrintWindow(hwnd, src_dc, 2)  # PW_RENDERFULLCONTENT
        else:
            ok = g.BitBlt(src_dc, 0, 0, w, h, screen, r.left, r.top, 0x00CC0020 | 0x40000000)  # SRCCOPY | CAPTUREBLT
        if not ok:
            raise RuntimeError('%s capture failed' % source)
        g.SetStretchBltMode(dst_dc, 4)  # HALFTONE
        g.SetBrushOrgEx(dst_dc, 0, 0, None)
        g.StretchBlt(dst_dc, 0, 0, tw, th, src_dc, 0, 0, w, h, 0x00CC0020)
        g.SelectObject(dst_dc, old_dst)
        old_dst = None
        info = _BITMAPINFOHEADER()
        info.biSize, info.biWidth, info.biHeight = ctypes.sizeof(info), tw, -th  # top-down
        info.biPlanes, info.biBitCount = 1, 32
        pixels = ctypes.create_string_buffer(tw * th * 4)
        if g.GetDIBits(dst_dc, dst_bmp, 0, th, pixels, ctypes.byref(info), 0) != th:
            raise RuntimeError('GetDIBits failed')
        return _png(tw, th, bytearray(pixels.raw))
    finally:
        g.SelectObject(src_dc, old_src)
        if old_dst:
            g.SelectObject(dst_dc, old_dst)
        for o in (src_bmp, dst_bmp):
            g.DeleteObject(o)
        for d in (src_dc, dst_dc):
            g.DeleteDC(d)
        u.ReleaseDC(None, screen)


def _png(w, h, bgra):
    rgb = bytearray(w * h * 3)
    rgb[0::3], rgb[1::3], rgb[2::3] = bgra[2::4], bgra[1::4], bgra[0::4]
    stride = w * 3
    raw = ''.join('\x00' + str(rgb[y * stride:(y + 1) * stride]) for y in xrange(h))

    def chunk(tag, data):
        return struct.pack('>I', len(data)) + tag + data + struct.pack('>I', zlib.crc32(tag + data) & 0xffffffff)

    return ('\x89PNG\r\n\x1a\n' + chunk('IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0)) +
            chunk('IDAT', zlib.compress(raw, 6)) + chunk('IEND', ''))


_TOOL_FUNCS = {'xsi_execute': _execute, 'xsi_capture': _capture}
