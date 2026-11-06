import React, { useState, useEffect } from 'react';
import { FileCode, Folder, Copy, Check, Download, Search, Terminal, Info } from 'lucide-react';

interface FileData {
  path: string;
  name: string;
  folder: string;
  content: string;
  lines: number;
  size: number;
}

const FILE_NOTES: Record<string, string> = {
  'src/core/she_server.c': 'SHE Server core orchestrator. Manages socket listener, epoll event dispatching, signal handling, and plugin pipeline initialization.',
  'src/core/she_server.h': 'Header for she_server_t, she_server_config_t, and server response code counters.',
  'src/core/she_connection.c': 'SHE 10-State Connection State Machine. Drives non-blocking I/O, security checks (rate limiting, Host header, smuggling), and zero-copy chunkqueue.',
  'src/core/she_connection.h': 'Header defining she_connection_t, she_con_state_t 10-state enum, and state engine driver signatures.',
  'src/core/she_config.c': 'Runtime configuration file parser. Reads she.conf directives for bindings, security, auth, and virtual hosts.',
  'src/core/she_config.h': 'Configuration loader API prototypes.',
  'src/core/she_chunkqueue.c': 'Zero-copy response queue. Handles SHE_CHUNK_TYPE_MEM for headers and SHE_CHUNK_TYPE_FILE with Linux sendfile(2) zero-copy.',
  'src/core/she_chunkqueue.h': 'Chunk queue structures and non-blocking socket chunk streaming declarations.',
  'src/core/she_buffer.c': 'SHE dynamic buffer memory allocator with bounds-checked operations to eliminate buffer overflow risks.',
  'src/core/she_buffer.h': 'she_buffer_t struct and dynamic string manipulation functions.',
  'src/security/she_security.c': 'SHE Security Hardening engine. Implements per-IP token-bucket rate limiter, Host header RFC validation, smuggling guards, and defense headers injection.',
  'src/security/she_security.h': 'Security config, rate bucket hash table, and security API signatures.',
  'src/modules/she_plugin.c': 'Lighttpd-style modular plugin pipeline manager. Executes lifecycle and request hooks (URI clean, subrequest start, response done).',
  'src/modules/she_plugin.h': 'she_plugin_t, she_handler_t return codes (SHE_HANDLER_GO_ON, FINISHED, ERROR).',
  'src/modules/mod_auth.c': 'HTTP Basic Authentication (RFC 7617) plugin with base64 decoding. Protects /secure/ and /admin/ with 401 challenges.',
  'src/modules/mod_auth.h': 'mod_auth plugin declaration.',
  'src/modules/mod_vhost.c': 'Virtual Host routing plugin. Maps Host header to isolated document roots (docs.local, api.local, secure.local).',
  'src/modules/mod_vhost.h': 'mod_vhost plugin declaration.',
  'src/modules/mod_fastcgi.c': 'FastCGI 1.0 upstream client protocol record generator. Constructs binary FCGI_BEGIN_REQUEST, FCGI_PARAMS, and inspects upstream telemetry.',
  'src/modules/mod_fastcgi.h': 'FastCGI 1.0 protocol binary structures and record headers.',
  'src/modules/mod_status.c': 'SHE online runtime metrics and performance dashboard (/server-status and /server-status?json).',
  'src/modules/mod_status.h': 'mod_status plugin declaration.',
  'src/modules/mod_indexfile.c': 'Directory index resolution cascade (index.html, index.htm, default.html).',
  'src/modules/mod_indexfile.h': 'mod_indexfile plugin declaration.',
  'src/modules/mod_dirlisting.c': 'Dynamic interactive HTML directory indexing for folders without index files.',
  'src/modules/mod_dirlisting.h': 'mod_dirlisting plugin declaration.',
  'src/modules/mod_accesslog.c': 'Standard Combined Log Format (CLF) request logger writing to logs/access.log.',
  'src/modules/mod_accesslog.h': 'mod_accesslog plugin declaration.',
  'src/modules/mod_deflate.c': 'Dynamic Gzip HTTP Content-Encoding compression (RFC 1952 via zlib). Compresses JSON, HTML, CSS on-the-fly with 70%+ bandwidth savings.',
  'src/modules/mod_deflate.h': 'mod_deflate plugin declaration and zlib compression prototypes.',
  'src/modules/mod_rewrite.c': 'URL rewriting and HTTP 301/302 redirection subsystem. Supports transparent path rewrites and Location redirects.',
  'src/modules/mod_rewrite.h': 'mod_rewrite plugin declaration.',
  'src/modules/mod_proxy.c': 'Reverse Proxy Gateway module. Upstream pass-through with X-Forwarded-For, X-Forwarded-Host, and X-Forwarded-Proto headers.',
  'src/modules/mod_proxy.h': 'mod_proxy plugin declaration.',
  'src/modules/mod_websocket.c': 'RFC 6455 WebSocket Handshake Engine. Computes SHA-1 + Base64 Sec-WebSocket-Accept token with GUID 258EAFA5-E914-47DA-95CA-C5AB0DC85B11 and generates 101 Switching Protocols response.',
  'src/modules/mod_websocket.h': 'mod_websocket plugin declaration and she_websocket_compute_accept prototype.',
  'src/modules/mod_ssl.c': 'OpenSSL 3.0 TLS 1.3 / 1.2 Transport Security Context. Configures SSL_CTX, ALPN negotiation advertising h2 and http/1.1, and telemetry reporting via /ssl-status.',
  'src/modules/mod_ssl.h': 'mod_ssl plugin declaration and OpenSSL context structures.',
  'src/modules/mod_http2.c': 'RFC 9113 HTTP/2 24-byte client connection preface detector, 9-byte binary frame header serializer (she_h2_frame_header_t), server SETTINGS frame generator, and /h2-status endpoint.',
  'src/modules/mod_http2.h': 'RFC 9113 HTTP/2 frame types, flags, error codes, and binary frame header definitions.',
  'src/modules/mod_wasm.c': 'WebAssembly 1.0 Serverless Edge Function Runtime. Validates WASM binary header, manages isolated 64KB linear memory sandboxes, and dispatches edge functions (edge_hash, edge_fibonacci, edge_auth_check, edge_filter) via /edge/invoke.',
  'src/modules/mod_wasm.h': 'mod_wasm plugin declarations, WASM section enums, and edge execution function prototypes.',
  'src/modules/mod_ebpf.c': 'Linux eBPF / XDP Kernel Bypass Packet Acceleration & DDoS Mitigation Shield. Emulates in-kernel BPF maps (BPF_MAP_TYPE_HASH) for fast-path IP blacklisting and SYN flood mitigation.',
  'src/modules/mod_ebpf.h': 'mod_ebpf plugin declarations, XDP action enums (XDP_PASS, XDP_DROP), and kernel filter prototypes.',
  'src/modules/mod_acme.c': 'RFC 8555 Automated Certificate Management Environment (ACME v2) Engine. Automates Let\'s Encrypt TLS certificate lifecycle, http-01 challenge responder (/.well-known/acme-challenge/), and auto-renewal.',
  'src/modules/mod_acme.h': 'mod_acme plugin declarations, ACME certificate state enums, and challenge management prototypes.',
  'src/modules/mod_quic.c': 'RFC 9000 QUIC Transport Protocol & RFC 9114 HTTP/3 Multiplexer. Implements Long/Short header parsing, Connection ID tracking, QPACK compression state, Alt-Svc header emission, and /quic-status telemetry.',
  'src/modules/mod_quic.h': 'mod_quic plugin declarations, QUIC packet types, HTTP/3 frame enums, and connection stats.',
  'src/modules/mod_wireguard.c': 'Zero-Trust Noise_IKpsk2 VPN & WireGuard Mesh Subsystem. Implements in-kernel cryptokey routing table for interface she-wg0 on port 51820 with dynamic peer management API.',
  'src/modules/mod_wireguard.h': 'mod_wireguard plugin declarations, WireGuard message types, peer struct, and mesh telemetry.',
  'src/modules/mod_grpc.c': 'gRPC over HTTP/2 & Protocol Buffers Binary Transcoder Gateway. Encodes/decodes 5-byte length-prefixed binary message framing, handles trailing grpc-status headers, and provides /grpc/transcode.',
  'src/modules/mod_grpc.h': 'mod_grpc plugin declarations, gRPC status codes, frame structs, and transcoding prototypes.',
  'src/modules/mod_ai_gateway.c': 'Multi-Tenant AI Gateway & LLM Streaming Proxy. Implements OpenAI-compatible /v1/chat/completions, Server-Sent Events (SSE) chunk streaming, tenant token billing quotas, and /ai-gateway-status TTFT latency metrics.',
  'src/modules/mod_ai_gateway.h': 'mod_ai_gateway plugin declarations, tenant structures, chat completion framing, and token accounting prototypes.',
  'src/modules/mod_raft.c': 'Raft Distributed Consensus State Machine. Implements leader election, term management, in-memory replicated log entries (/raft/propose), and cluster quorum peer synchronization (/raft-status).',
  'src/modules/mod_raft.h': 'mod_raft plugin declarations, Raft state enums (Leader, Follower, Candidate), log entry structs, and consensus RPC handlers.',
  'src/modules/mod_otel.c': 'OpenTelemetry Distributed Tracing & W3C Trace Context engine. Parses/injects traceparent headers, records microsecond spans into a high-speed ring buffer, and exposes /otel-status telemetry.',
  'src/modules/mod_otel.h': 'mod_otel plugin declarations, W3C traceparent structures, span recorder models, and OTLP exporter prototypes.',
  'src/event/she_fdevent.c': 'Linux epoll abstraction layer. Implements epoll_create1, epoll_ctl, and epoll_wait event multiplexing loop.',
  'src/event/she_fdevent.h': 'she_fdevent_system_t definition, SHE_FDEVENT_IN, SHE_FDEVENT_OUT, and callback prototypes.',
  'src/net/she_net.c': 'Low-level socket creation, non-blocking fcntl(O_NONBLOCK), TCP_NODELAY, SO_REUSEADDR, and accept handling.',
  'src/net/she_net.h': 'Network helper function prototypes.',
  'src/http/she_http_parser.c': 'HTTP/1.1 request line and header parser with strict path traversal sanitization, URL decoding, and Range header support.',
  'src/http/she_http_parser.h': 'she_http_request_t struct, HTTP method enums, and header key-value lookup structures.',
  'src/http/she_mime.c': 'Fast static extension-to-MIME-type mapping table (HTML, CSS, JS, JSON, images, fonts, WASM, conf).',
  'src/http/she_mime.h': 'MIME lookup declaration.',
  'src/main.c': 'SHE entry point, CLI argument parser (-c, -p, -d, -k, -m, -v), signal handlers, and ASCII banner.',
  'she.conf': 'SHE server runtime configuration file. Directives for server, security rate limiting, auth, and virtual hosts.',
  'Makefile': 'Modular GCC build recipe targeting C11, -Wall, -Wextra, -O2, and subfolder compilation.',
};

