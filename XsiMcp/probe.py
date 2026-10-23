"""A stdlib MCP client for XsiMcp (Python 3):
probe.py [list] | exec "<python>" | capture [window|A-D] [out.png] [screen|print] | register [name]"""
import base64
import glob
import json
import os
import sys
import urllib.request

def _connection():
    # XSI_MCP_CONNECTION, else the newest connection.json of this repository's workgroup
    # or of an XsiMcp add-on installed in a Softimage user folder.
    if os.environ.get('XSI_MCP_CONNECTION'):
        return os.environ['XSI_MCP_CONNECTION']
    here = os.path.dirname(os.path.abspath(__file__))
    tail = os.path.join('Addons', 'XsiMcp', 'Application', 'xsi_mcp', 'connection.json')
    found = glob.glob(os.path.join(here, '..', 'Workgroup', tail))
    found += glob.glob(os.path.join(os.path.expanduser('~'), 'Autodesk', 'Softimage_*', tail))
    if not found:
        sys.exit('No connection.json: start Softimage with XsiMcp, or set XSI_MCP_CONNECTION.')
    return max(found, key=os.path.getmtime)


CONNECTION = _connection()


def call(method, params=None, mid=1):
    with open(CONNECTION) as f:
        conn = json.load(f)
    body = json.dumps({'jsonrpc': '2.0', 'id': mid, 'method': method, 'params': params or {}}).encode()
    req = urllib.request.Request(conn['url'], body, {
        'Content-Type': 'application/json', 'Accept': 'application/json, text/event-stream',
        'Authorization': 'Bearer ' + conn['token']})
    with urllib.request.urlopen(req, timeout=600) as r:
        return json.load(r)


def tool(name, args):
    reply = call('tools/call', {'name': name, 'arguments': args})
    if 'error' in reply:
        sys.exit('error: %s' % reply['error'])
    return reply['result']


if __name__ == '__main__':
    cmd = sys.argv[1] if len(sys.argv) > 1 else 'list'
    if cmd == 'list':
        print(json.dumps(call('initialize', {'protocolVersion': '2025-06-18'})['result'], indent=2))
        print([t['name'] for t in call('tools/list')['result']['tools']])
    elif cmd == 'exec':
        result = tool('xsi_execute', {'code': sys.argv[2]})
        print(('ERROR\n' if result['isError'] else '') + result['content'][0]['text'])
    elif cmd == 'register':
        # Registers the endpoint with Claude Code without showing the token.
        import subprocess
        with open(CONNECTION) as f:
            conn = json.load(f)
        name = sys.argv[2] if len(sys.argv) > 2 else 'xsi'
        subprocess.call('claude mcp remove %s -s user' % name, shell=True,
                        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        sys.exit(subprocess.call('claude mcp add --scope user --transport http %s %s --header "Authorization: Bearer %s"'
                                 % (name, conn['url'], conn['token']), shell=True))
    elif cmd == 'capture':
        target = sys.argv[2] if len(sys.argv) > 2 else 'window'
        out = sys.argv[3] if len(sys.argv) > 3 else 'capture.png'
        args = {'target': target}
        if len(sys.argv) > 4:
            args['source'] = sys.argv[4]
        result = tool('xsi_capture', args)
        block = result['content'][0]
        if block['type'] != 'image':
            sys.exit(block['text'])
        with open(out, 'wb') as f:
            f.write(base64.b64decode(block['data']))
        print('%s: %d bytes, %s' % (out, os.path.getsize(out), block['mimeType']))
