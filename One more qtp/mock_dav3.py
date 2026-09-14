
import os, sys, email.utils, urllib.parse
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

ROOT = r"D:\QtpDav"
PORT = 8899
REDIRECT = os.environ.get("DAV_REDIRECT", "1") == "1"

def fs_path(url_path):
    p = urllib.parse.unquote(url_path.split('?')[0]).lstrip('/')
    full = os.path.normpath(os.path.join(ROOT, p.replace('/', os.sep)))
    return full if full.startswith(os.path.normpath(ROOT)) else None

def href_for(path):
    rel = os.path.relpath(path, ROOT).replace(os.sep, '/')
    return '/' if rel == '.' else '/' + rel + ('/' if os.path.isdir(path) else '')

def resp_block(href, path):
    is_dir = os.path.isdir(path)
    mt = email.utils.formatdate(os.path.getmtime(path), usegmt=True)
    rt = '<D:resourcetype><D:collection/></D:resourcetype>' if is_dir else '<D:resourcetype/>'
    sz = '' if is_dir else '<D:getcontentlength>%d</D:getcontentlength>' % os.path.getsize(path)
    return ('<D:response><D:href>%s</D:href><D:propstat><D:prop>%s%s'
            '<D:getlastmodified>%s</D:getlastmodified></D:prop>'
            '<D:status>HTTP/1.1 200 OK</D:status></D:propstat></D:response>') % (href, rt, sz, mt)

class H(BaseHTTPRequestHandler):
    protocol_version = 'HTTP/1.1'
    def log_message(self, *a): pass
    def _drain(self):
        n = int(self.headers.get('Content-Length', 0) or 0)
        if n > 0: self.rfile.read(n)
    def _send(self, code, body=b'', ctype='text/plain', extra=None):
        self.send_response(code)
        if extra:
            for k, v in extra.items(): self.send_header(k, v)
        self.send_header('Content-Type', ctype)
        self.send_header('Content-Length', str(len(body)))
        self.end_headers()
        if body: self.wfile.write(body)
    def _autoindex(self, p):
        names = sorted(os.listdir(p)) if os.path.isdir(p) else []
        items = ''.join('<li><a href="%s">%s</a></li>' % (n, n) for n in names)
        return ('<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 3.2 Final//EN">\n'
                '<html><head><title>Index of %s</title></head><body>'
                '<h1>Index of %s</h1><ul>%s</ul></body></html>'
                % (self.path, self.path, items)).encode('utf-8')
    def do_PROPFIND(self):
        self._drain()
        sys.stderr.write("PROPFIND->%s depth=%s\n" % (self.path, self.headers.get('Depth')))
        if REDIRECT and not self.path.endswith('/') and os.path.isdir(fs_path(self.path) or ''):
            self._send(301, b'', 'text/html', {'Location': self.path + '/'})
            sys.stderr.write("  -> 301 redirect to %s/\n" % self.path)
            return
        p = fs_path(self.path)
        if not p or not os.path.exists(p):
            self._send(404, b'not found'); return
        parts = [resp_block(href_for(p), p)]
        if self.headers.get('Depth', '1') != '0' and os.path.isdir(p):
            for name in sorted(os.listdir(p)):
                parts.append(resp_block(href_for(os.path.join(p, name)), os.path.join(p, name)))
        body = ('<?xml version="1.0" encoding="utf-8"?>'
                '<D:multistatus xmlns:D="DAV:">' + ''.join(parts) + '</D:multistatus>').encode('utf-8')
        self._send(207, body, 'application/xml; charset=utf-8')
    def do_GET(self):
        self._drain()
        sys.stderr.write("GET->%s\n" % self.path)
        p = fs_path(self.path)
        if not p or not os.path.exists(p): self._send(404, b'nf'); return
        if os.path.isdir(p):
            self._send(200, self._autoindex(p), 'text/html; charset=utf-8'); return
        with open(p, 'rb') as f: self._send(200, f.read(), 'application/octet-stream')
    def do_MKCOL(self):
        self._drain()
        sys.stderr.write("MKCOL->%s\n" % self.path)
        p = fs_path(self.path)
        if not p: self._send(403); return
        if os.path.isdir(p): self._send(405); return
        try: os.makedirs(p); self._send(201)
        except OSError: self._send(409)
    def do_PUT(self):
        p = fs_path(self.path)
        if not p: self._send(403); return
        os.makedirs(os.path.dirname(p), exist_ok=True)
        open(p,'wb').write(self.rfile.read(int(self.headers.get('Content-Length', 0))))
        self._send(201)
    def do_DELETE(self):
        self._drain()
        sys.stderr.write("DELETE->%s\n" % self.path)
        if REDIRECT and not self.path.endswith('/') and os.path.isdir(fs_path(self.path) or ''):
            self._send(301, b'', 'text/html', {'Location': self.path + '/'})
            return
        p = fs_path(self.path)
        if not p or not os.path.exists(p): self._send(404); return
        import shutil
        shutil.rmtree(p) if os.path.isdir(p) else os.remove(p)
        self._send(204)
    def do_OPTIONS(self):
        self.send_response(200); self.send_header('DAV','1'); self.send_header('Content-Length','0'); self.end_headers()
    def do_HEAD(self):
        self._drain(); self._send(200)

os.makedirs(ROOT, exist_ok=True)
print("mock(redirect=%s) on 127.0.0.1:%d" % (REDIRECT, PORT))
ThreadingHTTPServer(('127.0.0.1', PORT), H).serve_forever()