const FOLDER_LABELS: Record<string, { label: string; desc: string }> = {
  core: { label: 'src/core/', desc: 'State Machine & Memory' },
  security: { label: 'src/security/', desc: 'Rate Limiter & Hardening' },
  modules: { label: 'src/modules/', desc: 'Plugins (auth, vhost, fastcgi)' },
  event: { label: 'src/event/', desc: 'epoll(7) Multiplexer' },
  net: { label: 'src/net/', desc: 'TCP & Socket Layer' },
  http: { label: 'src/http/', desc: 'Parser & MIME Mapping' },
  main: { label: 'src/', desc: 'Server Entry Point' },
  config: { label: 'config/', desc: 'Runtime she.conf' },
  build: { label: './', desc: 'Build Configuration' },
};

export const CodeExplorer: React.FC = () => {
  const [files, setFiles] = useState<FileData[]>([]);
  const [selectedFilePath, setSelectedFilePath] = useState<string>('src/security/she_security.c');
  const [searchTerm, setSearchTerm] = useState<string>('');
  const [copied, setCopied] = useState<boolean>(false);
  const [loading, setLoading] = useState<boolean>(true);

  useEffect(() => {
    fetch('/api/c-server/files')
      .then(async (res) => {
        if (!res.ok) return null;
        const contentType = res.headers.get('content-type') || '';
        if (!contentType.includes('application/json')) return null;
        return res.json();
      })
      .then((data) => {
        if (data && data.files && data.files.length > 0) {
          setFiles(data.files);
        }
        setLoading(false);
      })
      .catch(() => {
        setLoading(false);
      });
  }, []);

  const selectedFile = files.find((f) => f.path === selectedFilePath) || files[0];

  const handleCopy = () => {
    if (!selectedFile) return;
    navigator.clipboard.writeText(selectedFile.content);
    setCopied(true);
    setTimeout(() => setCopied(false), 2000);
  };

  const filteredFiles = files.filter(
    (f) =>
      f.name.toLowerCase().includes(searchTerm.toLowerCase()) ||
      f.path.toLowerCase().includes(searchTerm.toLowerCase())
  );

  const groupedFiles: Record<string, FileData[]> = {};
  filteredFiles.forEach((f) => {
    const key = f.folder || 'other';
    if (!groupedFiles[key]) groupedFiles[key] = [];
    groupedFiles[key].push(f);
  });

  const fileLines = selectedFile ? selectedFile.content.split('\n') : [];

  return (
    <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8 py-8 space-y-6">
      {/* Header bar */}
      <div className="flex flex-col sm:flex-row items-start sm:items-center justify-between gap-4">
        <div>
          <h2 className="text-xl font-bold text-white tracking-tight">SHE (Secure HTTP Engine) C Source Repository</h2>
          <p className="text-xs text-slate-400">
            Organized with standard <code className="text-emerald-300 font-mono">she_</code> keyword prefixes into subsystems:
            (<code className="text-slate-300">core/</code>, <code className="text-slate-300">security/</code>,{' '}
            <code className="text-slate-300">modules/</code>, <code className="text-slate-300">event/</code>,{' '}
            <code className="text-slate-300">net/</code>, <code className="text-slate-300">http/</code>).
          </p>
        </div>

        <div className="flex items-center gap-3">
          <button
            onClick={handleCopy}
            className="flex items-center gap-1.5 px-3 py-1.5 text-xs font-medium text-slate-300 bg-slate-900 hover:bg-slate-800 border border-slate-800 rounded-md transition-colors"
          >
            {copied ? <Check className="w-3.5 h-3.5 text-emerald-400" /> : <Copy className="w-3.5 h-3.5 text-slate-400" />}
            <span>{copied ? 'Copied' : 'Copy File'}</span>
          </button>
          <a
            href="/api/c-server/download"
            className="flex items-center gap-1.5 px-3.5 py-1.5 text-xs font-medium text-white bg-emerald-600 hover:bg-emerald-500 rounded-md transition-colors shadow-sm"
          >
            <Download className="w-3.5 h-3.5" />
            <span>Download C Code (.tar.gz)</span>
          </a>
        </div>
      </div>

      {/* Main File Explorer Container */}
      <div className="grid grid-cols-1 lg:grid-cols-12 gap-6 items-start">
        {/* Left Sidebar: Hierarchical File Tree (4 cols) */}
        <div className="lg:col-span-4 bg-slate-900/70 border border-slate-800 rounded-xl p-4 space-y-4">
          <div className="relative">
            <Search className="w-3.5 h-3.5 text-slate-500 absolute left-2.5 top-2.5" />
            <input
              type="text"
              placeholder="Search by path or filename..."
              value={searchTerm}
              onChange={(e) => setSearchTerm(e.target.value)}
              className="w-full bg-slate-950 border border-slate-800 rounded-md pl-8 pr-3 py-1.5 text-xs text-slate-200 placeholder-slate-500 focus:outline-none focus:border-emerald-500"
            />
          </div>

          <div className="space-y-4 max-h-[600px] overflow-y-auto pr-1">
            {Object.entries(groupedFiles).map(([folderKey, items]) => {
              const meta = FOLDER_LABELS[folderKey] || { label: folderKey, desc: '' };
              return (
                <div key={folderKey} className="space-y-1">
                  <div className="flex items-center justify-between px-2 py-1 text-[11px] font-mono text-slate-400">
                    <div className="flex items-center gap-1.5 font-semibold text-slate-300">
                      <Folder className="w-3.5 h-3.5 text-emerald-400" />
                      <span>{meta.label}</span>
                    </div>
                    <span className="text-[10px] text-slate-500">{meta.desc}</span>
                  </div>

                  <div className="space-y-0.5 pl-2 border-l border-slate-800/80 ml-2">
                    {items.map((f) => {
                      const isSelected = f.path === selectedFilePath;
                      const isHeader = f.name.endsWith('.h');
                      const isMakefile = f.name === 'Makefile';
                      const isConf = f.name.endsWith('.conf');
                      const isSecurity = f.folder === 'security' || f.name.includes('security') || f.name.includes('auth');

                      return (
                        <button
                          key={f.path}
                          onClick={() => setSelectedFilePath(f.path)}
                          className={`w-full flex items-center justify-between px-2.5 py-1.5 rounded text-xs font-mono transition-colors text-left ${
                            isSelected
                              ? 'bg-emerald-950/60 text-emerald-400 border border-emerald-800/60 font-semibold'
                              : 'text-slate-400 hover:text-slate-200 hover:bg-slate-800/40 border border-transparent'
                          }`}
                        >
                          <div className="flex items-center gap-2 truncate">
                            <FileCode
                              className={`w-3.5 h-3.5 shrink-0 ${
                                isSecurity ? 'text-emerald-400' : isHeader ? 'text-amber-400' : isMakefile || isConf ? 'text-purple-400' : 'text-sky-400'
                              }`}
                            />
                            <span className="truncate">{f.name}</span>
                          </div>
                          <span className="text-[10px] text-slate-500 tabular-nums shrink-0">
                            {f.lines}L
                          </span>
                        </button>
                      );
                    })}
                  </div>
                </div>
              );
            })}
          </div>

          <div className="pt-3 border-t border-slate-800/80 text-[11px] text-slate-500 font-mono flex items-center justify-between">
            <span>{files.length} Source Files</span>
            <span>Total: {files.reduce((acc, f) => acc + f.lines, 0)} lines</span>
          </div>
        </div>

        {/* Right Editor View: Code Content (8 cols) */}
        <div className="lg:col-span-8 bg-slate-950 border border-slate-800 rounded-xl overflow-hidden flex flex-col">
          {/* Top meta strip */}
          <div className="flex flex-col sm:flex-row items-start sm:items-center justify-between gap-3 px-5 py-3 bg-slate-900 border-b border-slate-800">
            <div className="flex items-center gap-3">
              <span className="text-sm font-bold text-white font-mono">
                {selectedFile?.path || selectedFilePath}
              </span>
              <div className="flex items-center gap-2 text-xs text-slate-400 font-mono">
                <span>{selectedFile?.lines || 0} lines</span>
                <span aria-hidden="true">·</span>
                <span>{((selectedFile?.size || 0) / 1024).toFixed(1)} KB</span>
              </div>
            </div>

            <div className="flex items-center gap-2 text-[11px] text-slate-400 font-mono bg-slate-950 px-2.5 py-1 rounded border border-slate-800">
              <Terminal className="w-3.5 h-3.5 text-emerald-400" />
              <span>SHE (C11 / Linux epoll)</span>
            </div>
          </div>

          {/* Architectural note banner */}
          {(FILE_NOTES[selectedFile?.path || ''] || FILE_NOTES[selectedFile?.name || '']) && (
            <div className="px-5 py-2.5 bg-slate-900/40 border-b border-slate-800/60 text-xs text-slate-300 flex items-start gap-2.5">
              <Info className="w-4 h-4 text-emerald-400 shrink-0 mt-0.5" />
              <p className="leading-relaxed">
                {FILE_NOTES[selectedFile?.path || ''] || FILE_NOTES[selectedFile?.name || '']}
              </p>
            </div>
          )}

          {/* Code Viewer */}
          <div className="overflow-x-auto max-h-[640px] overflow-y-auto p-4 font-mono text-xs leading-relaxed bg-slate-950 text-slate-300">
            {loading ? (
              <div className="py-20 text-center text-slate-500">Loading C source files...</div>
            ) : (
              <table className="w-full border-collapse">
                <tbody>
                  {fileLines.map((line, idx) => (
                    <tr key={idx} className="hover:bg-slate-900/60 group">
                      <td className="w-12 pr-4 text-right select-none text-slate-600 group-hover:text-slate-400 tabular-nums text-[11px]">
                        {idx + 1}
                      </td>
                      <td className="whitespace-pre text-slate-300 font-mono">
                        {line || ' '}
                      </td>
                    </tr>
                  ))}
                </tbody>
              </table>
            )}
          </div>
        </div>
      </div>
    </div>
  );
};
