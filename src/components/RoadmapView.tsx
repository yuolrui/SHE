import React, { useState } from 'react';
import { CheckCircle2, Clock, ArrowRight, Code2, Layers, Cpu, Shield, Zap } from 'lucide-react';

interface Milestone {
  phase: string;
  title: string;
  status: 'completed' | 'in-progress' | 'planned';
  description: string;
  features: string[];
  codeHighlight: string;
}

const MILESTONES: Milestone[] = [
  {
    phase: 'Phase 1',
    title: 'Core Engine & Non-Blocking State Machine (Current Milestone)',
    status: 'completed',
    description: 'Foundation inspired directly by Lighttpd 1.4: Single-process event-driven architecture, Linux epoll multiplexing, 10-state connection FSM, and zero-copy sendfile(2).',
    features: [
      'Linux epoll(7) multiplexing via fdevent abstraction layer',
      '10-state Connection State Machine (CON_STATE_CONNECT to CON_STATE_CLOSE)',
      'Zero-copy file transmission via Linux sendfile(2) in chunkqueue',
      'HTTP/1.1 Request Line & Header parser with Range header support',
      'Path traversal security guard (normalizing /../ and %2e%2e)',
      'HTTP 206 Partial Content byte-range slicing',
      'HTTP/1.1 persistent Keep-Alive connection loop & timeout reaper',
      'Static MIME-type mapping table with extension detection',
    ],
    codeHighlight: `/* Verified & Active in c_server/ */
int connection_state_machine(server_t *srv, connection_t *con);
ssize_t chunkqueue_write_chunk_to_socket(chunkqueue_t *cq, int socket_fd);
int fdevent_poll(fdevent_system_t *ev, int timeout_ms);`,
  },
  {
    phase: 'Phase 2',
    title: 'Lighttpd Modular Plugin System',
    status: 'completed',
    description: 'Decoupled plugin subsystem with plugin_t interface and lifecycle hooks: mod_status (/server-status), mod_indexfile cascade, mod_dirlisting HTML indexer, and mod_accesslog (CLF format).',
    features: [
      'plugin_t interface struct with standard Lighttpd hook callbacks',
      'mod_indexfile: configurable index fallback cascade (index.html, index.htm, default.html)',
      'mod_dirlisting: automatic responsive HTML directory indexing with size and mtime columns',
      'mod_accesslog: Apache/Lighttpd Combined Log Format (CLF) request logging to logs/access.log',
      'mod_status: internal /server-status metrics endpoint showing active connections and RPS',
    ],
    codeHighlight: `/* Verified & Active in src/modules/ */
typedef struct plugin plugin_t;
struct plugin {
    size_t version;
    const char *name;
    handler_t (*init)(server_t *srv, plugin_t *p);
    handler_t (*cleanup)(server_t *srv, plugin_t *p);
    handler_t (*handle_uri_clean)(server_t *srv, connection_t *con, void *p_data);
    handler_t (*handle_subrequest_start)(server_t *srv, connection_t *con, void *p_data);
    handler_t (*handle_response_done)(server_t *srv, connection_t *con, void *p_data);
};`,
  },
  {
    phase: 'Phase 3',
    title: 'Dynamic Gateways & Configuration Lexer',
    status: 'in-progress',
    description: 'Support dynamic web applications via FastCGI (PHP-FPM, Python, Node) and implement lighttpd.conf nested conditional configuration.',
    features: [
      'mod_fastcgi: binary FastCGI protocol client communicating over UNIX domain sockets or TCP',
      'Non-blocking upstream proxying using fdevent event loop',
      'lighttpd.conf parser supporting nested conditionals ($HTTP["host"] == "...")',
      'mod_simple_vhost: multi-tenant Virtual Hosting with per-host document roots',
      'mod_auth: HTTP Basic Authentication with htpasswd file parsing',
    ],
    codeHighlight: `/* Phase 3 Blueprint: FastCGI Record Header */
typedef struct {
    unsigned char version;
    unsigned char type;          /* FCGI_BEGIN_REQUEST, FCGI_PARAMS, FCGI_STDIN */
    unsigned char requestIdB1;
    unsigned char requestIdB0;
    unsigned char contentLengthB1;
    unsigned char contentLengthB0;
    unsigned char paddingLength;
    unsigned char reserved;
} fcgi_header_t;`,
  },
  {
    phase: 'Phase 4',
    title: 'Production Hardening, TLS & Protocol Evolution',
    status: 'planned',
    description: 'Enterprise deployment capabilities including TLS termination, on-the-fly streaming compression, and HTTP/2 framing.',
    features: [
      'mod_openssl: TLS 1.3 / 1.2 termination using OpenSSL or mbedTLS with ALPN',
      'mod_compress: dynamic on-the-fly chunked Gzip & Brotli HTTP transfer encoding',
      'mod_rewrite / mod_redirect: regex-based URI routing and 301/302 rewrites',
      'Multi-worker process model with SO_REUSEPORT kernel load balancing',
      'HTTP/2 binary framing and HPACK header compression engine',
    ],
    codeHighlight: `/* Phase 4 Blueprint: TLS Socket Abstraction */
typedef struct {
    int (*read)(connection_t *con, void *buf, size_t count);
    int (*write)(connection_t *con, const void *buf, size_t count);
    SSL *ssl_handle;
} network_backend_t;`,
  },
];

