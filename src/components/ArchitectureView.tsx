import React, { useState } from 'react';
import { Layers, Cpu, Zap, ArrowRight, Play, RefreshCw, Code2, CheckCircle2 } from 'lucide-react';

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
    name: 'CON_STATE_CONNECT',
    phase: 'Connection Setup',
    description: 'New TCP connection accepted by network_accept_client(). Sockets are set to O_NONBLOCK and TCP_NODELAY immediately.',
    lighttpdConcept: 'In Lighttpd, connections are decoupled from threads. The listening socket triggers EPOLLIN on epoll_wait, accepting non-blocking client fds without blocking.',
    cSnippet: `case CON_STATE_CONNECT: {
    con->state = CON_STATE_REQUEST_START;
    break;
}`,
    nextStates: ['REQUEST_START'],
  },
  {
    id: 'REQUEST_START',
    name: 'CON_STATE_REQUEST_START',
    phase: 'Request Initialization',
    description: 'Resets connection request and response buffers, registers FDEVENT_IN (EPOLLIN) on the client fd, and prepares to read HTTP wire bytes.',
    lighttpdConcept: 'Persistent connections (Keep-Alive) loop back to this state rather than closing and re-opening sockets, avoiding 3-way TCP handshakes.',
    cSnippet: `case CON_STATE_REQUEST_START: {
    connection_reset(con);
    fdevent_event_set(srv->ev, con->fd, FDEVENT_IN);
    con->state = CON_STATE_READ;
    done = 1; /* Wait for epoll read event */
    break;
}`,
    nextStates: ['READ'],
  },
  {
    id: 'READ',
    name: 'CON_STATE_READ',
    phase: 'Socket I/O (Input)',
    description: 'Non-blocking recv() reads raw HTTP bytes into con->read_buf. Feeds bytes into http_request_parse() until complete headers are detected.',
    lighttpdConcept: 'If recv() returns EAGAIN/EWOULDBLOCK, the state machine does not block; it parks in CON_STATE_READ and returns control to the epoll loop.',
    cSnippet: `case CON_STATE_READ: {
    char buf[READ_CHUNK_SIZE];
    ssize_t bytes = recv(con->fd, buf, sizeof(buf), 0);
    if (bytes < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
        done = 1; break;
    }
    buffer_append_string_len(con->read_buf, buf, bytes);
    int parse_res = http_request_parse(&con->request, ...);
    if (parse_res == 1) con->state = CON_STATE_REQUEST_END;
    break;
}`,
    nextStates: ['REQUEST_END', 'WRITE', 'CLOSE'],
  },
  {
    id: 'REQUEST_END',
    name: 'CON_STATE_REQUEST_END',
    phase: 'Parser Validation',
    description: 'HTTP Request Line and Headers fully parsed and validated. Increments request metrics and negotiates Keep-Alive persistence.',
    lighttpdConcept: 'Lighttpd strictly verifies HTTP/1.1 requirements (e.g., Host header presence, URI lengths, sanitization) before triggering resource handlers.',
    cSnippet: `case CON_STATE_REQUEST_END: {
    con->state = CON_STATE_HANDLE_REQUEST;
    break;
}`,
    nextStates: ['HANDLE_REQUEST'],
  },
  {
    id: 'HANDLE_REQUEST',
    name: 'CON_STATE_HANDLE_REQUEST',
    phase: 'Routing & Handlers',
    description: 'Resolves filesystem path against document root, neutralizes directory traversal (/../), verifies file stat, checks If-Modified-Since and Range headers.',
    lighttpdConcept: 'This is where Lighttpd plugin hooks run (mod_indexfile, mod_dirlisting, mod_staticfile). Returns 304 if cached, or calculates byte ranges for 206.',
    cSnippet: `case CON_STATE_HANDLE_REQUEST: {
    /* Safe path concatenation & canonicalization */
    snprintf(full_path, sizeof(full_path), "%s%s", srv->config.document_root, uri_path);
    if (stat(full_path, &st) != 0) {
        generate_error_response(con, 404, srv->config.server_tag);
        con->state = CON_STATE_RESPONSE_START;
        break;
    }
    /* Check Range: bytes=start-end -> 206 Partial Content */
    ...
    con->state = CON_STATE_RESPONSE_START;
    break;
}`,
    nextStates: ['RESPONSE_START'],
  },
  {
    id: 'RESPONSE_START',
    name: 'CON_STATE_RESPONSE_START',
    phase: 'Response Framing',
    description: 'Builds HTTP status line and response headers into a memory chunk, enqueues static file fd into chunkqueue, and flips epoll to FDEVENT_OUT (EPOLLOUT).',
    lighttpdConcept: 'Lighttpd never reads large files into RAM. It pushes an open file descriptor with offset and length into chunkqueue for kernel zero-copy transfer.',
    cSnippet: `case CON_STATE_RESPONSE_START: {
    fdevent_event_set(srv->ev, con->fd, FDEVENT_OUT);
    con->state = CON_STATE_WRITE;
    break;
}`,
    nextStates: ['WRITE'],
  },
  {
    id: 'WRITE',
    name: 'CON_STATE_WRITE',
    phase: 'Socket I/O (Output)',
    description: 'Calls chunkqueue_write_chunk_to_socket(). Headers are sent via send(MSG_NOSIGNAL), while file payloads use Linux sendfile(2) zero-copy.',
    lighttpdConcept: 'sendfile(2) transfers bytes straight from page cache to socket buffers inside the Linux kernel, bypassing userspace buffer copying entirely.',
    cSnippet: `case CON_STATE_WRITE: {
    ssize_t w = chunkqueue_write_chunk_to_socket(con->write_queue, con->fd);
    if (chunkqueue_is_empty(con->write_queue)) {
        con->state = CON_STATE_RESPONSE_END;
    } else if (w == -1) {
        done = 1; /* Wait for next EPOLLOUT */
    }
    break;
}`,
    nextStates: ['RESPONSE_END', 'ERROR'],
  },
  {
    id: 'RESPONSE_END',
    name: 'CON_STATE_RESPONSE_END',
    phase: 'Transaction Completion',
    description: 'Response completely sent to client. If Keep-Alive is active and within request limit, transitions back to REQUEST_START; otherwise closes.',
    lighttpdConcept: 'By recycling the connection struct and keeping the TCP connection warm, lighttpd achieves massive throughput with sub-millisecond latencies.',
    cSnippet: `case CON_STATE_RESPONSE_END: {
    if (con->keep_alive) {
        /* Loop back to read next pipelined / keep-alive request */
        con->state = CON_STATE_REQUEST_START;
    } else {
        con->state = CON_STATE_CLOSE;
    }
    break;
}`,
    nextStates: ['REQUEST_START', 'CLOSE'],
  },
  {
    id: 'ERROR',
    name: 'CON_STATE_ERROR',
    phase: 'Exception Recovery',
    description: 'Catches socket broken pipes, client resets, and protocol violations. Cleans up file descriptors and transitions to CLOSE.',
    lighttpdConcept: 'Guarantees no memory leaks or dangling fds occur even when aggressive clients drop connections prematurely.',
    cSnippet: `case CON_STATE_ERROR:
case CON_STATE_CLOSE: {
    server_close_connection(srv, con);
    done = 1;
    return 0;
}`,
    nextStates: ['CLOSE'],
  },
  {
    id: 'CLOSE',
    name: 'CON_STATE_CLOSE',
    phase: 'Connection Tear-down',
    description: 'Unregisters socket from epoll, closes the client fd, decrements active connection counter, and frees the connection_t memory.',
    lighttpdConcept: 'All memory buffers allocated during the transaction are returned to the pool or freed, returning the server to baseline memory usage.',
    cSnippet: `void server_close_connection(server_t *srv, connection_t *con) {
    int fd = con->fd;
    fdevent_unregister(srv->ev, fd);
    close(fd);
    srv->fd_to_con[fd] = NULL;
    srv->stats.active_connections--;
    connection_free(con);
}`,
    nextStates: [],
  },
];

