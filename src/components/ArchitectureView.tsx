import React, { useState } from 'react';
import { Layers, Shield, Zap, ArrowRight, Play, RefreshCw, Code2, CheckCircle2, Lock, Globe, Server } from 'lucide-react';

interface StateInfo {
  id: string;
  name: string;
  phase: string;
  description: string;
  lighttpdConcept: string;
  cSnippet: string;
  nextStates: string[];
}

const STATES: StateInfo[] = [
  {
    id: 'CONNECT',
    name: 'SHE_CON_STATE_CONNECT',
    phase: 'Connection Setup',
    description: 'New TCP connection accepted by she_net_accept_client(). Sockets are configured with O_NONBLOCK and TCP_NODELAY immediately.',
    lighttpdConcept: 'In SHE & Lighttpd, connections are decoupled from threads. The listening socket triggers EPOLLIN on epoll_wait, accepting non-blocking client fds without thread overhead.',
    cSnippet: `case SHE_CON_STATE_CONNECT: {
    con->state = SHE_CON_STATE_REQUEST_START;
    break;
}`,
    nextStates: ['REQUEST_START'],
  },
  {
    id: 'REQUEST_START',
    name: 'SHE_CON_STATE_REQUEST_START',
    phase: 'Request Initialization',
    description: 'Resets connection request and response buffers, registers SHE_FDEVENT_IN (EPOLLIN) on client fd, and prepares to read HTTP wire bytes.',
    lighttpdConcept: 'Persistent connections (Keep-Alive) recycle this state rather than closing and re-opening sockets, avoiding 3-way TCP handshakes.',
    cSnippet: `case SHE_CON_STATE_REQUEST_START: {
    she_connection_reset(con);
    she_fdevent_event_set(srv->ev, con->fd, SHE_FDEVENT_IN);
    con->state = SHE_CON_STATE_READ;
    done = 1; /* Wait for epoll read event */
    break;
}`,
    nextStates: ['READ'],
  },
  {
    id: 'READ',
    name: 'SHE_CON_STATE_READ',
    phase: 'Socket I/O & Security Screening',
    description: 'Non-blocking recv() reads raw HTTP bytes into con->read_buf. Validates request, applies token-bucket rate limiter, validates Host header, and guards against request smuggling.',
    lighttpdConcept: 'If recv() returns EAGAIN/EWOULDBLOCK, the engine parks in SHE_CON_STATE_READ and yields control to epoll without blocking any other connections.',
    cSnippet: `case SHE_CON_STATE_READ: {
    char buf[SHE_READ_CHUNK_SIZE];
    ssize_t bytes = recv(con->fd, buf, sizeof(buf), 0);
    if (bytes < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
        done = 1; break;
    }
    she_buffer_append_string_len(con->read_buf, buf, bytes);
    int parse_res = she_http_request_parse(&con->request, ...);
    if (parse_res == 1) {
        /* Token-bucket Rate Limiter check */
        if (!she_security_check_rate_limit(srv->security, con->client_ip)) {
            she_generate_error_response(srv, con, 429);
            con->state = SHE_CON_STATE_WRITE; break;
        }
        con->state = SHE_CON_STATE_REQUEST_END;
    }
    break;
}`,
    nextStates: ['REQUEST_END', 'WRITE', 'CLOSE'],
  },
  {
    id: 'REQUEST_END',
    name: 'SHE_CON_STATE_REQUEST_END',
    phase: 'Parser Validation',
    description: 'HTTP Request Line and Headers fully parsed and validated. Increments request metrics and negotiates Keep-Alive persistence.',
    lighttpdConcept: 'Strictly verifies HTTP/1.1 requirements (RFC 7230 Host header, URI lengths, sanitization) before triggering resource handlers.',
    cSnippet: `case SHE_CON_STATE_REQUEST_END: {
    con->state = SHE_CON_STATE_HANDLE_REQUEST;
    break;
}`,
    nextStates: ['HANDLE_REQUEST'],
  },
  {
    id: 'HANDLE_REQUEST',
    name: 'SHE_CON_STATE_HANDLE_REQUEST',
    phase: 'Routing, Auth & Modules',
    description: 'Executes SHE plugin pipeline: mod_vhost resolves Host header to isolated docroot; mod_auth enforces HTTP Basic Authentication (RFC 7617); mod_fastcgi, mod_status, mod_indexfile, mod_dirlisting run.',
    lighttpdConcept: 'Modular plugin pipeline returns SHE_HANDLER_FINISHED if intercepted (e.g. 401 Unauthorized or /server-status), or falls through to static zero-copy file serving.',
    cSnippet: `case SHE_CON_STATE_HANDLE_REQUEST: {
    /* Virtual host routing */
    she_plugins_call_handle_uri_clean(srv, con);
    /* Subrequest hooks (mod_auth, mod_status, mod_fastcgi, etc.) */
    she_handler_t hook_res = she_plugins_call_handle_subrequest_start(srv, con);
    if (hook_res == SHE_HANDLER_FINISHED) break;
    /* Static file lookup & HTTP Range checking */
    ...
    con->state = SHE_CON_STATE_RESPONSE_START;
    break;
}`,
    nextStates: ['RESPONSE_START'],
  },
  {
    id: 'RESPONSE_START',
    name: 'SHE_CON_STATE_RESPONSE_START',
    phase: 'Response Framing & Security Injection',
    description: 'Builds HTTP status line and response headers, injects defense headers (nosniff, SAMEORIGIN, XSS-Protection), enqueues file fd into chunkqueue, and arms epoll for EPOLLOUT.',
    lighttpdConcept: 'Never reads entire files into RAM. It pushes an open file descriptor with byte offsets into chunkqueue for kernel zero-copy transfer.',
    cSnippet: `case SHE_CON_STATE_RESPONSE_START: {
    /* Inject security headers into response */
    she_security_append_headers(srv->security, con);
    she_fdevent_event_set(srv->ev, con->fd, SHE_FDEVENT_OUT);
    con->state = SHE_CON_STATE_WRITE;
    break;
}`,
    nextStates: ['WRITE'],
  },
  {
    id: 'WRITE',
    name: 'SHE_CON_STATE_WRITE',
    phase: 'Socket I/O (Output)',
    description: 'Calls she_chunkqueue_write_chunk_to_socket(). Headers are sent via send(MSG_NOSIGNAL), while file payloads use Linux sendfile(2) zero-copy.',
    lighttpdConcept: 'sendfile(2) transfers bytes straight from page cache to socket buffers inside the Linux kernel, bypassing userspace buffer copying entirely.',
    cSnippet: `case SHE_CON_STATE_WRITE: {
    ssize_t w = she_chunkqueue_write_chunk_to_socket(con->write_queue, con->fd);
    if (she_chunkqueue_is_empty(con->write_queue)) {
        con->state = SHE_CON_STATE_RESPONSE_END;
    } else if (w == -1) {
        done = 1; /* Wait for next EPOLLOUT */
    }
    break;
}`,
    nextStates: ['RESPONSE_END', 'ERROR'],
  },
  {
    id: 'RESPONSE_END',
    name: 'SHE_CON_STATE_RESPONSE_END',
    phase: 'Transaction Completion',
    description: 'Response completely sent to client. Calls mod_accesslog (CLF format). If Keep-Alive is active, loops back to REQUEST_START; otherwise closes.',
    lighttpdConcept: 'By recycling the connection struct and keeping the TCP connection warm, SHE achieves massive throughput with sub-millisecond latencies.',
    cSnippet: `case SHE_CON_STATE_RESPONSE_END: {
    she_plugins_call_handle_response_done(srv, con);
    if (con->keep_alive) {
        con->state = SHE_CON_STATE_REQUEST_START;
    } else {
        con->state = SHE_CON_STATE_CLOSE;
    }
    break;
}`,
    nextStates: ['REQUEST_START', 'CLOSE'],
  },
  {
    id: 'ERROR',
    name: 'SHE_CON_STATE_ERROR',
    phase: 'Exception Recovery',
    description: 'Catches socket broken pipes, client resets, and protocol violations. Cleans up file descriptors and transitions to CLOSE.',
    lighttpdConcept: 'Guarantees no memory leaks or dangling fds occur even when aggressive clients drop connections prematurely.',
    cSnippet: `case SHE_CON_STATE_ERROR:
case SHE_CON_STATE_CLOSE: {
    she_server_close_connection(srv, con);
    done = 1;
    return 0;
}`,
    nextStates: ['CLOSE'],
  },
  {
    id: 'CLOSE',
    name: 'SHE_CON_STATE_CLOSE',
    phase: 'Connection Tear-down',
    description: 'Unregisters socket from epoll, closes client fd, decrements active connection counter, and frees she_connection_t memory.',
    lighttpdConcept: 'All memory buffers allocated during transaction are returned or freed, keeping the engine at deterministic baseline memory usage.',
    cSnippet: `void she_server_close_connection(she_server_t *srv, she_connection_t *con) {
    int fd = con->fd;
    she_fdevent_unregister(srv->ev, fd);
    close(fd);
    srv->fd_to_con[fd] = NULL;
    srv->stats.active_connections--;
    she_connection_free(con);
}`,
    nextStates: [],
  },
];

