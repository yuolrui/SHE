import React, { useState } from 'react';
import { BarChart3, Zap, Activity, Clock, CheckCircle2, AlertCircle, RefreshCw } from 'lucide-react';

interface BenchmarkResult {
  totalRequests: number;
  concurrency: number;
  successful: number;
  failed: number;
  totalBytes: number;
  durationSeconds: number;
  rps: number;
  latency: {
    avg: number;
    p50: number;
    p95: number;
    min: number;
    max: number;
  };
}

export const BenchmarkLab: React.FC = () => {
  const [requests, setRequests] = useState<number>(250);
  const [concurrency, setConcurrency] = useState<number>(10);
  const [targetPath, setTargetPath] = useState<string>('/index.html');
  const [isRunning, setIsRunning] = useState<boolean>(false);
  const [result, setResult] = useState<BenchmarkResult | null>(null);
  const [history, setHistory] = useState<BenchmarkResult[]>([]);

  const runBenchmark = async () => {
    setIsRunning(true);
    try {
      const res = await fetch('/api/c-server/benchmark', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          requests,
          concurrency,
          path: targetPath,
        }),
      });
      if (res.ok) {
        const contentType = res.headers.get('content-type') || '';
        if (contentType.includes('application/json')) {
          const data: BenchmarkResult = await res.json();
          setResult(data);
          setHistory((prev) => [data, ...prev.slice(0, 4)]);
        }
      }
    } catch {
      // Benchmark error handled cleanly
    } finally {
      setIsRunning(false);
    }
  };

  return (
    <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8 py-8 space-y-6">
      {/* Intro */}
      <div className="bg-slate-900/60 border border-slate-800 rounded-xl p-6">
        <div className="flex items-center gap-2 text-xs text-slate-400 font-mono mb-2">
          <span>High-Concurrency Stress Benchmarking</span>
          <span aria-hidden="true">·</span>
          <span>HTTP/1.1 Persistent Keep-Alive</span>
          <span aria-hidden="true">·</span>
          <span>Zero-Copy sendfile(2)</span>
        </div>
        <h2 className="text-xl font-bold text-white tracking-tight">c-lighttpd Performance Testing Lab</h2>
        <p className="text-xs text-slate-300 leading-relaxed max-w-3xl mt-1">
          Simulate concurrent HTTP client requests against the real, live compiled C web server running inside this
          environment. Benchmark measuring latency distribution, throughput (requests/sec), and zero-copy packet throughput.
        </p>
      </div>

      {/* Control Strip */}
      <div className="grid grid-cols-1 md:grid-cols-4 gap-4 p-5 bg-slate-900/70 border border-slate-800 rounded-xl items-end">
        <div>
          <label className="block text-xs font-medium text-slate-400 mb-1.5">Total Requests</label>
          <select
            value={requests}
            onChange={(e) => setRequests(Number(e.target.value))}
            className="w-full bg-slate-950 border border-slate-800 rounded-md px-3 py-1.5 text-xs font-mono text-slate-200 focus:outline-none focus:border-sky-500"
          >
            <option value={50}>50 requests</option>
            <option value={100}>100 requests</option>
            <option value={250}>250 requests</option>
            <option value={500}>500 requests</option>
            <option value={1000}>1,000 requests</option>
          </select>
        </div>

        <div>
          <label className="block text-xs font-medium text-slate-400 mb-1.5">Concurrency (Parallel Sockets)</label>
          <select
            value={concurrency}
            onChange={(e) => setConcurrency(Number(e.target.value))}
            className="w-full bg-slate-950 border border-slate-800 rounded-md px-3 py-1.5 text-xs font-mono text-slate-200 focus:outline-none focus:border-sky-500"
          >
            <option value={5}>5 concurrent connections</option>
            <option value={10}>10 concurrent connections</option>
            <option value={25}>25 concurrent connections</option>
            <option value={50}>50 concurrent connections</option>
          </select>
        </div>

        <div>
          <label className="block text-xs font-medium text-slate-400 mb-1.5">Target Resource</label>
          <select
            value={targetPath}
            onChange={(e) => setTargetPath(e.target.value)}
            className="w-full bg-slate-950 border border-slate-800 rounded-md px-3 py-1.5 text-xs font-mono text-slate-200 focus:outline-none focus:border-sky-500"
          >
            <option value="/index.html">/index.html (HTML - 1.4 KB)</option>
            <option value="/style.css">/style.css (CSS - 1.3 KB)</option>
            <option value="/data.json">/data.json (JSON - 470 B)</option>
            <option value="/readme.txt">/readme.txt (Text - 380 B)</option>
          </select>
        </div>

        <div>
          <button
            onClick={runBenchmark}
            disabled={isRunning}
            className="w-full flex items-center justify-center gap-2 px-4 py-2 text-xs font-semibold text-white bg-sky-600 hover:bg-sky-500 rounded-md transition-colors shadow-sm disabled:opacity-50"
          >
            <Zap className={`w-3.5 h-3.5 ${isRunning ? 'animate-bounce' : 'fill-current'}`} />
            <span>{isRunning ? 'Running Stress Test...' : 'Run Benchmark'}</span>
          </button>
        </div>
      </div>

      {/* Metrics Results Grid */}
      {result ? (
        <div className="space-y-6">
          <div className="grid grid-cols-2 md:grid-cols-4 gap-4">
            <div className="bg-slate-900/60 border border-slate-800 rounded-xl p-5 space-y-1">
              <span className="text-[11px] font-mono text-slate-400">Throughput</span>
              <div className="text-2xl font-bold font-mono text-sky-400 tabular-nums">
                {result.rps.toLocaleString()} <span className="text-xs font-normal text-slate-400">req/s</span>
              </div>
              <span className="text-[11px] text-slate-500 font-mono">
                Duration: {result.durationSeconds}s
              </span>
            </div>

            <div className="bg-slate-900/60 border border-slate-800 rounded-xl p-5 space-y-1">
              <span className="text-[11px] font-mono text-slate-400">Average Latency</span>
              <div className="text-2xl font-bold font-mono text-emerald-400 tabular-nums">
                {result.latency.avg} <span className="text-xs font-normal text-slate-400">ms</span>
              </div>
              <span className="text-[11px] text-slate-500 font-mono">
                p50: {result.latency.p50}ms · p95: {result.latency.p95}ms
              </span>
            </div>

            <div className="bg-slate-900/60 border border-slate-800 rounded-xl p-5 space-y-1">
              <span className="text-[11px] font-mono text-slate-400">Success Rate</span>
              <div className="text-2xl font-bold font-mono text-white tabular-nums">
                {((result.successful / result.totalRequests) * 100).toFixed(1)}%
              </div>
              <span className="text-[11px] text-slate-500 font-mono">
                {result.successful} ok / {result.failed} failed
              </span>
            </div>

            <div className="bg-slate-900/60 border border-slate-800 rounded-xl p-5 space-y-1">
              <span className="text-[11px] font-mono text-slate-400">Data Transferred</span>
              <div className="text-2xl font-bold font-mono text-purple-400 tabular-nums">
                {(result.totalBytes / 1024).toFixed(1)} <span className="text-xs font-normal text-slate-400">KB</span>
              </div>
              <span className="text-[11px] text-slate-500 font-mono">
                Zero-copy sendfile(2)
              </span>
            </div>
          </div>

          {/* Latency Percentile Breakdown */}
          <div className="bg-slate-950 border border-slate-800 rounded-xl p-5">
            <h4 className="text-xs font-semibold text-slate-300 uppercase tracking-wider mb-4">
              Latency Percentile Distribution
            </h4>
            <div className="grid grid-cols-2 sm:grid-cols-5 gap-3 text-center">
              <div className="p-3 bg-slate-900/80 rounded-lg border border-slate-800">
                <span className="text-[10px] text-slate-400 font-mono">MIN</span>
                <div className="text-sm font-bold font-mono text-slate-200 mt-0.5 tabular-nums">
                  {result.latency.min} ms
                </div>
              </div>
              <div className="p-3 bg-slate-900/80 rounded-lg border border-slate-800">
                <span className="text-[10px] text-slate-400 font-mono">P50 (MEDIAN)</span>
                <div className="text-sm font-bold font-mono text-emerald-400 mt-0.5 tabular-nums">
                  {result.latency.p50} ms
                </div>
              </div>
              <div className="p-3 bg-slate-900/80 rounded-lg border border-slate-800">
                <span className="text-[10px] text-slate-400 font-mono">AVERAGE</span>
                <div className="text-sm font-bold font-mono text-sky-400 mt-0.5 tabular-nums">
                  {result.latency.avg} ms
                </div>
              </div>
              <div className="p-3 bg-slate-900/80 rounded-lg border border-slate-800">
                <span className="text-[10px] text-slate-400 font-mono">P95</span>
                <div className="text-sm font-bold font-mono text-amber-400 mt-0.5 tabular-nums">
                  {result.latency.p95} ms
                </div>
              </div>
              <div className="p-3 bg-slate-900/80 rounded-lg border border-slate-800">
                <span className="text-[10px] text-slate-400 font-mono">MAX</span>
                <div className="text-sm font-bold font-mono text-rose-400 mt-0.5 tabular-nums">
                  {result.latency.max} ms
                </div>
              </div>
            </div>
          </div>
        </div>
      ) : (
        <div className="bg-slate-950 border border-slate-800 rounded-xl p-12 text-center text-slate-500">
          Click &quot;Run Benchmark&quot; to execute stress testing on the active C server instance.
        </div>
      )}

      {/* Historical Runs */}
      {history.length > 1 && (
        <div className="bg-slate-900/50 border border-slate-800 rounded-xl p-5 space-y-3">
          <h4 className="text-xs font-semibold text-slate-300 uppercase tracking-wider">Recent Benchmark Runs</h4>
          <div className="overflow-x-auto">
            <table className="w-full text-xs font-mono">
              <thead>
                <tr className="border-b border-slate-800 text-left text-slate-500 text-[11px]">
                  <th className="py-2">Requests</th>
                  <th className="py-2">Concurrency</th>
                  <th className="py-2">RPS</th>
                  <th className="py-2">Avg Latency</th>
                  <th className="py-2">P95 Latency</th>
                  <th className="py-2">Transferred</th>
                </tr>
              </thead>
              <tbody>
                {history.map((h, i) => (
                  <tr key={i} className="border-b border-slate-800/40 hover:bg-slate-800/30">
                    <td className="py-2 text-slate-300 tabular-nums">{h.totalRequests}</td>
                    <td className="py-2 text-slate-300 tabular-nums">{h.concurrency}</td>
                    <td className="py-2 text-sky-400 font-bold tabular-nums">{h.rps} req/s</td>
                    <td className="py-2 text-emerald-400 tabular-nums">{h.latency.avg} ms</td>
                    <td className="py-2 text-amber-400 tabular-nums">{h.latency.p95} ms</td>
                    <td className="py-2 text-slate-400 tabular-nums">{(h.totalBytes / 1024).toFixed(1)} KB</td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
        </div>
      )}
    </div>
  );
};
