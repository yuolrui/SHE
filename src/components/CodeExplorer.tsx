import React, { useState, useEffect } from 'react';
import { FileCode, Copy, Check, Download, Search, Terminal, Info } from 'lucide-react';

interface FileData {
  name: string;
  content: string;
  lines: number;
  size: number;
}

const FILE_NOTES: Record<string, string> = {
  'connection.c': 'Core Lighttpd Connection State Machine. Drives transitions between 10 states, non-blocking reading, chunkqueue writing, and Keep-Alive loops.',
  'connection.h': 'Header defining connection_t, the connection_state_t enum, and state transition function signatures.',
  'chunkqueue.c': 'Zero-copy response queue. Handles CHUNK_TYPE_MEM for headers/error pages and CHUNK_TYPE_FILE with Linux sendfile(2) for disk files.',
  'chunkqueue.h': 'Chunk queue struct definitions and non-blocking socket chunk streaming declarations.',
  'fdevent.c': 'Linux epoll abstraction layer. Implements epoll_create1, epoll_ctl, and epoll_wait event multiplexing loop.',
  'fdevent.h': 'fdevent_system_t definition, FDEVENT_IN, FDEVENT_OUT, FDEVENT_ERR flags, and callback prototypes.',
  'http_parser.c': 'HTTP/1.1 request line and header parser with strict path traversal sanitization, URL decoding, and Range header support.',
  'http_parser.h': 'http_request_t struct, HTTP method enums, and header key-value lookup structures.',
  'buffer.c': 'Lighttpd iconic dynamic buffer implementation. Reduces malloc/free cycles by tracking pre-allocated capacity.',
  'buffer.h': 'buffer_t struct and dynamic string manipulation macros and helper functions.',
  'network.c': 'Low-level socket creation, non-blocking fcntl(O_NONBLOCK), TCP_NODELAY, SO_REUSEADDR, and accept handling.',
  'network.h': 'Socket setup declarations and network helper function prototypes.',
  'mime_types.c': 'Fast static extension-to-MIME-type mapping table (HTML, CSS, JS, JSON, images, fonts, WASM).',
  'mime_types.h': 'MIME lookup declaration.',
  'server.c': 'Main server runtime structure, epoll event dispatching loop, SIGINT/SIGTERM handlers, and idle connection reaper.',
  'server.h': 'server_t struct, server_config_t, server_stats_t metrics counters.',
  'main.c': 'CLI argument parser (-p, -d, -k, -m, -v), signal setup, startup banner, and graceful shutdown sequence.',
  'Makefile': 'GCC compilation recipe targeting C11, -Wall, -Wextra, -O2, and -D_GNU_SOURCE.',
};

