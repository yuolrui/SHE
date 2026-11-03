import React, { useState, useEffect } from 'react';
import { Play, Square, RotateCw, Send, Terminal, ShieldCheck, CheckCircle2, AlertTriangle, Clock, Layers } from 'lucide-react';

interface ServerStatus {
  isRunning: boolean;
  binaryExists: boolean;
  pid: number | null;
  port: number;
  uptime: number;
  docRoot: string;
  logs: string[];
}

interface HttpResponse {
  statusCode: number;
  statusMessage: string;
  headers: Record<string, string>;
  body: string;
  bodyLength: number;
  latencyMs: number;
}

const PRESETS = [
  {
    name: '200 OK (HTML Document)',
    method: 'GET',
    path: '/index.html',
    headers: {},
    desc: 'Fetches default index page with Content-Type text/html and full body.',
  },
  {
    name: '200 OK (JSON API)',
    method: 'GET',
    path: '/data.json',
    headers: {},
    desc: 'Fetches structured JSON data with Content-Type application/json.',
  },
  {
    name: '206 Partial Content (HTTP Range)',
    method: 'GET',
    path: '/style.css',
    headers: { Range: 'bytes=0-49' },
    desc: 'Tests HTTP Range header with zero-copy sendfile(2) offset slicing.',
  },
  {
    name: '304 Not Modified (Conditional GET)',
    method: 'GET',
    path: '/index.html',
    headers: { 'If-Modified-Since': new Date().toUTCString() },
    desc: 'Tests client cache validation without retransmitting unchanged body.',
  },
  {
    name: 'mod_status (/server-status)',
    method: 'GET',
    path: '/server-status',
    headers: {},
    desc: 'Lighttpd online metrics dashboard with uptime, active connections, and RPS.',
  },
  {
    name: 'mod_status JSON (/server-status?json)',
    method: 'GET',
    path: '/server-status?json',
    headers: {},
    desc: 'Machine-readable JSON performance telemetry provided by mod_status.',
  },
  {
    name: 'mod_dirlisting (/docs/)',
    method: 'GET',
    path: '/docs/',
    headers: {},
    desc: 'Automatic HTML directory indexing tree for folders without index.html.',
  },
  {
    name: 'HEAD Request (Headers Only)',
    method: 'HEAD',
    path: '/readme.txt',
    headers: {},
    desc: 'Returns headers and Content-Length with zero payload body transfer.',
  },
  {
    name: '404 Not Found (Missing Resource)',
    method: 'GET',
    path: '/non_existent_file.html',
    headers: {},
    desc: 'Triggers lighttpd error generator with styled HTML error response.',
  },
  {
    name: '405 Method Not Allowed (POST)',
    method: 'POST',
    path: '/index.html',
    headers: {},
    desc: 'Static file server safely rejects write methods on read-only endpoints.',
  },
  {
    name: 'Security Test (/../../etc/passwd)',
    method: 'GET',
    path: '/../../etc/passwd',
    headers: {},
    desc: 'Verifies path canonicalization prevents directory traversal attacks.',
  },
];

