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
    title: 'Dynamic Compression, URL Rewriting & Reverse Proxy',
    status: 'completed',
    description: 'Enterprise edge routing & bandwidth optimization: RFC 1952 dynamic Gzip compression with zlib, transparent URL rewriting and 301/302 redirects, Reverse Proxy Gateway with X-Forwarded-* headers, and multi-worker SO_REUSEPORT socket balancing.',
    features: [
      'mod_deflate: On-the-fly RFC 1952 Gzip compression via zlib with Content-Encoding: gzip',
      'mod_rewrite: URL rewriting and HTTP 301/302 redirection with Location header injection',
      'mod_proxy: Reverse Proxy Gateway forwarding X-Forwarded-For, X-Forwarded-Host, and X-Forwarded-Proto',
      'Multi-worker process model with Linux kernel SO_REUSEPORT socket load balancing (-w <workers>)',
      'she.conf runtime configuration directives for deflate, rewrite, redirect, and proxy',
      'Telemetry tracking for requests_compressed, bytes_compressed_saved, and requests_proxied',
    ],
    codeHighlight: `/* Verified & Active in Phase 4 */
int she_deflate_compress_gzip(const char *in_data, size_t in_len, char **out_data, size_t *out_len);
she_plugin_t *mod_deflate_plugin_init(void);
she_plugin_t *mod_rewrite_plugin_init(void);
she_plugin_t *mod_proxy_plugin_init(void);`,
  },
  {
    phase: 'Phase 5',
    title: 'TLS 1.3 Termination, WebSocket Protocol & HTTP/2 Multiplexing',
    status: 'completed',
    description: 'Next generation transport security and protocol modernization: Native OpenSSL 3.0 TLS 1.3 / 1.2 termination with ALPN negotiation ("h2", "http/1.1"), RFC 6455 WebSocket handshake engine with SHA-1 + Base64 accept computation, and RFC 9113 HTTP/2 24-byte preface verification and 9-byte binary frame header multiplexer.',
    features: [
      'RFC 6455 WebSocket Handshake Engine with SHA-1 + Base64 Sec-WebSocket-Accept computation',
      'WebSocket 101 Switching Protocols upgrade handler & runtime opcode telemetry (/ws)',
      'OpenSSL 3.0 TLS 1.3 / 1.2 Transport Security Context (SSL_CTX) & modern ciphersuites',
      'TLS ALPN (Application-Layer Protocol Negotiation) callback advertising "h2" and "http/1.1"',
      'RFC 9113 HTTP/2 24-byte client connection preface verification (PRI * HTTP/2.0...)',
      'HTTP/2 9-byte binary frame header engine (she_h2_frame_header_t) & SETTINGS negotiation',
      'Modular configuration directives in she.conf for websocket, ssl, and http2',
      'Live endpoints: /ws (handshake/telemetry), /ssl-status, and /h2-status',
    ],
    codeHighlight: `/* Verified & Active in Phase 5 */
int she_websocket_compute_accept(const char *client_key, char *out_accept, size_t out_len);
she_plugin_t *mod_websocket_plugin_init(void);
she_plugin_t *mod_ssl_plugin_init(void);
she_plugin_t *mod_http2_plugin_init(void);`,
  },
  {
    phase: 'Phase 6',
    title: 'WASM Edge Function Execution & eBPF Kernel Acceleration',
    status: 'completed',
    description: 'Enterprise serverless computing, kernel-level packet filtering and automated PKI: Embedded WebAssembly (WASM 1.0) sandbox runtime for dynamic edge microservices, eBPF/XDP socket filtering for DDoS mitigation with in-kernel BPF maps, and RFC 8555 ACME automated Let\'s Encrypt certificate management.',
    features: [
      'mod_wasm: WebAssembly 1.0 embedded sandbox runtime for serverless edge handlers',
      'Edge function microservices: edge_hash, edge_fibonacci, edge_auth_check, edge_filter',
      'Isolated 64KB linear memory page sandboxing with microsecond execution latency',
      'mod_ebpf: Linux eBPF / XDP in-kernel network driver packet filter & DDoS shield',
      'Dynamic kernel BPF hash maps for real-time attacker IP blacklisting & SYN flood defense',
      'mod_acme: RFC 8555 ACME automated certificate management (Let\'s Encrypt / ZeroSSL)',
      'Automated http-01 challenge responder (/.well-known/acme-challenge/) & auto-renewal',
      'Interactive telemetry endpoints: /wasm-status, /ebpf-status, /acme-status, and /edge/invoke',
    ],
    codeHighlight: `/* Verified & Active in Phase 6 */
int she_wasm_execute_function(const char *func_name, const char *arg, char *out_buf, size_t out_max, uint64_t *out_duration_us);
int she_ebpf_inspect_packet(const char *src_ip, uint16_t src_port, int is_syn);
int she_acme_set_http01_challenge(const char *token, const char *key_auth, const char *domain);
she_plugin_t *mod_wasm_plugin_init(void);
she_plugin_t *mod_ebpf_plugin_init(void);
she_plugin_t *mod_acme_plugin_init(void);`,
  },
  {
    phase: 'Phase 7',
    title: 'QUIC / HTTP/3 UDP Engine & Zero-Trust WireGuard Mesh',
    status: 'planned',
    description: 'Next-generation UDP transport and zero-trust mesh networking: Native epoll UDP listener with recvmmsg(2) batching, RFC 9000 QUIC packet framing with connection migration, QPACK dynamic header compression, and embedded WireGuard noise protocol VPN mesh tunnel.',
    features: [
      'Native Linux epoll UDP socket engine with recvmmsg(2) kernel batch I/O',
      'RFC 9000 QUIC transport protocol layer with connection ID routing',
      'RFC 9204 QPACK dynamic & static table header compression',
      'Zero-RTT connection resumption with TLS 1.3 0-RTT early data',
      'Embedded WireGuard Noise Protocol zero-trust site-to-site mesh overlay',
      'UDP connection migration resilience across network interface changes',
    ],
    codeHighlight: `/* Phase 7 Blueprint: QUIC & UDP Batch Engine */
typedef struct {
    int udp_fd;
    uint64_t connection_id;
    int (*quic_recv_packet)(she_connection_t *con, struct mmsghdr *msgs, unsigned int vlen);
    int (*qpack_decode_header)(she_connection_t *con, const uint8_t *in, size_t in_len);
} she_quic_engine_t;`,
  },
];

