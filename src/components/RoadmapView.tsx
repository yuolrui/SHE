import React, { useState } from 'react';
import { CheckCircle2, Clock, ArrowRight, Code2, Layers, Cpu, ShieldCheck, Zap, Lock, Globe } from 'lucide-react';

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
    title: 'Core Engine & Non-Blocking State Machine',
    status: 'completed',
    description: 'Foundation inspired by Lighttpd 1.4: Single-process event-driven architecture, Linux epoll multiplexing, 10-state connection FSM, and zero-copy sendfile(2).',
    features: [
      'Linux epoll(7) multiplexing via she_fdevent abstraction layer',
      '10-state Connection Finite State Machine (SHE_CON_STATE_CONNECT to CLOSE)',
      'Zero-copy file transmission via Linux sendfile(2) in she_chunkqueue',
      'HTTP/1.1 Request Line & Header parser with Range header support',
      'Path traversal security guard (normalizing /../ and %2e%2e)',
      'HTTP 206 Partial Content byte-range slicing',
      'HTTP/1.1 persistent Keep-Alive connection loop & timeout reaper',
      'Static MIME-type mapping table with extension detection',
    ],
    codeHighlight: `/* Verified & Active in c_server/ */
int she_connection_state_machine(she_server_t *srv, she_connection_t *con);
ssize_t she_chunkqueue_write_chunk_to_socket(she_chunkqueue_t *cq, int socket_fd);
int she_fdevent_poll(she_fdevent_system_t *ev, int timeout_ms);`,
  },
  {
    phase: 'Phase 2',
    title: 'Lighttpd Modular Plugin Subsystem',
    status: 'completed',
    description: 'Decoupled plugin subsystem with she_plugin_t interface and lifecycle hooks: mod_status (/server-status), mod_indexfile cascade, mod_dirlisting HTML indexer, and mod_accesslog (CLF format).',
    features: [
      'she_plugin_t interface struct with standard hook callbacks',
      'mod_indexfile: configurable index fallback cascade (index.html, index.htm, default.html)',
      'mod_dirlisting: automatic responsive HTML directory indexing with size and mtime columns',
      'mod_accesslog: Apache/Lighttpd Combined Log Format (CLF) request logging to logs/access.log',
      'mod_status: internal /server-status metrics endpoint showing active connections and RPS',
    ],
    codeHighlight: `/* Verified & Active in src/modules/ */
typedef struct she_plugin she_plugin_t;
struct she_plugin {
    size_t version;
    const char *name;
    she_handler_t (*init)(she_server_t *srv, she_plugin_t *p);
    she_handler_t (*cleanup)(she_server_t *srv, she_plugin_t *p);
    she_handler_t (*handle_uri_clean)(she_server_t *srv, she_connection_t *con, void *p_data);
    she_handler_t (*handle_subrequest_start)(she_server_t *srv, she_connection_t *con, void *p_data);
    she_handler_t (*handle_response_done)(she_server_t *srv, she_connection_t *con, void *p_data);
};`,
  },
  {
    phase: 'Phase 3',
    title: 'SHE Security Hardening, Basic Auth, VHost & FastCGI',
    status: 'completed',
    description: 'Rebranded project to SHE (Secure HTTP Engine) with she_ keyword prefix across all subsystems. Implemented token-bucket rate limiter, RFC 7617 Basic Authentication, virtual hosts, and FastCGI protocol framing.',
    features: [
      'she_ keyword prefix across types (she_server_t, she_connection_t, she_buffer_t)',
      'she_security: token-bucket per-IP rate limiter returning 429 Too Many Requests',
      'Injected defense headers: nosniff, SAMEORIGIN, XSS-Protection, Referrer-Policy',
      'mod_auth: RFC 7617 HTTP Basic Authentication with base64 decoding for /secure/',
      'mod_vhost: Virtual Host routing matching Host headers (docs.local, api.local)',
      'mod_fastcgi: FastCGI 1.0 binary upstream client framing (FCGI_BEGIN_REQUEST, FCGI_PARAMS)',
      'Runtime configuration file parser: she.conf support via -c argument',
    ],
    codeHighlight: `/* Verified & Active in c_server/ */
int she_security_check_rate_limit(she_security_t *sec, const char *client_ip);
int she_security_append_headers(she_security_t *sec, she_connection_t *con);
she_plugin_t *mod_auth_plugin_init(void);
she_plugin_t *mod_vhost_plugin_init(void);
she_plugin_t *mod_fastcgi_plugin_init(void);
int she_config_load_file(she_server_config_t *cfg, const char *filepath);`,
  },
  {
    phase: 'Phase 4',
    title: 'TLS 1.3 Termination, Compression & HTTP/2',
    status: 'planned',
    description: 'Enterprise deployment capabilities including TLS termination, on-the-fly streaming compression, and HTTP/2 multiplexing.',
    features: [
      'mod_openssl: TLS 1.3 / 1.2 termination using OpenSSL or mbedTLS with ALPN',
      'mod_compress: dynamic on-the-fly chunked Gzip & Brotli HTTP transfer encoding',
      'mod_rewrite / mod_redirect: regex-based URI routing and 301/302 rewrites',
      'Multi-worker process model with SO_REUSEPORT kernel load balancing',
      'HTTP/2 binary framing and HPACK header compression engine',
    ],
    codeHighlight: `/* Phase 4 Blueprint: TLS Socket Abstraction */
typedef struct {
    int (*read)(she_connection_t *con, void *buf, size_t count);
    int (*write)(she_connection_t *con, const void *buf, size_t count);
    void *ssl_handle;
} she_network_backend_t;`,
  },
];