export const LiveTester: React.FC = () => {
  const [status, setStatus] = useState<ServerStatus | null>(null);
  const [method, setMethod] = useState<string>('GET');
  const [path, setPath] = useState<string>('/index.html');
  const [customHeaders, setCustomHeaders] = useState<string>('');
  const [isSending, setIsSending] = useState<boolean>(false);
  const [response, setResponse] = useState<HttpResponse | null>(null);
  const [activeTab, setActiveTab] = useState<'body' | 'headers' | 'wire' | 'logs'>('body');
  const [compilerOutput, setCompilerOutput] = useState<string | null>(null);
  const [isCompiling, setIsCompiling] = useState<boolean>(false);

  const fetchStatus = async () => {
    try {
      const res = await fetch('/api/c-server/status');
      if (!res.ok) return;
      const contentType = res.headers.get('content-type') || '';
      if (!contentType.includes('application/json')) return;
      const data = await res.json();
      if (data) {
        setStatus(data);
      }
    } catch {
      // Silently ignore transient network or initial startup states
    }
  };

  useEffect(() => {
    fetchStatus();
    const interval = setInterval(fetchStatus, 3000);
    return () => clearInterval(interval);
  }, []);

  const handleStart = async () => {
    try {
      await fetch('/api/c-server/start', { method: 'POST' });
    } catch {}
    fetchStatus();
  };

  const handleStop = async () => {
    try {
      await fetch('/api/c-server/stop', { method: 'POST' });
    } catch {}
    fetchStatus();
  };

  const handleCompile = async () => {
    setIsCompiling(true);
    setCompilerOutput('Compiling with gcc -Wall -Wextra -O2...');
    try {
      const res = await fetch('/api/c-server/compile', { method: 'POST' });
      if (res.ok) {
        const contentType = res.headers.get('content-type') || '';
        if (contentType.includes('application/json')) {
          const data = await res.json();
          setCompilerOutput(data.output || 'Compilation finished cleanly with 0 warnings.');
        }
      }
      fetchStatus();
    } catch (err: any) {
      setCompilerOutput(`Compilation failed: ${err.message}`);
    } finally {
      setIsCompiling(false);
    }
  };

  const handleSendRequest = async () => {
    setIsSending(true);
    let parsedHeaders: Record<string, string> = {};

    if (customHeaders.trim()) {
      customHeaders.split('\n').forEach((line) => {
        const colon = line.indexOf(':');
        if (colon > 0) {
          const k = line.substring(0, colon).trim();
          const v = line.substring(colon + 1).trim();
          if (k) parsedHeaders[k] = v;
        }
      });
    }

    try {
      const res = await fetch('/api/c-server/request', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          method,
          path,
          headers: parsedHeaders,
        }),
      });
      if (res.ok) {
        const contentType = res.headers.get('content-type') || '';
        if (contentType.includes('application/json')) {
          const data = await res.json();
          setResponse(data);
        }
      }
    } catch {
      // Request failed
    } finally {
      setIsSending(false);
      fetchStatus();
    }
  };

  const applyPreset = (preset: typeof PRESETS[0]) => {
    setMethod(preset.method);
    setPath(preset.path);
    const headerLines = Object.entries(preset.headers)
      .map(([k, v]) => `${k}: ${v}`)
      .join('\n');
    setCustomHeaders(headerLines);
  };

  const formatWireView = (resp: HttpResponse) => {
    const headerLines = Object.entries(resp.headers)
      .map(([k, v]) => `${k}: ${v}`)
      .join('\r\n');
    return `HTTP/1.1 ${resp.statusCode} ${resp.statusMessage}\r\n${headerLines}\r\n\r\n${resp.body}`;
  };

  return (
    <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8 py-8 space-y-6">
      {/* Server Status Header Card */}
      <div className="bg-slate-900/70 border border-slate-800 rounded-xl p-5 flex flex-col md:flex-row items-start md:items-center justify-between gap-4">
        <div className="flex items-center gap-4">
          <div
            className={`w-10 h-10 rounded-lg flex items-center justify-center ${
              status?.isRunning ? 'bg-emerald-500/10 text-emerald-400 border border-emerald-500/30' : 'bg-slate-800 text-slate-400 border border-slate-700'
            }`}
          >
            <Clock className="w-5 h-5" />
          </div>
          <div>
            <div className="flex items-center gap-2">
              <h3 className="text-sm font-bold text-white">c-lighttpd Daemon</h3>
              <span
                className={`text-[11px] font-mono px-2 py-0.5 rounded ${
                  status?.isRunning ? 'bg-emerald-950/80 text-emerald-400 border border-emerald-800/80' : 'bg-slate-800 text-slate-400'
                }`}
              >
                {status?.isRunning ? `PID ${status?.pid} · Running` : 'Stopped'}
              </span>
            </div>
            <div className="flex items-center gap-2 text-xs text-slate-400 font-mono mt-0.5">
              <span>Port: {status?.port || 8899}</span>
              <span aria-hidden="true">·</span>
              <span>Uptime: {status?.uptime || 0}s</span>
              <span aria-hidden="true">·</span>
              <span className="truncate max-w-[220px]">DocRoot: {status?.docRoot}</span>
            </div>
          </div>
        </div>

        <div className="flex items-center gap-2.5 w-full md:w-auto">
          {status?.isRunning ? (
            <button
              onClick={handleStop}
              className="flex-1 md:flex-initial flex items-center justify-center gap-1.5 px-3.5 py-1.5 text-xs font-medium text-rose-300 bg-rose-950/40 hover:bg-rose-900/50 border border-rose-800/80 rounded-md transition-colors"
            >
              <Square className="w-3.5 h-3.5 fill-current" />
              <span>Stop Server</span>
            </button>
          ) : (
            <button
              onClick={handleStart}
              className="flex-1 md:flex-initial flex items-center justify-center gap-1.5 px-3.5 py-1.5 text-xs font-medium text-emerald-300 bg-emerald-950/50 hover:bg-emerald-900/60 border border-emerald-800/80 rounded-md transition-colors"
            >
              <Play className="w-3.5 h-3.5 fill-current" />
              <span>Start Server</span>
            </button>
          )}

          <button
            onClick={handleCompile}
            disabled={isCompiling}
            className="flex-1 md:flex-initial flex items-center justify-center gap-1.5 px-3.5 py-1.5 text-xs font-medium text-slate-300 bg-slate-800 hover:bg-slate-700 border border-slate-700 rounded-md transition-colors disabled:opacity-50"
          >
            <RotateCw className={`w-3.5 h-3.5 text-sky-400 ${isCompiling ? 'animate-spin' : ''}`} />
            <span>Recompile (GCC)</span>
          </button>
        </div>
      </div>

      {/* Compiler Log Banner (if open) */}
      {compilerOutput && (
        <div className="bg-slate-950 border border-slate-800 rounded-xl p-4 font-mono text-xs space-y-2">
          <div className="flex items-center justify-between text-slate-400 border-b border-slate-800/80 pb-2">
            <span className="flex items-center gap-1.5 text-sky-400">
              <Terminal className="w-3.5 h-3.5" />
              <span>GCC Compiler Console</span>
            </span>
            <button
              onClick={() => setCompilerOutput(null)}
              className="text-slate-500 hover:text-slate-300 text-[11px]"
            >
              Close
            </button>
          </div>
          <pre className="text-slate-300 whitespace-pre-wrap leading-relaxed max-h-40 overflow-y-auto">
            {compilerOutput}
          </pre>
        </div>
      )}

      {/* HTTP Presets Carousel */}
      <div className="space-y-2">
        <span className="text-xs font-semibold text-slate-300 uppercase tracking-wider">
          Request Scenarios & Test Presets
        </span>
        <div className="grid grid-cols-2 sm:grid-cols-4 gap-2">
          {PRESETS.map((p) => (
            <button
              key={p.name}
              onClick={() => applyPreset(p)}
              className="flex flex-col text-left p-2.5 rounded-lg bg-slate-900/60 hover:bg-slate-800/80 border border-slate-800 hover:border-slate-700 transition-all text-xs"
            >
              <div className="flex items-center justify-between w-full mb-1">
                <span className="font-semibold text-sky-300 truncate">{p.name}</span>
                <span className="text-[10px] font-mono text-slate-400 shrink-0">{p.method}</span>
              </div>
              <p className="text-[11px] text-slate-400 line-clamp-1">{p.desc}</p>
            </button>
          ))}
        </div>
      </div>

      {/* Main Request / Response Workbench */}
      <div className="grid grid-cols-1 lg:grid-cols-12 gap-6 items-start">
        {/* Left: Request Form (5 cols) */}
        <div className="lg:col-span-5 bg-slate-900/70 border border-slate-800 rounded-xl p-5 space-y-4">
          <h3 className="text-sm font-bold text-white tracking-tight">HTTP Client Workbench</h3>

          <div className="space-y-3">
            <div>
              <label className="block text-xs font-medium text-slate-400 mb-1">Method & Resource Path</label>
              <div className="flex gap-2">
                <select
                  value={method}
                  onChange={(e) => setMethod(e.target.value)}
                  className="bg-slate-950 border border-slate-800 rounded-md px-2.5 py-1.5 text-xs font-mono text-sky-400 focus:outline-none focus:border-sky-500"
                >
                  <option value="GET">GET</option>
                  <option value="HEAD">HEAD</option>
                  <option value="POST">POST</option>
                  <option value="OPTIONS">OPTIONS</option>
                </select>
                <input
                  type="text"
                  value={path}
                  onChange={(e) => setPath(e.target.value)}
                  placeholder="/index.html"
                  className="flex-1 bg-slate-950 border border-slate-800 rounded-md px-3 py-1.5 text-xs font-mono text-slate-200 placeholder-slate-500 focus:outline-none focus:border-sky-500"
                />
              </div>
            </div>

            <div>
              <label className="block text-xs font-medium text-slate-400 mb-1">
                Custom Headers (one per line, e.g. <code className="text-slate-300 font-mono">Range: bytes=0-100</code>)
              </label>
              <textarea
                value={customHeaders}
                onChange={(e) => setCustomHeaders(e.target.value)}
                placeholder="Range: bytes=0-50&#10;If-Modified-Since: Sat, 31 Oct 2026 00:00:00 GMT"
                rows={4}
                className="w-full bg-slate-950 border border-slate-800 rounded-md p-3 text-xs font-mono text-slate-200 placeholder-slate-600 focus:outline-none focus:border-sky-500"
              />
            </div>

            <button
              onClick={handleSendRequest}
              disabled={isSending}
              className="w-full flex items-center justify-center gap-2 px-4 py-2 text-xs font-semibold text-white bg-sky-600 hover:bg-sky-500 rounded-md transition-colors shadow-sm disabled:opacity-50"
            >
              <Send className="w-3.5 h-3.5" />
              <span>{isSending ? 'Sending Request...' : 'Send HTTP Request'}</span>
            </button>
          </div>

          <div className="pt-3 border-t border-slate-800/80 text-[11px] text-slate-400 space-y-1">
            <div className="flex items-center gap-1.5 text-emerald-400">
              <ShieldCheck className="w-3.5 h-3.5" />
              <span>Traversal Guard Active: /../ paths automatically normalized</span>
            </div>
            <p className="text-slate-500">Target server: 127.0.0.1:{status?.port || 8899}</p>
          </div>
        </div>

        {/* Right: Response Inspector (7 cols) */}
        <div className="lg:col-span-7 bg-slate-950 border border-slate-800 rounded-xl overflow-hidden flex flex-col">
          {/* Header strip */}
          <div className="flex items-center justify-between px-4 py-2.5 bg-slate-900 border-b border-slate-800">
            <div className="flex items-center gap-3">
              <span className="text-xs font-semibold text-white">HTTP Response Inspector</span>
              {response && (
                <span
                  className={`text-xs font-mono px-2 py-0.5 rounded font-bold ${
                    response.statusCode === 200 || response.statusCode === 206
                      ? 'bg-emerald-950/80 text-emerald-400 border border-emerald-800/80'
                      : response.statusCode === 304
                      ? 'bg-blue-950/80 text-blue-400 border border-blue-800/80'
                      : 'bg-amber-950/80 text-amber-400 border border-amber-800/80'
                  }`}
                >
                  {response.statusCode} {response.statusMessage}
                </span>
              )}
            </div>

            {response && (
              <div className="flex items-center gap-2 text-xs font-mono text-slate-400">
                <Clock className="w-3 h-3 text-sky-400" />
                <span className="tabular-nums">{response.latencyMs} ms</span>
                <span aria-hidden="true">·</span>
                <span className="tabular-nums">{response.bodyLength} bytes</span>
              </div>
            )}
          </div>

          {/* Tab Selector */}
          <div className="flex items-center gap-1 px-4 py-2 bg-slate-900/50 border-b border-slate-800 text-xs">
            <button
              onClick={() => setActiveTab('body')}
              className={`px-2.5 py-1 rounded transition-colors ${
                activeTab === 'body' ? 'bg-slate-800 text-sky-400 font-medium' : 'text-slate-400 hover:text-slate-200'
              }`}
            >
              Response Body
            </button>
            <button
              onClick={() => setActiveTab('headers')}
              className={`px-2.5 py-1 rounded transition-colors ${
                activeTab === 'headers' ? 'bg-slate-800 text-sky-400 font-medium' : 'text-slate-400 hover:text-slate-200'
              }`}
            >
              Headers Table
            </button>
            <button
              onClick={() => setActiveTab('wire')}
              className={`px-2.5 py-1 rounded transition-colors ${
                activeTab === 'wire' ? 'bg-slate-800 text-sky-400 font-medium' : 'text-slate-400 hover:text-slate-200'
              }`}
            >
              Raw Wire (curl style)
            </button>
            <button
              onClick={() => setActiveTab('logs')}
              className={`px-2.5 py-1 rounded transition-colors ${
                activeTab === 'logs' ? 'bg-slate-800 text-sky-400 font-medium' : 'text-slate-400 hover:text-slate-200'
              }`}
            >
              Server Logs
            </button>
          </div>

          {/* Content Area */}
          <div className="p-4 font-mono text-xs min-h-[300px] max-h-[500px] overflow-y-auto bg-slate-950 text-slate-300">
            {!response && activeTab !== 'logs' ? (
              <div className="py-24 text-center text-slate-500">
                Select a preset above or click &quot;Send HTTP Request&quot; to probe the running C server.
              </div>
            ) : activeTab === 'body' ? (
              <pre className="whitespace-pre-wrap leading-relaxed text-slate-300">
                {response?.body || '[Empty response body]'}
              </pre>
            ) : activeTab === 'headers' ? (
              <table className="w-full border-collapse">
                <thead>
                  <tr className="border-b border-slate-800 text-left text-slate-500 text-[11px]">
                    <th className="py-1.5 font-medium">Header Name</th>
                    <th className="py-1.5 font-medium">Value</th>
                  </tr>
                </thead>
                <tbody>
                  {Object.entries(response?.headers || {}).map(([key, val]) => (
                    <tr key={key} className="border-b border-slate-900 hover:bg-slate-900/40">
                      <td className="py-1.5 text-sky-400 font-medium">{key}</td>
                      <td className="py-1.5 text-slate-300">{val}</td>
                    </tr>
                  ))}
                </tbody>
              </table>
            ) : activeTab === 'wire' ? (
              <pre className="whitespace-pre text-emerald-400/90 leading-relaxed">
                {response ? formatWireView(response) : ''}
              </pre>
            ) : (
              <div className="space-y-1">
                {(status?.logs || []).map((log, idx) => (
                  <div key={idx} className="text-slate-400 font-mono text-[11px]">
                    {log}
                  </div>
                ))}
              </div>
            )}
          </div>
        </div>
      </div>
    </div>
  );
};
