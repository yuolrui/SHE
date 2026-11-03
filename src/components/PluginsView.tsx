import React, { useState, useEffect } from 'react';
import { Layers, Activity, FileText, CheckCircle2, RefreshCw, Send, Terminal, ExternalLink } from 'lucide-react';

interface PluginInfo {
  name: string;
  version: string;
  hooks: string[];
  desc: string;
  active: boolean;
}

export const PluginsView: React.FC = () => {
  const [plugins, setPlugins] = useState<PluginInfo[]>([]);
  const [accessLogs, setAccessLogs] = useState<string[]>([]);
  const [loading, setLoading] = useState<boolean>(true);
  const [autoRefresh, setAutoRefresh] = useState<boolean>(true);

  const fetchData = async () => {
    try {
      const [pluginsRes, logsRes] = await Promise.all([
        fetch('/api/c-server/plugins'),
        fetch('/api/c-server/logs/access'),
      ]);

      if (pluginsRes.ok) {
        const pData = await pluginsRes.json();
        if (pData.plugins) setPlugins(pData.plugins);
      }
      if (logsRes.ok) {
        const lData = await logsRes.json();
        if (lData.lines) setAccessLogs(lData.lines);
      }
    } catch {
      // Silently ignore network hiccup
    } finally {
      setLoading(false);
    }
  };

  useEffect(() => {
    fetchData();
    if (!autoRefresh) return;
    const interval = setInterval(fetchData, 3000);
    return () => clearInterval(interval);
  }, [autoRefresh]);

  const handleTestStatus = () => {
    window.open('http://127.0.0.1:8899/server-status', '_blank');
  };

  return (
    <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8 py-8 space-y-8">
      {/* Intro Header */}
      <div className="bg-slate-900/60 border border-slate-800 rounded-xl p-6">
        <div className="flex items-center gap-2 text-xs text-slate-400 font-mono mb-2">
          <span>Phase 2: Modular Architecture</span>
          <span aria-hidden="true">·</span>
          <span>Lighttpd plugin_t Hook System</span>
          <span aria-hidden="true">·</span>
          <span>4 Built-in Core Modules</span>
        </div>
        <h2 className="text-xl font-bold text-white tracking-tight">
          Lighttpd Modular Plugin Subsystem
        </h2>
        <p className="text-xs text-slate-300 leading-relaxed max-w-3xl mt-1">
          In Lighttpd, modules register function pointers into distinct hook phases of the request lifecycle.
          The core server remains clean and minimal while features like <code className="text-sky-300">mod_status</code>,{' '}
          <code className="text-sky-300">mod_indexfile</code>, <code className="text-sky-300">mod_dirlisting</code>, and{' '}
          <code className="text-sky-300">mod_accesslog</code> run as decoupled plugins.
        </p>
      </div>

      {/* Active Modules Grid */}
      <div className="space-y-4">
        <div className="flex items-center justify-between">
          <h3 className="text-sm font-bold text-white uppercase tracking-wider">Registered Active Modules</h3>
          <span className="text-xs text-slate-400 font-mono">4 Plugins Initialized</span>
        </div>

        <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
          {plugins.map((p) => (
            <div
              key={p.name}
              className="p-5 rounded-xl bg-slate-900/70 border border-slate-800 space-y-3 relative overflow-hidden"
            >
              <div className="flex items-start justify-between gap-3">
                <div className="flex items-center gap-2.5">
                  <div className="w-8 h-8 rounded-lg bg-sky-500/10 border border-sky-500/30 flex items-center justify-center text-sky-400">
                    <Layers className="w-4 h-4" />
                  </div>
                  <div>
                    <h4 className="text-sm font-bold text-white font-mono">{p.name}</h4>
                    <span className="text-[11px] text-slate-400 font-mono">v{p.version} · Active</span>
                  </div>
                </div>

                <div className="flex items-center gap-1.5 text-xs text-emerald-400 font-mono bg-emerald-950/60 border border-emerald-800/80 px-2 py-0.5 rounded">
                  <CheckCircle2 className="w-3.5 h-3.5" />
                  <span>Hooked</span>
                </div>
              </div>

              <p className="text-xs text-slate-300 leading-relaxed">{p.desc}</p>

              <div className="pt-2 border-t border-slate-800/80 flex items-center justify-between text-xs font-mono">
                <span className="text-slate-500 text-[11px]">Hooks:</span>
                <div className="flex flex-wrap gap-1.5">
                  {p.hooks.map((h) => (
                    <span
                      key={h}
                      className="px-2 py-0.5 rounded bg-slate-800 text-sky-300 text-[10px] border border-slate-700/60"
                    >
                      {h}()
                    </span>
                  ))}
                </div>
              </div>
            </div>
          ))}
        </div>
      </div>

      {/* Live Access Log Stream (mod_accesslog) */}
      <div className="bg-slate-950 border border-slate-800 rounded-xl overflow-hidden flex flex-col space-y-0">
        <div className="flex flex-col sm:flex-row items-start sm:items-center justify-between gap-3 px-5 py-3.5 bg-slate-900 border-b border-slate-800">
          <div className="flex items-center gap-2.5">
            <FileText className="w-4 h-4 text-sky-400" />
            <div>
              <span className="text-xs font-bold text-white font-mono">logs/access.log (mod_accesslog)</span>
              <p className="text-[11px] text-slate-400">Standard Common Log Format (CLF) / Combined HTTP Stream</p>
            </div>
          </div>

          <div className="flex items-center gap-3">
            <button
              onClick={() => setAutoRefresh(!autoRefresh)}
              className={`px-2.5 py-1 text-[11px] font-mono rounded border transition-colors ${
                autoRefresh
                  ? 'bg-emerald-950/60 text-emerald-400 border-emerald-800'
                  : 'bg-slate-800 text-slate-400 border-slate-700'
              }`}
            >
              {autoRefresh ? 'Live Polling: On (3s)' : 'Live Polling: Paused'}
            </button>
            <button
              onClick={fetchData}
              className="flex items-center gap-1.5 px-2.5 py-1 text-xs text-slate-300 bg-slate-800 hover:bg-slate-700 rounded border border-slate-700 transition-colors"
            >
              <RefreshCw className="w-3 h-3" />
              <span>Refresh</span>
            </button>
          </div>
        </div>

        <div className="p-4 font-mono text-xs max-h-72 overflow-y-auto bg-slate-950 text-slate-300 space-y-1">
          {accessLogs.length === 0 ? (
            <div className="py-12 text-center text-slate-500">
              No access logs recorded yet. Send requests from the Live Tester tab to see mod_accesslog stream output in real-time.
            </div>
          ) : (
            accessLogs.map((log, idx) => {
              const is200 = log.includes(' 200 ');
              const is206 = log.includes(' 206 ');
              const is304 = log.includes(' 304 ');
              const is404 = log.includes(' 404 ');

              return (
                <div key={idx} className="hover:bg-slate-900/50 py-0.5 px-1.5 rounded flex items-start gap-2">
                  <span className="text-slate-600 select-none text-[10px] w-6 shrink-0">{idx + 1}</span>
                  <span
                    className={`leading-relaxed break-all ${
                      is200 || is206
                        ? 'text-emerald-300'
                        : is304
                        ? 'text-sky-300'
                        : is404
                        ? 'text-amber-300'
                        : 'text-slate-300'
                    }`}
                  >
                    {log}
                  </span>
                </div>
              );
            })
          )}
        </div>
      </div>
    </div>
  );
};