export const RoadmapView: React.FC = () => {
  const [selectedMilestone, setSelectedMilestone] = useState<Milestone>(MILESTONES[5]);

  return (
    <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8 py-8 space-y-8">
      {/* Overview header */}
      <div className="bg-slate-900/60 border border-slate-800 rounded-xl p-6">
        <div className="flex items-center gap-2 text-xs text-slate-400 font-mono mb-2">
          <span className="text-emerald-400 font-bold">SHE Evolutionary Roadmap</span>
          <span aria-hidden="true">·</span>
          <span>Lighttpd Architecture & Security Parity</span>
          <span aria-hidden="true">·</span>
          <span>Phase 1, 2, 3, 4, 5, and 6 Completed</span>
        </div>
        <h2 className="text-xl font-bold text-white tracking-tight">
          SHE (Secure HTTP Engine) Milestone Roadmap
        </h2>
        <p className="text-xs text-slate-300 leading-relaxed max-w-3xl mt-1">
          Evolution from a bare single-process epoll loop into an enterprise secure HTTP engine with standard{' '}
          <code className="text-emerald-300 font-mono">she_</code> prefix keywords, token-bucket rate limiting,
          RFC 7617 Basic Authentication, virtual hosting, FastCGI framing, dynamic Gzip deflate, RFC 6455 WebSocket, OpenSSL TLS 1.3, RFC 9113 HTTP/2,
          WASM 1.0 Serverless Edge, eBPF/XDP Kernel Shield, and RFC 8555 ACME automated certificates.
        </p>
      </div>

      {/* Milestones list */}
      <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-7 gap-2.5">
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
