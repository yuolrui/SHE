import React from 'react';
import { ShieldCheck, Download, RotateCw } from 'lucide-react';

interface HeaderProps {
  activeTab: string;
  onTabChange: (tab: string) => void;
  isRunning: boolean;
  onCompile: () => void;
  isCompiling: boolean;
}

export const Header: React.FC<HeaderProps> = ({
  activeTab,
  onTabChange,
  isRunning,
  onCompile,
  isCompiling,
}) => {
  const navTabs = [
    { id: 'overview', label: 'Architecture' },
    { id: 'code', label: 'Code Explorer' },
    { id: 'tester', label: 'Security & Tester' },
    { id: 'plugins', label: 'Modules & Logs' },
    { id: 'benchmark', label: 'Benchmark' },
    { id: 'roadmap', label: 'Roadmap' },
  ];

  const handleDownload = () => {
    window.location.href = '/api/c-server/download';
  };

  return (
    <header className="sticky top-0 z-50 flex items-center justify-between gap-8 px-6 py-3.5 bg-slate-950/90 backdrop-blur-md border-b border-slate-800/80">
      {/* Zone 1: Wordmark */}
      <div className="flex items-center gap-3 shrink-0">
        <div className="w-8 h-8 rounded-lg bg-emerald-500/10 border border-emerald-500/30 flex items-center justify-center text-emerald-400">
          <ShieldCheck className="w-4 h-4" />
        </div>
        <div className="flex flex-col">
          <div className="flex items-center gap-2">
            <span className="text-base font-bold tracking-tight text-white whitespace-nowrap">
              SHE
            </span>
            <span className="text-[11px] px-1.5 py-0.5 rounded bg-emerald-500/20 text-emerald-300 font-mono font-medium">
              Secure HTTP Engine
            </span>
          </div>
          <span className="text-[11px] text-slate-400 font-mono tracking-tight">
            Pure C · she_ prefix · epoll · 10-State FSM · sendfile(2)
          </span>
        </div>
      </div>

      {/* Zone 2: Navigation Links */}
      <nav className="hidden lg:flex items-center gap-1 bg-slate-900/90 p-1 rounded-lg border border-slate-800">
        {navTabs.map((tab) => {
          const isActive = activeTab === tab.id;
          return (
            <button
              key={tab.id}
              onClick={() => onTabChange(tab.id)}
              className={`px-3.5 py-1.5 text-xs font-medium rounded-md transition-colors whitespace-nowrap shrink-0 ${
                isActive
                  ? 'bg-slate-800 text-emerald-400 shadow-sm border border-slate-700/60'
                  : 'text-slate-400 hover:text-slate-200 hover:bg-slate-800/40'
              }`}
            >
              {tab.label}
            </button>
          );
        })}
      </nav>

      {/* Zone 3: Actions */}
      <div className="flex items-center gap-2.5 shrink-0">
        <div className="hidden sm:flex items-center gap-2 text-xs text-slate-400 font-mono px-2.5 py-1 bg-slate-900/60 rounded-md border border-slate-800">
          <span className={`w-2 h-2 rounded-full ${isRunning ? 'bg-emerald-400' : 'bg-rose-400'}`} />
          <span>{isRunning ? 'Port 8899 Active' : 'Stopped'}</span>
        </div>

        <button
          onClick={onCompile}
          disabled={isCompiling}
          title="Recompile SHE C project using gcc"
          className="flex items-center gap-1.5 px-3 py-1.5 text-xs font-medium text-slate-300 bg-slate-900 hover:bg-slate-800 border border-slate-700/80 rounded-md transition-colors whitespace-nowrap disabled:opacity-50"
        >
          <RotateCw className={`w-3.5 h-3.5 text-emerald-400 ${isCompiling ? 'animate-spin' : ''}`} />
          <span>{isCompiling ? 'Compiling...' : 'Recompile (she)'}</span>
        </button>

        <button
          onClick={handleDownload}
          className="flex items-center gap-1.5 px-3.5 py-1.5 text-xs font-medium text-white bg-emerald-600 hover:bg-emerald-500 rounded-md shadow-sm transition-colors whitespace-nowrap shrink-0"
        >
          <Download className="w-3.5 h-3.5" />
          <span>Download C Code (.tar.gz)</span>
        </button>
      </div>
    </header>
  );
};