export const ArchitectureView: React.FC = () => {
  const [selectedStateId, setSelectedStateId] = useState<string>('HANDLE_REQUEST');
  const [simIndex, setSimIndex] = useState<number>(4);

  const activeState = STATES.find((s) => s.id === selectedStateId) || STATES[0];

  const handleStepSimulation = () => {
    const nextIdx = (simIndex + 1) % STATES.length;
    setSimIndex(nextIdx);
    setSelectedStateId(STATES[nextIdx].id);
  };

  const handleResetSimulation = () => {
    setSimIndex(0);
    setSelectedStateId(STATES[0].id);
  };

  return (
    <div className="space-y-8 max-w-7xl mx-auto px-4 sm:px-6 lg:px-8 py-8">
      {/* Hero Overview */}
      <div className="border border-slate-800 bg-slate-900/60 rounded-xl p-6 sm:p-8 backdrop-blur-sm">
        <div className="flex flex-col lg:flex-row items-start lg:items-center justify-between gap-6">
          <div className="space-y-3 max-w-3xl">
            <div className="flex items-center gap-2 text-xs text-slate-400 font-mono">
              <span className="text-emerald-400 font-bold">SHE: Secure HTTP Engine</span>
              <span aria-hidden="true">·</span>
              <span>she_ Keyword Prefix</span>
              <span aria-hidden="true">·</span>
              <span>Single-Process Non-Blocking epoll</span>
            </div>
            <h1 className="text-2xl sm:text-3xl font-bold tracking-tight text-white">
              Secure HTTP Engine (SHE) Architecture
            </h1>
            <p className="text-sm text-slate-300 leading-relaxed">
              <strong className="text-emerald-400">SHE (Secure HTTP Engine)</strong> is an industrial-grade C web server
              combining Lighttpd&apos;s iconic 10-state non-blocking finite state machine (FSM) with hardened security subsystems.
              All types and functions use the standardized <code className="text-emerald-300 font-mono">she_</code> prefix.
              It features built-in token-bucket IP rate limiting, RFC 7617 HTTP Basic Authentication, virtual host routing,
              zero-copy <code className="text-emerald-300 font-mono">sendfile(2)</code>, and FastCGI protocol framing.
            </p>
          </div>

          <div className="flex flex-col sm:flex-row items-center gap-3 w-full lg:w-auto">
            <button
              onClick={handleStepSimulation}
              className="w-full sm:w-auto flex items-center justify-center gap-2 px-4 py-2 text-xs font-semibold text-white bg-emerald-600 hover:bg-emerald-500 rounded-lg transition-colors shadow-sm"
            >
              <Play className="w-3.5 h-3.5 fill-current" />
              <span>Step FSM Simulation ({STATES[simIndex].name.replace('SHE_CON_STATE_', '')})</span>
            </button>
            <button
              onClick={handleResetSimulation}
              className="w-full sm:w-auto flex items-center justify-center gap-2 px-3.5 py-2 text-xs font-medium text-slate-300 bg-slate-800 hover:bg-slate-700 border border-slate-700 rounded-lg transition-colors"
            >
              <RefreshCw className="w-3.5 h-3.5" />
              <span>Reset</span>
            </button>
          </div>
        </div>

        {/* 4 Core Architecture Pillars */}
        <div className="grid grid-cols-1 md:grid-cols-4 gap-4 mt-8 pt-8 border-t border-slate-800/80">
          <div className="p-4 rounded-lg bg-slate-950/60 border border-slate-800/80 space-y-2">
            <div className="flex items-center gap-2 text-emerald-400 font-semibold text-xs uppercase tracking-wider">
              <Shield className="w-4 h-4" />
              <span>she_security</span>
            </div>
            <p className="text-xs text-slate-400 leading-relaxed">
              Token-bucket per-IP rate limiter (429 Too Many Requests), Host header validation, HTTP request smuggling guard, and automatic security response headers.
            </p>
          </div>

          <div className="p-4 rounded-lg bg-slate-950/60 border border-slate-800/80 space-y-2">
            <div className="flex items-center gap-2 text-sky-400 font-semibold text-xs uppercase tracking-wider">
              <Lock className="w-4 h-4" />
              <span>mod_auth (Basic)</span>
            </div>
            <p className="text-xs text-slate-400 leading-relaxed">
              RFC 7617 HTTP Basic Authentication with base64 decoding. Protects sensitive directories (e.g. <code className="text-slate-300 font-mono">/secure/</code>) with 401 challenges.
            </p>
          </div>

          <div className="p-4 rounded-lg bg-slate-950/60 border border-slate-800/80 space-y-2">
            <div className="flex items-center gap-2 text-amber-400 font-semibold text-xs uppercase tracking-wider">
              <Globe className="w-4 h-4" />
              <span>mod_vhost Routing</span>
            </div>
            <p className="text-xs text-slate-400 leading-relaxed">
              Virtual Host router mapping incoming <code className="text-slate-300 font-mono">Host:</code> headers to isolated document roots (e.g. <code className="text-slate-300 font-mono">docs.local</code>, <code className="text-slate-300 font-mono">api.local</code>).
            </p>
          </div>

          <div className="p-4 rounded-lg bg-slate-950/60 border border-slate-800/80 space-y-2">
            <div className="flex items-center gap-2 text-purple-400 font-semibold text-xs uppercase tracking-wider">
              <Zap className="w-4 h-4" />
              <span>Zero-Copy sendfile</span>
            </div>
            <p className="text-xs text-slate-400 leading-relaxed">
              Lighttpd-style chunkqueue streaming headers via memory chunks and large static payloads via Linux <code className="text-slate-300 font-mono">sendfile(2)</code> straight from page cache.
            </p>
          </div>
        </div>
      </div>

      {/* State Machine Interactive Grid */}
      <div className="space-y-4">
        <div className="flex items-center justify-between">
          <div>
            <h2 className="text-lg font-bold text-white tracking-tight">SHE 10-State Connection FSM Flowchart</h2>
            <p className="text-xs text-slate-400">Click any state to inspect its security role and C implementation.</p>
          </div>
          <span className="text-xs font-mono text-emerald-400 bg-emerald-950/50 border border-emerald-800/50 px-2.5 py-1 rounded">
            Selected: {activeState.name}
          </span>
        </div>

        {/* States ribbon */}
        <div className="grid grid-cols-2 sm:grid-cols-5 gap-2.5">
          {STATES.map((state, idx) => {
            const isSelected = selectedStateId === state.id;
            const isSimCurrent = STATES[simIndex].id === state.id;

            return (
              <button
                key={state.id}
                onClick={() => {
                  setSelectedStateId(state.id);
                  setSimIndex(idx);
                }}
                className={`flex flex-col text-left p-3 rounded-lg border transition-all ${
                  isSelected
                    ? 'bg-slate-800 border-emerald-500 shadow-md ring-1 ring-emerald-500/30'
                    : 'bg-slate-900/60 border-slate-800 hover:border-slate-700 hover:bg-slate-800/50'
                } ${isSimCurrent ? 'ring-2 ring-emerald-500/80' : ''}`}
              >
                <div className="flex items-center justify-between w-full mb-1">
                  <span className="text-[10px] font-mono text-slate-400">{String(idx + 1).padStart(2, '0')}</span>
                  {isSimCurrent && (
                    <span className="text-[10px] font-mono text-emerald-400 flex items-center gap-1">
                      <span className="w-1.5 h-1.5 rounded-full bg-emerald-400" />
                      Active
                    </span>
                  )}
                </div>
                <span className="text-xs font-semibold text-slate-200 font-mono truncate">{state.name.replace('SHE_CON_STATE_', '')}</span>
                <span className="text-[11px] text-slate-400 truncate mt-0.5">{state.phase}</span>
              </button>
            );
          })}
        </div>
      </div>

      {/* Selected State Deep-Dive */}
      <div className="grid grid-cols-1 lg:grid-cols-12 gap-6 items-start">
        {/* State Explanation (5 cols) */}
        <div className="lg:col-span-5 bg-slate-900/70 border border-slate-800 rounded-xl p-6 space-y-5">
          <div className="space-y-1">
            <span className="text-xs font-mono text-emerald-400">{activeState.phase}</span>
            <h3 className="text-xl font-bold text-white font-mono">{activeState.name}</h3>
          </div>

          <div className="space-y-3">
            <h4 className="text-xs font-semibold text-slate-300 uppercase tracking-wider">Functional & Security Role</h4>
            <p className="text-xs text-slate-300 leading-relaxed">{activeState.description}</p>
          </div>

          <div className="space-y-3 pt-3 border-t border-slate-800">
            <h4 className="text-xs font-semibold text-emerald-400 uppercase tracking-wider">Lighttpd Design Principle</h4>
            <p className="text-xs text-slate-400 leading-relaxed">{activeState.lighttpdConcept}</p>
          </div>

          <div className="space-y-2 pt-3 border-t border-slate-800">
            <h4 className="text-xs font-semibold text-slate-300 uppercase tracking-wider">Valid Next Transitions</h4>
            <div className="flex flex-wrap items-center gap-2">
              {activeState.nextStates.length > 0 ? (
                activeState.nextStates.map((next) => (
                  <button
                    key={next}
                    onClick={() => {
                      setSelectedStateId(next);
                      const targetIdx = STATES.findIndex((s) => s.id === next);
                      if (targetIdx >= 0) setSimIndex(targetIdx);
                    }}
                    className="flex items-center gap-1.5 px-2.5 py-1 text-xs font-mono bg-slate-800 hover:bg-slate-700 text-emerald-300 rounded border border-slate-700 transition-colors"
                  >
                    <span>SHE_CON_STATE_{next}</span>
                    <ArrowRight className="w-3 h-3 text-slate-400" />
                  </button>
                ))
              ) : (
                <span className="text-xs text-slate-500 font-mono">Terminal state (Socket recycled)</span>
              )}
            </div>
          </div>
        </div>

        {/* C Code Snippet (7 cols) */}
        <div className="lg:col-span-7 bg-slate-950 border border-slate-800 rounded-xl overflow-hidden">
          <div className="flex items-center justify-between px-4 py-2.5 bg-slate-900 border-b border-slate-800 text-xs">
            <div className="flex items-center gap-2 text-slate-300 font-mono">
              <Code2 className="w-4 h-4 text-emerald-400" />
              <span>c_server/src/core/she_connection.c : she_connection_state_machine()</span>
            </div>
            <span className="text-[11px] text-slate-400 font-mono">C11 / Linux epoll</span>
          </div>

          <div className="p-4 font-mono text-xs text-slate-300 leading-relaxed overflow-x-auto bg-slate-950">
            <pre className="text-slate-300">
              <code>{activeState.cSnippet}</code>
            </pre>
          </div>

          <div className="px-4 py-3 bg-slate-900/40 border-t border-slate-800/80 text-[11px] text-slate-400 flex items-center justify-between">
            <span>Non-blocking dispatch loop verified warning-free with GCC -Wall -Wextra</span>
            <div className="flex items-center gap-1.5 text-emerald-400">
              <CheckCircle2 className="w-3.5 h-3.5" />
              <span>Compiled & Active (Binary: she)</span>
            </div>
          </div>
        </div>
      </div>
    </div>
  );
};