export const RoadmapView: React.FC = () => {
  const [selectedMilestone, setSelectedMilestone] = useState<Milestone>(MILESTONES[0]);

  return (
    <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8 py-8 space-y-8">
      {/* Overview header */}
      <div className="bg-slate-900/60 border border-slate-800 rounded-xl p-6">
        <div className="flex items-center gap-2 text-xs text-slate-400 font-mono mb-2">
          <span>Iterative Engineering Roadmap</span>
          <span aria-hidden="true">·</span>
          <span>Lighttpd Architecture Equivalence</span>
          <span aria-hidden="true">·</span>
          <span>Step-by-Step Milestones</span>
        </div>
        <h2 className="text-xl font-bold text-white tracking-tight">
          Server Evolution: From Foundation to Lighttpd Parity
        </h2>
        <p className="text-xs text-slate-300 leading-relaxed max-w-3xl mt-1">
          As requested (&quot;可以一步一步完善。先实现基础功能。&quot;), the project begins with Phase 1: the core
          high-performance non-blocking event engine, 10-state machine, and zero-copy chunkqueue. Below is the
          step-by-step roadmap to achieve complete equivalence with lighttpd.
        </p>
      </div>

      {/* Milestones list */}
      <div className="grid grid-cols-1 md:grid-cols-4 gap-4">
        {MILESTONES.map((m) => {
          const isSelected = selectedMilestone.phase === m.phase;
          return (
            <button
              key={m.phase}
              onClick={() => setSelectedMilestone(m)}
              className={`flex flex-col text-left p-5 rounded-xl border transition-all ${
                isSelected
                  ? 'bg-slate-800 border-sky-500 shadow-md ring-1 ring-sky-500/30'
                  : 'bg-slate-900/60 border-slate-800 hover:border-slate-700 hover:bg-slate-800/40'
              }`}
            >
              <div className="flex items-center justify-between w-full mb-3">
                <span className="text-xs font-mono font-bold text-sky-400">{m.phase}</span>
                {m.status === 'completed' ? (
                  <span className="flex items-center gap-1 text-[11px] text-emerald-400 font-mono">
                    <CheckCircle2 className="w-3.5 h-3.5" />
                    Verified
                  </span>
                ) : m.status === 'in-progress' ? (
                  <span className="flex items-center gap-1 text-[11px] text-amber-400 font-mono">
                    <Clock className="w-3.5 h-3.5" />
                    In Progress
                  </span>
                ) : (
                  <span className="text-[11px] text-slate-500 font-mono">Planned</span>
                )}
              </div>
              <h3 className="text-sm font-bold text-white tracking-tight line-clamp-2">{m.title}</h3>
              <p className="text-[11px] text-slate-400 mt-2 line-clamp-3 leading-relaxed">
                {m.description}
              </p>
            </button>
          );
        })}
      </div>

      {/* Selected Milestone Deep-Dive */}
      <div className="grid grid-cols-1 lg:grid-cols-12 gap-6 items-start">
        {/* Left: Features checklist (6 cols) */}
        <div className="lg:col-span-6 bg-slate-900/70 border border-slate-800 rounded-xl p-6 space-y-5">
          <div>
            <span className="text-xs font-mono text-sky-400">{selectedMilestone.phase} Specifications</span>
            <h3 className="text-lg font-bold text-white tracking-tight mt-0.5">{selectedMilestone.title}</h3>
            <p className="text-xs text-slate-300 mt-2 leading-relaxed">{selectedMilestone.description}</p>
          </div>

          <div className="space-y-3 pt-3 border-t border-slate-800">
            <h4 className="text-xs font-semibold text-slate-300 uppercase tracking-wider">Features & Capabilities</h4>
            <div className="space-y-2">
              {selectedMilestone.features.map((feat, i) => (
                <div key={i} className="flex items-start gap-2.5 text-xs text-slate-300">
                  <CheckCircle2
                    className={`w-4 h-4 shrink-0 mt-0.5 ${
                      selectedMilestone.status === 'completed' ? 'text-emerald-400' : 'text-slate-500'
                    }`}
                  />
                  <span>{feat}</span>
                </div>
              ))}
            </div>
          </div>
        </div>

        {/* Right: Code Blueprint & Architecture (6 cols) */}
        <div className="lg:col-span-6 bg-slate-950 border border-slate-800 rounded-xl overflow-hidden flex flex-col">
          <div className="flex items-center justify-between px-4 py-3 bg-slate-900 border-b border-slate-800">
            <div className="flex items-center gap-2 text-xs font-mono text-slate-300">
              <Code2 className="w-4 h-4 text-sky-400" />
              <span>Architectural Blueprint / Code Interface</span>
            </div>
            <span className="text-[11px] font-mono text-slate-400">{selectedMilestone.phase}</span>
          </div>

          <div className="p-4 font-mono text-xs text-slate-300 leading-relaxed overflow-x-auto bg-slate-950">
            <pre>
              <code>{selectedMilestone.codeHighlight}</code>
            </pre>
          </div>

          <div className="px-4 py-3 bg-slate-900/40 border-t border-slate-800 text-[11px] text-slate-400">
            <span>
              {selectedMilestone.status === 'completed'
                ? 'Phase 1 is fully compiled with GCC and running live in the environment.'
                : 'Interface designed for smooth drop-in expansion without breaking existing socket handlers.'}
            </span>
          </div>
        </div>
      </div>
    </div>
  );
};
