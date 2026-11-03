import React, { useState, useEffect } from 'react';
import { Header } from './components/Header';
import { ArchitectureView } from './components/ArchitectureView';
import { CodeExplorer } from './components/CodeExplorer';
import { LiveTester } from './components/LiveTester';
import { BenchmarkLab } from './components/BenchmarkLab';
import { RoadmapView } from './components/RoadmapView';
import { PluginsView } from './components/PluginsView';

export default function App() {
  const [activeTab, setActiveTab] = useState<string>('overview');
  const [isRunning, setIsRunning] = useState<boolean>(true);
  const [isCompiling, setIsCompiling] = useState<boolean>(false);

  const fetchStatus = async () => {
    try {
      const res = await fetch('/api/c-server/status');
      if (!res.ok) return;
      const contentType = res.headers.get('content-type') || '';
      if (!contentType.includes('application/json')) return;
      const data = await res.json();
      if (data && typeof data.isRunning === 'boolean') {
        setIsRunning(data.isRunning);
      }
    } catch {
      // Silently ignore transient network or initial startup states
    }
  };

  useEffect(() => {
    fetchStatus();
    const interval = setInterval(fetchStatus, 4000);
    return () => clearInterval(interval);
  }, []);

  const handleCompile = async () => {
    setIsCompiling(true);
    try {
      await fetch('/api/c-server/compile', { method: 'POST' });
      fetchStatus();
    } catch (err) {
      console.error('Compilation error:', err);
    } finally {
      setIsCompiling(false);
    }
  };

  return (
    <div className="min-h-screen bg-slate-950 text-slate-100 flex flex-col selection:bg-sky-500/30 selection:text-sky-200">
      <Header
        activeTab={activeTab}
        onTabChange={setActiveTab}
        isRunning={isRunning}
        onCompile={handleCompile}
        isCompiling={isCompiling}
      />

      <main className="flex-1">
        {activeTab === 'overview' && <ArchitectureView />}
        {activeTab === 'code' && <CodeExplorer />}
        {activeTab === 'tester' && <LiveTester />}
        {activeTab === 'plugins' && <PluginsView />}
        {activeTab === 'benchmark' && <BenchmarkLab />}
        {activeTab === 'roadmap' && <RoadmapView />}
      </main>

      <footer className="border-t border-slate-900 bg-slate-950/80 py-6 px-6 text-xs text-slate-500">
        <div className="max-w-7xl mx-auto flex flex-col sm:flex-row items-center justify-between gap-4">
          <div className="flex items-center gap-2">
            <span className="font-semibold text-slate-400">c-lighttpd</span>
            <span aria-hidden="true">·</span>
            <span>Modeled on Lighttpd&apos;s Single-Process Event-Driven Architecture</span>
          </div>

          <div className="flex items-center gap-3 font-mono text-[11px] text-slate-500">
            <span>Linux epoll(7)</span>
            <span aria-hidden="true">·</span>
            <span>sendfile(2) Zero-Copy</span>
            <span aria-hidden="true">·</span>
            <span>10-State Connection FSM</span>
            <span aria-hidden="true">·</span>
            <span>GCC -O2</span>
          </div>
        </div>
      </footer>
    </div>
  );
}