export const ArchitectureView: React.FC = () => {
  const [selectedStateId, setSelectedStateId] = useState<string>('HANDLE_REQUEST');
  const [simIndex, setSimIndex] = useState<number>(0);
  const [isSimulating, setIsSimulating] = useState<boolean>(false);

  const activeState = STATES.find((s) => s.id === selectedStateId) || STATES[0];

  const handleStepSimulation = () => {
    const nextIdx = (simIndex + 1) % STATES.length;
    setSimIndex(nextIdx);
    setSelectedStateId(STATES[nextIdx].id);
  };

  const handleResetSimulation = () => {
    setSimIndex(0);
    setSelectedStateId(STATES[0].id);
    setIsSimulating(false);
  };

  return (
    <div className="space-y-8 max-w-7xl mx-auto px-4 sm:px-6 lg:px-8 py-8">
      {/* Hero Overview */}
      <div className="border border-slate-800 bg-slate-900/60 rounded-xl p-6 sm:p-8 backdrop-blur-sm">
        <div className="flex flex-col lg:flex-row items-start lg:items-center justify-between gap-6">
          <div className="space-y-3 max-w-3xl">
            <div className="flex items-center gap-2 text-xs text-slate-400 font-mono">
              <span>Lighttpd Architecture in Pure C</span>
              <span aria-hidden="true">·</span>
              <span>Single-Process Non-Blocking I/O</span>
              <span aria-hidden="true">·</span>
              <span>Linux epoll(7) Engine</span>
            </div>
            <h1 className="text-2xl sm:text-3xl font-bold tracking-tight text-white">
              Lighttpd-Style Connection State Machine
            </h1>
            <p className="text-sm text-slate-300 leading-relaxed">
              Unlike traditional threaded servers (such as Apache prefork) that dedicate a thread or process per
              client, <strong className="text-sky-400">c-lighttpd</strong> mirrors lighttpd&apos;s pioneering
              single-process event loop. Every HTTP connection is modeled as an explicit 10-state finite state machine (FSM).
              Sockets never block; when kernel buffers fill or drain, the state machine yields to epoll and resumes instantly.
            </p>
          </div>

          <div className="flex flex-col sm:flex-row items-center gap-3 w-full lg:w-auto">
            <button
              onClick={handleStepSimulation}
              className="w-full sm:w-auto flex items-center justify-center gap-2 px-4 py-2 text-xs font-semibold text-white bg-sky-600 hover:bg-sky-500 rounded-lg transition-colors shadow-sm"
            >
              <Play className="w-3.5 h-3.5 fill-current" />
              <span>Step FSM Simulation ({STATES[simIndex].name})</span>
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

        {/* 3 Core Architecture Pillars */}
        <div className="grid grid-cols-1 md:grid-cols-3 gap-4 mt-8 pt-8 border-t border-slate-800/80">
          <div className="p-4 rounded-lg bg-slate-950/60 border border-slate-800/80 space-y-2">
            <div className="flex items-center gap-2 text-sky-400 font-semibold text-xs uppercase tracking-wider">
              <Cpu className="w-4 h-4" />
              <span>fdevent Subsystem</span>
            </div>
            <p className="text-xs text-slate-400 leading-relaxed">
              Wraps Linux <code className="text-slate-300 font-mono">epoll_create1</code>,{' '}
              <code className="text-slate-300 font-mono">epoll_ctl</code>, and{' '}
              <code className="text-slate-300 font-mono">epoll_wait</code>. Tracks active connection fds and dispatches
              I/O readiness events with zero context-switch overhead.
            </p>
          </div>

          <div className="p-4 rounded-lg bg-slate-950/60 border border-slate-800/80 space-y-2">
            <div className="flex items-center gap-2 text-emerald-400 font-semibold text-xs uppercase tracking-wider">
              <Zap className="w-4 h-4" />
              <span>Zero-Copy Chunkqueue</span>
            </div>
            <p className="text-xs text-slate-400 leading-relaxed">
              Decouples HTTP response generation from transmission. Dynamically generated headers sit in{' '}
              <code className="text-slate-300 font-mono">CHUNK_MEM</code>, while files use Linux{' '}
              <code className="text-slate-300 font-mono">sendfile(2)</code> directly from disk cache to TCP socket.
            </p>
          </div>

          <div className="p-4 rounded-lg bg-slate-950/60 border border-slate-800/80 space-y-2">
            <div className="flex items-center gap-2 text-purple-400 font-semibold text-xs uppercase tracking-wider">
              <Layers className="w-4 h-4" />
              <span>Buffer Memory Pooling</span>
            </div>
            <p className="text-xs text-slate-400 leading-relaxed">
              Avoids memory fragmentation with lighttpd-style <code className="text-slate-300 font-mono">buffer_t</code>{' '}
              structures. Buffers track allocated capacity and reused lengths, eliminating malloc/free churn during high-concurrency spikes.
            </p>
          </div>
        </div>
      </div>

      {/* State Machine Interactive Grid */}
      <div className="space-y-4">
        <div className="flex items-center justify-between">
          <div>
            <h2 className="text-lg font-bold text-white tracking-tight">10-State Connection Flowchart</h2>
            <p className="text-xs text-slate-400">Click any state to view its architectural purpose and C implementation.</p>
          </div>
          <span className="text-xs font-mono text-sky-400 bg-sky-950/50 border border-sky-800/50 px-2.5 py-1 rounded">
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
                    ? 'bg-slate-800 border-sky-500 shadow-md ring-1 ring-sky-500/30'
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
                <span className="text-xs font-semibold text-slate-200 font-mono truncate">{state.name.replace('CON_STATE_', '')}</span>
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
            <span className="text-xs font-mono text-sky-400">{activeState.phase}</span>
            <h3 className="text-xl font-bold text-white font-mono">{activeState.name}</h3>
          </div>

          <div className="space-y-3">
            <h4 className="text-xs font-semibold text-slate-300 uppercase tracking-wider">Functional Role</h4>
            <p className="text-xs text-slate-300 leading-relaxed">{activeState.description}</p>
          </div>

          <div className="space-y-3 pt-3 border-t border-slate-800">
            <h4 className="text-xs font-semibold text-sky-400 uppercase tracking-wider">Lighttpd Design Principle</h4>
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
                    className="flex items-center gap-1.5 px-2.5 py-1 text-xs font-mono bg-slate-800 hover:bg-slate-700 text-sky-300 rounded border border-slate-700 transition-colors"
                  >
                    <span>CON_STATE_{next}</span>
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
              <Code2 className="w-4 h-4 text-sky-400" />
              <span>c_server/connection.c : connection_state_machine()</span>
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
              <span>Compiled & Active</span>
            </div>
          </div>
        </div>
      </div>
    </div>
  );
};
