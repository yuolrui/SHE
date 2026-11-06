import React, { useState, useEffect } from 'react';
import { Layers, ShieldCheck, FileText, CheckCircle2, RefreshCw, Settings, Save, Lock, Globe, Terminal } from 'lucide-react';

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
  const [confContent, setConfContent] = useState<string>('');
  const [isSavingConf, setIsSavingConf] = useState<boolean>(false);
  const [confSavedMessage, setConfSavedMessage] = useState<string | null>(null);
  const [autoRefresh, setAutoRefresh] = useState<boolean>(true);

  const fetchData = async () => {
    try {
      const [pluginsRes, logsRes, confRes] = await Promise.all([
        fetch('/api/c-server/plugins'),
        fetch('/api/c-server/logs/access'),
        fetch('/api/c-server/config'),
      ]);

      if (pluginsRes.ok) {
        const pData = await pluginsRes.json();
        if (pData.plugins) setPlugins(pData.plugins);
      }
      if (logsRes.ok) {
        const lData = await logsRes.json();
        if (lData.lines) setAccessLogs(lData.lines);
      }
      if (confRes.ok) {
        const cData = await confRes.json();
        if (cData.content) setConfContent(cData.content);
      }
    } catch {
      // Silently ignore network hiccup
    }
  };

  useEffect(() => {
    fetchData();
    if (!autoRefresh) return;
    const interval = setInterval(fetchData, 3000);
    return () => clearInterval(interval);
  }, [autoRefresh]);

  const handleSaveConfig = async () => {
    setIsSavingConf(true);
    setConfSavedMessage(null);
    try {
      const res = await fetch('/api/c-server/config', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ content: confContent }),
      });
      if (res.ok) {
        setConfSavedMessage('Configuration saved to c_server/she.conf successfully!');
        setTimeout(() => setConfSavedMessage(null), 3000);
      }
    } catch {
      setConfSavedMessage('Failed to save configuration.');
    } finally {
      setIsSavingConf(false);
    }
  };

  return (
    <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8 py-8 space-y-8">
      {/* Intro Header */}
      <div className="bg-slate-900/60 border border-slate-800 rounded-xl p-6">
        <div className="flex items-center gap-2 text-xs text-slate-400 font-mono mb-2">
          <span className="text-emerald-400 font-bold">SHE: Phase 8 Active</span>
          <span aria-hidden="true">·</span>
          <span>Lighttpd-style she_plugin_t Subsystem</span>
          <span aria-hidden="true">·</span>
          <span>23 Active Modular Plugins</span>
        </div>
        <h2 className="text-xl font-bold text-white tracking-tight">
          SHE Modular Pipeline & Configuration Workbench
        </h2>
        <p className="text-xs text-slate-300 leading-relaxed max-w-3xl mt-1">
          SHE&apos;s modular architecture decouples high-level HTTP features into isolated plugins that hook into URI cleaning,
          subrequest evaluation, and response finalization. Built-in modules include <code className="text-emerald-300">she_security</code>,{' '}
          <code className="text-emerald-300">mod_ai_gateway</code>, <code className="text-emerald-300">mod_raft</code>,{' '}
          <code className="text-emerald-300">mod_otel</code>, <code className="text-emerald-300">mod_grpc</code>,{' '}
          <code className="text-emerald-300">mod_quic</code>, <code className="text-emerald-300">mod_wireguard</code>,{' '}
          <code className="text-emerald-300">mod_wasm</code>, <code className="text-emerald-300">mod_ebpf</code>,{' '}
          <code className="text-emerald-300">mod_acme</code>, <code className="text-emerald-300">mod_ssl</code>,{' '}
          <code className="text-emerald-300">mod_http2</code>, <code className="text-emerald-300">mod_websocket</code>,{' '}
          <code className="text-emerald-300">mod_proxy</code>, <code className="text-emerald-300">mod_deflate</code>,{' '}
          <code className="text-emerald-300">mod_rewrite</code>, <code className="text-emerald-300">mod_auth</code>,{' '}
          <code className="text-emerald-300">mod_vhost</code>, <code className="text-emerald-300">mod_fastcgi</code>,{' '}
          <code className="text-emerald-300">mod_status</code>, <code className="text-emerald-300">mod_indexfile</code>,{' '}
          <code className="text-emerald-300">mod_dirlisting</code>, and <code className="text-emerald-300">mod_accesslog</code>.
        </p>
      </div>

      {/* Active Modules Grid */}
      <div className="space-y-4">
        <div className="flex items-center justify-between">
          <h3 className="text-sm font-bold text-white uppercase tracking-wider">Registered Active Modules</h3>
          <span className="text-xs text-slate-400 font-mono">{plugins.length} Subsystems Initialized</span>
        </div>

        <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
          {plugins.map((p) => {
            const isSecurityModule = p.name.includes('security') || p.name.includes('auth');
            return (
              <div
                key={p.name}
                className="p-5 rounded-xl bg-slate-900/70 border border-slate-800 space-y-3 relative overflow-hidden"
              >
                <div className="flex items-start justify-between gap-3">
                  <div className="flex items-center gap-2.5">
                    <div className={`w-8 h-8 rounded-lg flex items-center justify-center ${
                      isSecurityModule
                        ? 'bg-emerald-500/10 border border-emerald-500/30 text-emerald-400'
                        : 'bg-sky-500/10 border border-sky-500/30 text-sky-400'
                    }`}>
                      {isSecurityModule ? <ShieldCheck className="w-4 h-4" /> : <Layers className="w-4 h-4" />}
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
                  <span className="text-slate-500 text-[11px]">Lifecycle Hooks:</span>
                  <div className="flex flex-wrap gap-1.5">
                    {p.hooks.map((h) => (
                      <span
                        key={h}
                        className="px-2 py-0.5 rounded bg-slate-800 text-emerald-300 text-[10px] border border-slate-700/60"
                      >
                        {h}()
                      </span>
                    ))}
                  </div>
                </div>
              </div>
            );
          })}
        </div>
      </div>

      {/* Configuration File Editor (she.conf) */}
      <div className="bg-slate-950 border border-slate-800 rounded-xl overflow-hidden flex flex-col space-y-0">
        <div className="flex flex-col sm:flex-row items-start sm:items-center justify-between gap-3 px-5 py-3.5 bg-slate-900 border-b border-slate-800">
          <div className="flex items-center gap-2.5">
            <Settings className="w-4 h-4 text-emerald-400" />
            <div>
              <span className="text-xs font-bold text-white font-mono">c_server/she.conf (Runtime Directives)</span>
              <p className="text-[11px] text-slate-400">Controls bindings, rate limits, basic auth realms, and virtual hosts</p>
            </div>
          </div>

          <div className="flex items-center gap-3">
            {confSavedMessage && (
              <span className="text-xs text-emerald-400 font-mono flex items-center gap-1">
                <CheckCircle2 className="w-3 h-3" />
                {confSavedMessage}
              </span>
            )}
            <button
              onClick={handleSaveConfig}
              disabled={isSavingConf}
              className="flex items-center gap-1.5 px-3 py-1.5 text-xs font-semibold text-slate-950 bg-emerald-400 hover:bg-emerald-300 rounded transition-colors disabled:opacity-50"
            >
              <Save className="w-3.5 h-3.5" />
              <span>{isSavingConf ? 'Saving...' : 'Save she.conf'}</span>
            </button>
          </div>
        </div>

        <div className="p-4 bg-slate-950">
          <textarea
            value={confContent}
            onChange={(e) => setConfContent(e.target.value)}
            rows={12}
            className="w-full bg-slate-900/60 border border-slate-800 rounded-lg p-3 font-mono text-xs text-emerald-300 placeholder-slate-600 focus:outline-none focus:border-emerald-500 leading-relaxed"
          />
        </div>
      </div>

      {/* Live Access Log Stream (mod_accesslog) */}
      <div className="bg-slate-950 border border-slate-800 rounded-xl overflow-hidden flex flex-col space-y-0">
        <div className="flex flex-col sm:flex-row items-start sm:items-center justify-between gap-3 px-5 py-3.5 bg-slate-900 border-b border-slate-800">
          <div className="flex items-center gap-2.5">
            <FileText className="w-4 h-4 text-emerald-400" />
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
              No access logs recorded yet. Send requests from the Security & Tester tab to see mod_accesslog stream output in real-time.
            </div>
          ) : (
            accessLogs.map((log, idx) => {
              const is200 = log.includes(' 200 ');
              const is206 = log.includes(' 206 ');
              const is304 = log.includes(' 304 ');
              const is401 = log.includes(' 401 ');
              const is429 = log.includes(' 429 ');
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
                        : is401
                        ? 'text-amber-300'
                        : is429
                        ? 'text-rose-400 font-semibold'
                        : is404
                        ? 'text-amber-400'
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