export const RoadmapView: React.FC = () => {
  const [selectedMilestone, setSelectedMilestone] = useState<Milestone>(MILESTONES[2]);

  return (
    <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8 py-8 space-y-8">
      {/* Overview header */}
      <div className="bg-slate-900/60 border border-slate-800 rounded-xl p-6">
        <div className="flex items-center gap-2 text-xs text-slate-400 font-mono mb-2">
          <span className="text-emerald-400 font-bold">SHE Evolutionary Roadmap</span>
          <span aria-hidden="true">·</span>
          <span>Lighttpd Architecture & Security Parity</span>
          <span aria-hidden="true">·</span>
          <span>Phase 1, 2, and 3 Completed</span>
        </div>
        <h2 className="text-xl font-bold text-white tracking-tight">
          SHE (Secure HTTP Engine) Milestone Roadmap
        </h2>
        <p className="text-xs text-slate-300 leading-relaxed max-w-3xl mt-1">
          Evolution from a bare single-process epoll loop into a fully-featured, secure HTTP engine with standard{' '}
          <code className="text-emerald-300 font-mono">she_</code> prefix keywords, token-bucket rate limiting,
          RFC 7617 Basic Authentication, virtual hosting, and FastCGI protocol framing.
        </p>
      </div>

      {/* Milestones list */}
      <div className="grid grid-cols-1 md:grid-cols-4 gap-4">
        {MILESTONES.map((m) => {
          const isSelected = selectedMilestone.phase === m.phase;
          const isCompleted = m.status === 'completed';

          return (
            <button
              key={m.phase}
              onClick={() => setSelectedMilestone(m)}
              className={`flex flex-col text-left p-5 rounded-xl border transition-all ${
                isSelected
                  ? 'bg-slate-800 border-emerald-500 shadow-md ring-1 ring-emerald-500/30'
                  : 'bg-slate-900/60 border-slate-800 hover:border-slate-700 hover:bg-slate-800/40'
              }`}
            >
              <div className="flex items-center justify-between w-full mb-2">
                <span className="text-xs font-mono font-bold text-emerald-400">{m.phase}</span>
                <span
                  className={`text-[10px] font-mono px-2 py-0.5 rounded font-semibold ${
                    isCompleted
                      ? 'bg-emerald-950/80 text-emerald-400 border border-emerald-800/80'
                      : 'bg-slate-800 text-slate-400'
                  }`}
                >
                  {isCompleted ? 'Completed' : 'Planned'}
                </span>
              </div>
              <h3 className="text-sm font-bold text-white mb-1.5">{m.title}</h3>
              <p className="text-xs text-slate-400 line-clamp-2">{m.description}</p>
            </button>
          );
        })}
      </div>

      {/* Selected Milestone Deep-Dive */}
      <div className="bg-slate-900/70 border border-slate-800 rounded-xl p-6 space-y-6">
        <div className="flex flex-col sm:flex-row items-start sm:items-center justify-between gap-4 border-b border-slate-800 pb-4">
          <div>
            <div className="flex items-center gap-2 text-xs font-mono text-emerald-400 mb-1">
              <span>{selectedMilestone.phase}</span>
              <span aria-hidden="true">·</span>
              <span className="uppercase">{selectedMilestone.status}</span>
            </div>
            <h3 className="text-xl font-bold text-white">{selectedMilestone.title}</h3>
          </div>

          <div className="flex items-center gap-2">
            <span
              className={`text-xs font-mono px-3 py-1 rounded font-bold ${
                selectedMilestone.status === 'completed'
                  ? 'bg-emerald-950 text-emerald-400 border border-emerald-800'
                  : 'bg-slate-800 text-slate-400'
              }`}
            >
              {selectedMilestone.status === 'completed' ? 'Verified in Codebase' : 'Next Roadmap Step'}
            </span>
          </div>
        </div>

        <p className="text-xs text-slate-300 leading-relaxed max-w-4xl">
          {selectedMilestone.description}
        </p>

        {/* Feature checklist */}
        <div className="space-y-3">
          <h4 className="text-xs font-semibold text-slate-200 uppercase tracking-wider">
            Subsystems & Capabilities Included
          </h4>
          <div className="grid grid-cols-1 sm:grid-cols-2 gap-2.5">
            {selectedMilestone.features.map((feat, idx) => (
              <div
                key={idx}
                className="flex items-start gap-2.5 p-3 rounded-lg bg-slate-950/60 border border-slate-800/80 text-xs text-slate-300"
              >
                <CheckCircle2 className="w-4 h-4 text-emerald-400 shrink-0 mt-0.5" />
                <span className="leading-relaxed">{feat}</span>
              </div>
            ))}
          </div>
        </div>

        {/* C Code Highlight */}
        <div className="space-y-2">
          <div className="flex items-center justify-between">
            <h4 className="text-xs font-semibold text-slate-200 uppercase tracking-wider flex items-center gap-2">
              <Code2 className="w-4 h-4 text-emerald-400" />
              <span>Architectural C Definition</span>
            </h4>
            <span className="text-[11px] font-mono text-slate-400">Pure C11</span>
          </div>
          <div className="p-4 rounded-lg bg-slate-950 border border-slate-800 font-mono text-xs text-emerald-300 overflow-x-auto">
            <pre>
              <code>{selectedMilestone.codeHighlight}</code>
            </pre>
          </div>
        </div>
      </div>
    </div>
  );
};