export const CodeExplorer: React.FC = () => {
  const [files, setFiles] = useState<FileData[]>([]);
  const [selectedFileName, setSelectedFileName] = useState<string>('connection.c');
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

  const selectedFile = files.find((f) => f.name === selectedFileName) || files[0];

  const handleCopy = () => {
    if (!selectedFile) return;
    navigator.clipboard.writeText(selectedFile.content);
    setCopied(true);
    setTimeout(() => setCopied(false), 2000);
  };

  const filteredFiles = files.filter((f) =>
    f.name.toLowerCase().includes(searchTerm.toLowerCase())
  );

  const fileLines = selectedFile ? selectedFile.content.split('\n') : [];

  return (
    <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8 py-8 space-y-6">
      {/* Header bar */}
      <div className="flex flex-col sm:flex-row items-start sm:items-center justify-between gap-4">
        <div>
          <h2 className="text-xl font-bold text-white tracking-tight">C Source Code Repository</h2>
          <p className="text-xs text-slate-400">
            Modular, production-grade C implementation following Lighttpd architecture principles.
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
            className="flex items-center gap-1.5 px-3.5 py-1.5 text-xs font-medium text-white bg-sky-600 hover:bg-sky-500 rounded-md transition-colors shadow-sm"
          >
            <Download className="w-3.5 h-3.5" />
            <span>Download All (.tar.gz)</span>
          </a>
        </div>
      </div>

      {/* Main File Explorer Container */}
      <div className="grid grid-cols-1 lg:grid-cols-12 gap-6 items-start">
        {/* Left Sidebar: File List (3 cols) */}
        <div className="lg:col-span-3 bg-slate-900/70 border border-slate-800 rounded-xl p-4 space-y-3">
          <div className="relative">
            <Search className="w-3.5 h-3.5 text-slate-500 absolute left-2.5 top-2.5" />
            <input
              type="text"
              placeholder="Filter files..."
              value={searchTerm}
              onChange={(e) => setSearchTerm(e.target.value)}
              className="w-full bg-slate-950 border border-slate-800 rounded-md pl-8 pr-3 py-1.5 text-xs text-slate-200 placeholder-slate-500 focus:outline-none focus:border-sky-500"
            />
          </div>

          <div className="space-y-1 max-h-[580px] overflow-y-auto pr-1">
            {filteredFiles.map((f) => {
              const isSelected = f.name === selectedFileName;
              const isHeader = f.name.endsWith('.h');
              const isMakefile = f.name === 'Makefile';

              return (
                <button
                  key={f.name}
                  onClick={() => setSelectedFileName(f.name)}
                  className={`w-full flex items-center justify-between px-3 py-2 rounded-md text-xs font-mono transition-colors text-left ${
                    isSelected
                      ? 'bg-sky-950/60 text-sky-400 border border-sky-800/60 font-semibold'
                      : 'text-slate-400 hover:text-slate-200 hover:bg-slate-800/40 border border-transparent'
                  }`}
                >
                  <div className="flex items-center gap-2 truncate">
                    <FileCode
                      className={`w-3.5 h-3.5 shrink-0 ${
                        isHeader ? 'text-amber-400' : isMakefile ? 'text-rose-400' : 'text-sky-400'
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

          <div className="pt-3 border-t border-slate-800/80 text-[11px] text-slate-500 font-mono">
            <span>{files.length} Files</span>
            <span aria-hidden="true" className="mx-1">·</span>
            <span>Total: {files.reduce((acc, f) => acc + f.lines, 0)} lines</span>
          </div>
        </div>

        {/* Right Editor View: Code Content (9 cols) */}
        <div className="lg:col-span-9 bg-slate-950 border border-slate-800 rounded-xl overflow-hidden flex flex-col">
          {/* Top meta strip */}
          <div className="flex flex-col sm:flex-row items-start sm:items-center justify-between gap-3 px-5 py-3 bg-slate-900 border-b border-slate-800">
            <div className="flex items-center gap-3">
              <span className="text-sm font-bold text-white font-mono">{selectedFileName}</span>
              <div className="flex items-center gap-2 text-xs text-slate-400 font-mono">
                <span>{selectedFile?.lines || 0} lines</span>
                <span aria-hidden="true">·</span>
                <span>{( (selectedFile?.size || 0) / 1024).toFixed(1)} KB</span>
              </div>
            </div>

            <div className="flex items-center gap-2 text-[11px] text-slate-400 font-mono bg-slate-950 px-2.5 py-1 rounded border border-slate-800">
              <Terminal className="w-3 h-3 text-sky-400" />
              <span>gcc -Wall -Wextra -O2</span>
            </div>
          </div>

          {/* Architectural note banner */}
          {FILE_NOTES[selectedFileName] && (
            <div className="px-5 py-2.5 bg-slate-900/40 border-b border-slate-800/60 text-xs text-slate-300 flex items-start gap-2.5">
              <Info className="w-4 h-4 text-sky-400 shrink-0 mt-0.5" />
              <p className="leading-relaxed">{FILE_NOTES[selectedFileName]}</p>
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
