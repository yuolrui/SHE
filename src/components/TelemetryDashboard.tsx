import React, { useState, useEffect, useCallback } from 'react';
import {
  Activity,
  RefreshCw,
  ShieldCheck,
  Server,
  Network,
  Lock,
  Cpu,
  RotateCw,
  CheckCircle2,
  AlertTriangle,
  Zap,
  Container,
  Layers,
} from 'lucide-react';

interface MetricsSnapshot {
  scrapes_total: number;
  uptime_seconds: number;
  plugins_loaded: number;
  connections_total: number;
  connections_active: number;
  requests_total: number;
  bytes_read_total: number;
  bytes_written_total: number;
  config_reloads_total: number;
  security_rate_blocks_total: number;
  security_host_rejections_total: number;
  http_status_200: number;
  http_status_401: number;
  http_status_404: number;
  http_status_429: number;
}

interface SelfHealSnapshot {
  supervisor_id: string;
  auto_heal_enabled: boolean;
  health_checks_passed: number;
  anomalies_recovered: number;
  fd_leaks_prevented: number;
  config_reloads: number;
  graceful_drain_ready: boolean;
  sighup_reload_armed: boolean;
}

interface SandboxSnapshot {
  profile_name: string;
  proc_seccomp_mode: number;
  proc_no_new_privs: number;
  landlock_abi_version: number;
  allowed_syscalls_count: number;
  blocked_syscalls_count: number;
  audit_runs: number;
  formal_tests_verified: boolean;
}

interface ProxySlot {
  slot_id: number;
  backend: string;
  in_use: boolean;
  sockmap: boolean;
  reused: number;
  bytes_spliced: number;
}

interface ProxyPoolSnapshot {
  pool_name: string;
  slot_count: number;
  total_acquires: number;
  pool_hits: number;
  pool_misses: number;
  hit_ratio_pct: number;
  slots: ProxySlot[];
}

interface MtlsSnapshot {
  trust_domain: string;
  ca_subject: string;
  require_client_cert: boolean;
  ocsp_stapling_enabled: boolean;
  certs_verified: number;
  certs_rejected: number;
  last_spiffe_id: string;
  last_sha256_fp: string;
}

interface CloudMeshSnapshot {
  status: string;
  cluster_id: string;
  container_image: string;
  namespace: string;
  plugins_ready: number;
  active_connections: number;
  probes_served: number;
}

interface IoUringSnapshot {
  ring_id: string;
  sq_entries: number;
  cq_entries: number;
  registered_fixed_buffers: number;
  sqpoll_enabled: boolean;
  submissions_total: number;
  completions_total: number;
  zero_copy_send_bytes: number;
  syscall_batches_saved: number;
}

interface XdsRegion {
  region_id: string;
  endpoint: string;
  primary_pct: number;
  canary_pct: number;
  rtt_ms: number;
  circuit_open: boolean;
  routed_requests: number;
}

interface XdsSnapshot {
  control_plane_uri: string;
  snapshot_version: number;
  nonce: string;
  sync_count: number;
  regions: XdsRegion[];
}

interface ProfilerHotspot {
  stack: string;
  samples: number;
  cpu_pct: number;
}

interface ProfilerSnapshot {
  alertmanager_sink: string;
  alert_state: string;
  sample_freq_hz: number;
  total_samples: number;
  captures_run: number;
  hotspots: ProfilerHotspot[];
}

interface TelemetryPoint {
  ts: string;
  requests: number;
  bytesTotal: number;
  latencyMs: number;
}

function formatBytes(bytes: number): string {
  if (bytes < 1024) return `${bytes} B`;
  if (bytes < 1024 * 1024) return `${(bytes / 1024).toFixed(1)} KB`;
  if (bytes < 1024 * 1024 * 1024) return `${(bytes / (1024 * 1024)).toFixed(2)} MB`;
  return `${(bytes / (1024 * 1024 * 1024)).toFixed(2)} GB`;
}

function formatDuration(sec: number): string {
  const h = Math.floor(sec / 3600);
  const m = Math.floor((sec % 3600) / 60);
  const s = sec % 60;
  if (h > 0) return `${h}h ${m}m ${s}s`;
  if (m > 0) return `${m}m ${s}s`;
  return `${s}s`;
}

async function queryCEngine<T>(reqPath: string): Promise<{ data: T | null; latencyMs: number }> {
  const res = await fetch('/api/c-server/request', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ method: 'GET', path: reqPath, headers: {} }),
  });
  if (!res.ok) return { data: null, latencyMs: 0 };
  const payload = await res.json();
  if (!payload || typeof payload.body !== 'string') return { data: null, latencyMs: 0 };
  try {
    return {
      data: JSON.parse(payload.body) as T,
      latencyMs: typeof payload.latencyMs === 'number' ? payload.latencyMs : 0,
    };
  } catch {
    return { data: null, latencyMs: 0 };
  }
}

export const TelemetryDashboard: React.FC = () => {
  const [metrics, setMetrics] = useState<MetricsSnapshot | null>(null);
  const [selfHeal, setSelfHeal] = useState<SelfHealSnapshot | null>(null);
  const [sandbox, setSandbox] = useState<SandboxSnapshot | null>(null);
  const [proxyPool, setProxyPool] = useState<ProxyPoolSnapshot | null>(null);
  const [mtls, setMtls] = useState<MtlsSnapshot | null>(null);
  const [cloudMesh, setCloudMesh] = useState<CloudMeshSnapshot | null>(null);
  const [ioUring, setIoUring] = useState<IoUringSnapshot | null>(null);
  const [xds, setXds] = useState<XdsSnapshot | null>(null);
  const [profiler, setProfiler] = useState<ProfilerSnapshot | null>(null);
  const [history, setHistory] = useState<TelemetryPoint[]>([]);
  const [autoRefresh, setAutoRefresh] = useState<boolean>(true);
  const [isRefreshing, setIsRefreshing] = useState<boolean>(false);
  const [actionBanner, setActionBanner] = useState<string | null>(null);
  const [isActionRunning, setIsActionRunning] = useState<boolean>(false);
  const [lastUpdated, setLastUpdated] = useState<string>('--:--:--');

  const fetchAllTelemetry = useCallback(async () => {
    setIsRefreshing(true);
    try {
      const [mRes, shRes, sbRes, ppRes, mtRes, cmRes, urRes, xdsRes, prRes] = await Promise.all([
        queryCEngine<MetricsSnapshot>('/metrics-json'),
        queryCEngine<SelfHealSnapshot>('/self-heal-status?json=1'),
        queryCEngine<SandboxSnapshot>('/sandbox-status?json=1'),
        queryCEngine<ProxyPoolSnapshot>('/proxy-pool-status?json=1'),
        queryCEngine<MtlsSnapshot>('/mtls-status?json=1'),
        queryCEngine<CloudMeshSnapshot>('/cloud-mesh-status'),
        queryCEngine<IoUringSnapshot>('/uring-status?json=1'),
        queryCEngine<XdsSnapshot>('/xds-status?json=1'),
        queryCEngine<ProfilerSnapshot>('/profiler-status?json=1'),
      ]);

      if (mRes.data) {
        setMetrics(mRes.data);
        const ts = new Date().toTimeString().slice(0, 8);
        setLastUpdated(ts);
        setHistory((prev) => {
          const next = [
            ...prev,
            {
              ts,
              requests: mRes.data!.requests_total,
              bytesTotal: mRes.data!.bytes_read_total + mRes.data!.bytes_written_total,
              latencyMs: mRes.latencyMs,
            },
          ];
          return next.slice(-24);
        });
      }
      if (shRes.data) setSelfHeal(shRes.data);
      if (sbRes.data) setSandbox(sbRes.data);
      if (ppRes.data) setProxyPool(ppRes.data);
      if (mtRes.data) setMtls(mtRes.data);
      if (cmRes.data) setCloudMesh(cmRes.data);
      if (urRes.data) setIoUring(urRes.data);
      if (xdsRes.data) setXds(xdsRes.data);
      if (prRes.data) setProfiler(prRes.data);
    } catch {
      // Transient C engine restart or polling hiccup
    } finally {
      setIsRefreshing(false);
    }
  }, []);

  useEffect(() => {
    fetchAllTelemetry();
    if (!autoRefresh) return;
    const timer = setInterval(fetchAllTelemetry, 2000);
    return () => clearInterval(timer);
  }, [autoRefresh, fetchAllTelemetry]);

  const handleHotReload = async () => {
    setIsActionRunning(true);
    setActionBanner(null);
    try {
      const { data } = await queryCEngine<{ status: string; config_reloads_total: number; active_connections_preserved: number }>('/self-heal/reload');
      if (data) {
        setActionBanner(
          `Zero-Downtime Hot Reload complete (${data.status}) · Reload #${data.config_reloads_total} · Preserved ${data.active_connections_preserved} active connections`
        );
      }
      await fetchAllTelemetry();
    } finally {
      setIsActionRunning(false);
    }
  };

  const handleTriggerSpliceAndMtls = async () => {
    setIsActionRunning(true);
    setActionBanner(null);
    try {
      const [spliceRes, mtlsRes] = await Promise.all([
        queryCEngine<{ status: string; acquired_slot_id: number; backend: string }>('/proxy-pool/acquire?backend=127.0.0.1:3000'),
        queryCEngine<{ status: string; spiffe_id: string; x509_sha256_fingerprint: string }>('/mtls/verify?spiffe_id=spiffe://she.mesh/ns/prod/sa/api-gateway'),
      ]);
      if (spliceRes.data && mtlsRes.data) {
        setActionBanner(
          `eBPF SOCKMAP spliced slot #${spliceRes.data.acquired_slot_id} -> ${spliceRes.data.backend} · OpenSSL mTLS verified ${mtlsRes.data.spiffe_id}`
        );
      }
      await fetchAllTelemetry();
    } finally {
      setIsActionRunning(false);
    }
  };

  const handleSyncXdsAndProfile = async () => {
    setIsActionRunning(true);
    setActionBanner(null);
    try {
      const [xdsSyncRes, uringRes, profRes] = await Promise.all([
        queryCEngine<{ snapshot_version: number; nonce: string }>('/xds/sync?control_plane=xds://global.she.mesh:18000'),
        queryCEngine<{ cqe_sequence: number; bytes_submitted: number }>('/uring/submit?bytes=65536&offset=0'),
        queryCEngine<{ samples_captured: number; alertmanager_state: string }>('/profiler/capture?ms=100'),
      ]);
      if (xdsSyncRes.data && uringRes.data && profRes.data) {
        setActionBanner(
          `Delta-xDS synced v${xdsSyncRes.data.snapshot_version} (${xdsSyncRes.data.nonce}) · io_uring CQE #${uringRes.data.cqe_sequence} · Profiler +${profRes.data.samples_captured} samples (${profRes.data.alertmanager_state})`
        );
      }
      await fetchAllTelemetry();
    } finally {
      setIsActionRunning(false);
    }
  };

  const totalStatusResponses = metrics
    ? metrics.http_status_200 + metrics.http_status_401 + metrics.http_status_404 + metrics.http_status_429
    : 0;

  const pct = (val: number) =>
    totalStatusResponses > 0 ? Math.max(0, Math.round((val / totalStatusResponses) * 1000) / 10) : 0;

  // Compute rolling SVG sparkline coordinates for requests_total delta or cumulative values
  const reqDeltas = history.map((pt, i) => (i === 0 ? 0 : Math.max(0, pt.requests - history[i - 1].requests)));
  const maxDelta = Math.max(1, ...reqDeltas);
  const sparkWidth = 520;
  const sparkHeight = 96;
  const sparkPoints =
    history.length > 1
      ? reqDeltas
          .map((d, idx) => {
            const x = (idx / (history.length - 1)) * sparkWidth;
            const y = sparkHeight - 10 - (d / maxDelta) * (sparkHeight - 20);
            return `${x.toFixed(1)},${y.toFixed(1)}`;
          })
          .join(' ')
      : `0,${sparkHeight - 10} ${sparkWidth},${sparkHeight - 10}`;

  const latencyMax = Math.max(1, ...history.map((h) => h.latencyMs));
  const latencyPoints =
    history.length > 1
      ? history
          .map((pt, idx) => {
            const x = (idx / (history.length - 1)) * sparkWidth;
            const y = sparkHeight - 10 - (pt.latencyMs / latencyMax) * (sparkHeight - 20);
            return `${x.toFixed(1)},${y.toFixed(1)}`;
          })
          .join(' ')
      : `0,${sparkHeight - 10} ${sparkWidth},${sparkHeight - 10}`;

  return (
    <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8 py-8 space-y-6">
      {/* Top Control Header */}
      <div className="bg-slate-900/70 border border-slate-800 rounded-xl p-5 flex flex-col lg:flex-row items-start lg:items-center justify-between gap-4">
        <div className="space-y-1">
          <div className="flex items-center gap-2 text-xs font-mono text-emerald-400">
            <Activity className="w-4 h-4" />
            <span className="font-bold">LIVE C11 TELEMETRY & CLOUD MESH OBSERVABILITY</span>
            <span className="text-slate-500">·</span>
            <span className="text-slate-400">Last Scrape: {lastUpdated}</span>
          </div>
          <h2 className="text-xl font-bold text-white tracking-tight">
            SHE Real-Time Kernel & Proxy Telemetry Dashboard
          </h2>
          <p className="text-xs text-slate-400">
            Directly polling <code className="text-emerald-300 font-mono">/metrics-json</code>,{' '}
            <code className="text-emerald-300 font-mono">/proxy-pool-status</code>,{' '}
            <code className="text-emerald-300 font-mono">/mtls-status</code>,{' '}
            <code className="text-emerald-300 font-mono">/self-heal-status</code>, and{' '}
            <code className="text-emerald-300 font-mono">/cloud-mesh-status</code> from port 8899.
          </p>
        </div>

        <div className="flex flex-wrap items-center gap-2.5">
          <button
            onClick={() => setAutoRefresh(!autoRefresh)}
            className={`px-3 py-1.5 text-xs font-mono rounded-md border transition-colors ${
              autoRefresh
                ? 'bg-emerald-950/70 text-emerald-300 border-emerald-700/80'
                : 'bg-slate-900 text-slate-400 border-slate-700'
            }`}
          >
            {autoRefresh ? 'Auto-Poll: 2s ON' : 'Auto-Poll: PAUSED'}
          </button>

          <button
            onClick={fetchAllTelemetry}
            disabled={isRefreshing}
            className="flex items-center gap-1.5 px-3 py-1.5 text-xs font-medium text-slate-200 bg-slate-800 hover:bg-slate-700 border border-slate-700 rounded-md transition-colors disabled:opacity-50"
          >
            <RefreshCw className={`w-3.5 h-3.5 text-emerald-400 ${isRefreshing ? 'animate-spin' : ''}`} />
            <span>Refresh</span>
          </button>

          <button
            onClick={handleTriggerSpliceAndMtls}
            disabled={isActionRunning}
            className="flex items-center gap-1.5 px-3 py-1.5 text-xs font-semibold text-sky-950 bg-sky-400 hover:bg-sky-300 rounded-md transition-colors disabled:opacity-50"
          >
            <Zap className="w-3.5 h-3.5 fill-current" />
            <span>Probe Splice & mTLS</span>
          </button>

          <button
            onClick={handleSyncXdsAndProfile}
            disabled={isActionRunning}
            className="flex items-center gap-1.5 px-3 py-1.5 text-xs font-semibold text-purple-950 bg-purple-400 hover:bg-purple-300 rounded-md transition-colors disabled:opacity-50"
          >
            <Activity className="w-3.5 h-3.5" />
            <span>Sync xDS & Profile</span>
          </button>

          <button
            onClick={handleHotReload}
            disabled={isActionRunning}
            className="flex items-center gap-1.5 px-3.5 py-1.5 text-xs font-semibold text-slate-950 bg-emerald-400 hover:bg-emerald-300 rounded-md transition-colors disabled:opacity-50"
          >
            <RotateCw className={`w-3.5 h-3.5 ${isActionRunning ? 'animate-spin' : ''}`} />
            <span>Zero-Downtime Reload</span>
          </button>
        </div>
      </div>

      {actionBanner && (
        <div className="bg-emerald-950/50 border border-emerald-700/70 rounded-xl px-4 py-3 flex items-center gap-2.5 text-xs font-mono text-emerald-300">
          <CheckCircle2 className="w-4 h-4 text-emerald-400 shrink-0" />
          <span>{actionBanner}</span>
        </div>
      )}

      {/* Primary KPI Grid */}
      <div className="grid grid-cols-2 sm:grid-cols-4 gap-4">
        <div className="bg-slate-900/70 border border-slate-800 rounded-xl p-4 space-y-1.5">
          <div className="flex items-center justify-between text-xs text-slate-400">
            <span>Engine Uptime & Plugins</span>
            <Server className="w-4 h-4 text-emerald-400" />
          </div>
          <div className="text-2xl font-bold font-mono text-white tabular-nums">
            {metrics ? formatDuration(metrics.uptime_seconds) : '--'}
          </div>
          <div className="text-[11px] font-mono text-emerald-400">
            {metrics ? `${metrics.plugins_loaded} C11 Plugins Active` : 'Awaiting metrics...'}
          </div>
        </div>

        <div className="bg-slate-900/70 border border-slate-800 rounded-xl p-4 space-y-1.5">
          <div className="flex items-center justify-between text-xs text-slate-400">
            <span>Total Requests / Conns</span>
            <Activity className="w-4 h-4 text-sky-400" />
          </div>
          <div className="text-2xl font-bold font-mono text-white tabular-nums">
            {metrics ? metrics.requests_total.toLocaleString() : '--'}
          </div>
          <div className="text-[11px] font-mono text-slate-400">
            {metrics
              ? `${metrics.connections_total} total · ${metrics.connections_active} active FDs`
              : 'Awaiting metrics...'}
          </div>
        </div>

        <div className="bg-slate-900/70 border border-slate-800 rounded-xl p-4 space-y-1.5">
          <div className="flex items-center justify-between text-xs text-slate-400">
            <span>eBPF SOCKMAP Hit Ratio</span>
            <Network className="w-4 h-4 text-emerald-400" />
          </div>
          <div className="text-2xl font-bold font-mono text-emerald-400 tabular-nums">
            {proxyPool ? `${proxyPool.hit_ratio_pct.toFixed(2)}%` : '--'}
          </div>
          <div className="text-[11px] font-mono text-slate-400">
            {proxyPool
              ? `${proxyPool.pool_hits.toLocaleString()} hits / ${proxyPool.pool_misses.toLocaleString()} misses`
              : 'Awaiting pool...'}
          </div>
        </div>

        <div className="bg-slate-900/70 border border-slate-800 rounded-xl p-4 space-y-1.5">
          <div className="flex items-center justify-between text-xs text-slate-400">
            <span>mTLS SPIFFE Verified</span>
            <Lock className="w-4 h-4 text-amber-400" />
          </div>
          <div className="text-2xl font-bold font-mono text-white tabular-nums">
            {mtls ? mtls.certs_verified.toLocaleString() : '--'}
          </div>
          <div className="text-[11px] font-mono text-slate-400">
            {mtls ? `${mtls.certs_rejected} rejected · OCSP Stapled` : 'Awaiting OpenSSL...'}
          </div>
        </div>
      </div>

      {/* Secondary KPI Row: Bytes RX/TX, Hot Reloads, Security Blocks, Cloud Mesh */}
      <div className="grid grid-cols-1 md:grid-cols-3 gap-4">
        <div className="bg-slate-900/70 border border-slate-800 rounded-xl p-4 flex items-center justify-between">
          <div>
            <span className="text-xs text-slate-400">Socket I/O Throughput (RX / TX)</span>
            <div className="text-lg font-bold font-mono text-white mt-1 tabular-nums">
              {metrics
                ? `${formatBytes(metrics.bytes_read_total)} / ${formatBytes(metrics.bytes_written_total)}`
                : '--'}
            </div>
            <span className="text-[11px] font-mono text-slate-500">
              {metrics ? `${metrics.scrapes_total} Prometheus scrapes served` : ''}
            </span>
          </div>
          <Cpu className="w-8 h-8 text-sky-400/70" />
        </div>

        <div className="bg-slate-900/70 border border-slate-800 rounded-xl p-4 flex items-center justify-between">
          <div>
            <span className="text-xs text-slate-400">Self-Heal & SIGHUP Hot Reloads</span>
            <div className="text-lg font-bold font-mono text-white mt-1 tabular-nums">
              {metrics ? `${metrics.config_reloads_total} Reloads` : '--'}
            </div>
            <span className="text-[11px] font-mono text-emerald-400">
              {selfHeal
                ? `${selfHeal.health_checks_passed} checks · ${selfHeal.fd_leaks_prevented} FD leaks prevented`
                : ''}
            </span>
          </div>
          <ShieldCheck className="w-8 h-8 text-emerald-400/70" />
        </div>

        <div className="bg-slate-900/70 border border-slate-800 rounded-xl p-4 flex items-center justify-between">
          <div>
            <span className="text-xs text-slate-400">Cloud Mesh Pod & Rate Guards</span>
            <div className="text-lg font-bold font-mono text-white mt-1">
              {cloudMesh ? `${cloudMesh.cluster_id} (${cloudMesh.status})` : '--'}
            </div>
            <span className="text-[11px] font-mono text-slate-400">
              {metrics
                ? `429 Rate Blocks: ${metrics.security_rate_blocks_total} · Host Rejects: ${metrics.security_host_rejections_total}`
                : ''}
            </span>
          </div>
          <Container className="w-8 h-8 text-purple-400/70" />
        </div>
      </div>

      {/* Sparklines & HTTP Status Distribution */}
      <div className="grid grid-cols-1 lg:grid-cols-12 gap-6">
        {/* Rolling Telemetry Sparkline */}
        <div className="lg:col-span-7 bg-slate-900/70 border border-slate-800 rounded-xl p-5 space-y-4">
          <div className="flex items-center justify-between">
            <div>
              <h3 className="text-sm font-bold text-white">Rolling Request Delta & Scrape Latency</h3>
              <p className="text-[11px] text-slate-400 font-mono">
                Last {history.length} sampling intervals (2s cadence)
              </p>
            </div>
            <div className="flex items-center gap-4 text-[11px] font-mono">
              <span className="flex items-center gap-1.5 text-emerald-400">
                <span className="w-2.5 h-0.5 bg-emerald-400 inline-block" />
                Req/interval
              </span>
              <span className="flex items-center gap-1.5 text-sky-400">
                <span className="w-2.5 h-0.5 bg-sky-400 inline-block" />
                Latency (ms)
              </span>
            </div>
          </div>

          <div className="bg-slate-950 border border-slate-800/80 rounded-lg p-3">
            <svg viewBox={`0 0 ${sparkWidth} ${sparkHeight}`} className="w-full h-28 overflow-visible">
              <line x1="0" y1={sparkHeight - 10} x2={sparkWidth} y2={sparkHeight - 10} stroke="#1e293b" strokeWidth="1" />
              <line x1="0" y1={sparkHeight / 2} x2={sparkWidth} y2={sparkHeight / 2} stroke="#1e293b" strokeDasharray="4 4" strokeWidth="1" />
              <polyline
                fill="none"
                stroke="#34d399"
                strokeWidth="2.2"
                strokeLinejoin="round"
                strokeLinecap="round"
                points={sparkPoints}
              />
              <polyline
                fill="none"
                stroke="#38bdf8"
                strokeWidth="1.6"
                strokeDasharray="3 3"
                strokeLinejoin="round"
                points={latencyPoints}
              />
            </svg>
          </div>

          <div className="flex items-center justify-between text-[11px] font-mono text-slate-400">
            <span>
              Peak Interval Delta: <strong className="text-emerald-400">{Math.max(0, ...reqDeltas)} reqs</strong>
            </span>
            <span>
              Latest Roundtrip: <strong className="text-sky-400">{history[history.length - 1]?.latencyMs ?? 0} ms</strong>
            </span>
          </div>
        </div>

        {/* HTTP Status Distribution & Sandbox/mTLS */}
        <div className="lg:col-span-5 bg-slate-900/70 border border-slate-800 rounded-xl p-5 space-y-4">
          <div className="flex items-center justify-between">
            <h3 className="text-sm font-bold text-white">HTTP Response Status Distribution</h3>
            <span className="text-[11px] font-mono text-slate-400 tabular-nums">
              {totalStatusResponses.toLocaleString()} tracked
            </span>
          </div>

          <div className="h-3 w-full rounded-full overflow-hidden bg-slate-950 border border-slate-800 flex">
            <div
              className="bg-emerald-500 transition-all"
              style={{ width: `${metrics ? pct(metrics.http_status_200) : 0}%` }}
              title="200 OK"
            />
            <div
              className="bg-amber-400 transition-all"
              style={{ width: `${metrics ? pct(metrics.http_status_401) : 0}%` }}
              title="401 Unauthorized"
            />
            <div
              className="bg-sky-400 transition-all"
              style={{ width: `${metrics ? pct(metrics.http_status_404) : 0}%` }}
              title="404 Not Found"
            />
            <div
              className="bg-rose-500 transition-all"
              style={{ width: `${metrics ? pct(metrics.http_status_429) : 0}%` }}
              title="429 Rate Limited"
            />
          </div>

          <div className="grid grid-cols-2 gap-2.5 text-xs font-mono">
            <div className="bg-slate-950/70 border border-slate-800/80 rounded-lg p-2.5 flex items-center justify-between">
              <span className="text-emerald-400">200 OK</span>
              <span className="text-white font-bold tabular-nums">
                {metrics ? `${metrics.http_status_200} (${pct(metrics.http_status_200)}%)` : '--'}
              </span>
            </div>
            <div className="bg-slate-950/70 border border-slate-800/80 rounded-lg p-2.5 flex items-center justify-between">
              <span className="text-amber-400">401 Auth</span>
              <span className="text-white font-bold tabular-nums">
                {metrics ? `${metrics.http_status_401} (${pct(metrics.http_status_401)}%)` : '--'}
              </span>
            </div>
            <div className="bg-slate-950/70 border border-slate-800/80 rounded-lg p-2.5 flex items-center justify-between">
              <span className="text-sky-400">404 Missing</span>
              <span className="text-white font-bold tabular-nums">
                {metrics ? `${metrics.http_status_404} (${pct(metrics.http_status_404)}%)` : '--'}
              </span>
            </div>
            <div className="bg-slate-950/70 border border-slate-800/80 rounded-lg p-2.5 flex items-center justify-between">
              <span className="text-rose-400">429 Limited</span>
              <span className="text-white font-bold tabular-nums">
                {metrics ? `${metrics.http_status_429} (${pct(metrics.http_status_429)}%)` : '--'}
              </span>
            </div>
          </div>

          {/* Sandbox & mTLS Summary */}
          <div className="pt-2 border-t border-slate-800 space-y-2 text-xs font-mono">
            <div className="flex items-center justify-between">
              <span className="text-slate-400">Sandbox Profile</span>
              <span className="text-emerald-300">{sandbox?.profile_name || '--'}</span>
            </div>
            <div className="flex items-center justify-between">
              <span className="text-slate-400">Landlock / Syscalls</span>
              <span className="text-slate-200">
                {sandbox ? `ABI v${sandbox.landlock_abi_version} · ${sandbox.allowed_syscalls_count} allow / ${sandbox.blocked_syscalls_count} deny` : '--'}
              </span>
            </div>
            <div className="flex items-center justify-between">
              <span className="text-slate-400">Last Verified SPIFFE</span>
              <span className="text-sky-300 truncate max-w-[230px]" title={mtls?.last_spiffe_id}>
                {mtls?.last_spiffe_id || '--'}
              </span>
            </div>
          </div>
        </div>
      </div>

      {/* eBPF SOCKMAP Upstream Connection Pool Table */}
      <div className="bg-slate-900/70 border border-slate-800 rounded-xl overflow-hidden">
        <div className="px-5 py-3.5 bg-slate-900 border-b border-slate-800 flex flex-col sm:flex-row sm:items-center justify-between gap-2">
          <div className="flex items-center gap-2.5">
            <Layers className="w-4 h-4 text-emerald-400" />
            <div>
              <h3 className="text-sm font-bold text-white">
                eBPF SOCKMAP Persistent Upstream Keep-Alive Pool (mod_proxy_pool)
              </h3>
              <p className="text-[11px] font-mono text-slate-400">
                {proxyPool ? `${proxyPool.pool_name} · ${proxyPool.slot_count} kernel-spliced slots` : 'Loading upstream slot map...'}
              </p>
            </div>
          </div>
          {cloudMesh && (
            <div className="flex items-center gap-2 text-[11px] font-mono px-2.5 py-1 rounded bg-slate-950 border border-slate-800 text-slate-300">
              {cloudMesh.status === 'READY' ? (
                <CheckCircle2 className="w-3.5 h-3.5 text-emerald-400" />
              ) : (
                <AlertTriangle className="w-3.5 h-3.5 text-amber-400" />
              )}
              <span>
                {cloudMesh.namespace} · {cloudMesh.container_image} · {cloudMesh.probes_served} probes
              </span>
            </div>
          )}
        </div>

        <div className="overflow-x-auto">
          <table className="w-full text-left border-collapse font-mono text-xs">
            <thead>
              <tr className="border-b border-slate-800 bg-slate-950/60 text-slate-400 text-[11px]">
                <th className="px-5 py-2.5 font-medium">Slot</th>
                <th className="px-5 py-2.5 font-medium">Upstream Backend</th>
                <th className="px-5 py-2.5 font-medium">State</th>
                <th className="px-5 py-2.5 font-medium">eBPF SOCKMAP</th>
                <th className="px-5 py-2.5 font-medium text-right">Requests Reused</th>
                <th className="px-5 py-2.5 font-medium text-right">Bytes Spliced</th>
              </tr>
            </thead>
            <tbody>
              {(proxyPool?.slots || []).map((slot) => (
                <tr key={slot.slot_id} className="border-b border-slate-800/60 hover:bg-slate-800/30">
                  <td className="px-5 py-2.5 text-emerald-400 font-bold">#{slot.slot_id}</td>
                  <td className="px-5 py-2.5 text-slate-200">{slot.backend}</td>
                  <td className="px-5 py-2.5">
                    <span
                      className={`px-2 py-0.5 rounded text-[10px] font-semibold ${
                        slot.in_use
                          ? 'bg-sky-950/80 text-sky-300 border border-sky-800/70'
                          : 'bg-emerald-950/70 text-emerald-400 border border-emerald-800/70'
                      }`}
                    >
                      {slot.in_use ? 'ACTIVE_SPLICE' : 'IDLE_KEEPALIVE'}
                    </span>
                  </td>
                  <td className="px-5 py-2.5 text-slate-300">
                    {slot.sockmap ? 'BPF_MAP_ATTACHED' : 'DETACHED'}
                  </td>
                  <td className="px-5 py-2.5 text-right text-slate-200 tabular-nums">
                    {slot.reused.toLocaleString()}
                  </td>
                  <td className="px-5 py-2.5 text-right text-emerald-300 tabular-nums">
                    {formatBytes(slot.bytes_spliced)}
                  </td>
                </tr>
              ))}
              {(!proxyPool || proxyPool.slots.length === 0) && (
                <tr>
                  <td colSpan={6} className="px-5 py-8 text-center text-slate-500">
                    Waiting for mod_proxy_pool slot telemetry from SHE C11 engine...
                  </td>
                </tr>
              )}
            </tbody>
          </table>
        </div>
      </div>

      {/* Phase 15: Multi-Region xDS Control Plane, io_uring & eBPF Flamegraph Profiler */}
      <div className="grid grid-cols-1 lg:grid-cols-12 gap-6">
        <div className="lg:col-span-7 bg-slate-900/70 border border-slate-800 rounded-xl overflow-hidden">
          <div className="px-5 py-3.5 bg-slate-900 border-b border-slate-800 flex flex-col sm:flex-row sm:items-center justify-between gap-2">
            <div>
              <h3 className="text-sm font-bold text-white">
                Multi-Region xDS Control Plane & Canary Clusters (mod_xds)
              </h3>
              <p className="text-[11px] font-mono text-slate-400">
                {xds
                  ? `${xds.control_plane_uri} · Version ${xds.snapshot_version} (${xds.nonce})`
                  : 'Loading xDS snapshot...'}
              </p>
            </div>
            {ioUring && (
              <span className="text-[11px] font-mono px-2.5 py-1 rounded bg-emerald-950/60 border border-emerald-800/70 text-emerald-300">
                io_uring SQ/CQ: {ioUring.sq_entries}/{ioUring.cq_entries} · {formatBytes(ioUring.zero_copy_send_bytes)} ZC
              </span>
            )}
          </div>

          <div className="overflow-x-auto">
            <table className="w-full text-left border-collapse font-mono text-xs">
              <thead>
                <tr className="border-b border-slate-800 bg-slate-950/60 text-slate-400 text-[11px]">
                  <th className="px-4 py-2.5 font-medium">Region POP</th>
                  <th className="px-4 py-2.5 font-medium">Endpoint</th>
                  <th className="px-4 py-2.5 font-medium">Traffic Split</th>
                  <th className="px-4 py-2.5 font-medium">RTT</th>
                  <th className="px-4 py-2.5 font-medium text-right">Routed</th>
                </tr>
              </thead>
              <tbody>
                {(xds?.regions || []).map((reg) => (
                  <tr key={reg.region_id} className="border-b border-slate-800/60 hover:bg-slate-800/30">
                    <td className="px-4 py-2.5 text-emerald-400 font-bold">{reg.region_id}</td>
                    <td className="px-4 py-2.5 text-slate-300">{reg.endpoint}</td>
                    <td className="px-4 py-2.5">
                      <span className="text-slate-200">{reg.primary_pct}%</span>
                      <span className="text-slate-500"> / </span>
                      <span className="text-purple-300">{reg.canary_pct}% canary</span>
                    </td>
                    <td className="px-4 py-2.5 text-sky-300 tabular-nums">{reg.rtt_ms}ms</td>
                    <td className="px-4 py-2.5 text-right text-slate-200 tabular-nums">
                      {reg.routed_requests.toLocaleString()}
                    </td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
        </div>

        <div className="lg:col-span-5 bg-slate-900/70 border border-slate-800 rounded-xl p-5 space-y-4">
          <div className="flex items-center justify-between">
            <div>
              <h3 className="text-sm font-bold text-white">eBPF Flamegraph Hotspots (mod_profiler)</h3>
              <p className="text-[11px] font-mono text-slate-400">
                {profiler
                  ? `${profiler.total_samples.toLocaleString()} samples @ ${profiler.sample_freq_hz} Hz · ${profiler.alert_state}`
                  : 'Loading CPU stack profile...'}
              </p>
            </div>
            <span className="text-[10px] font-mono px-2 py-0.5 rounded bg-emerald-950 text-emerald-300 border border-emerald-800">
              {profiler?.alert_state || 'OK'}
            </span>
          </div>

          <div className="space-y-3">
            {(profiler?.hotspots || []).map((hs) => (
              <div key={hs.stack} className="space-y-1">
                <div className="flex items-center justify-between text-[11px] font-mono">
                  <span className="text-slate-300 truncate max-w-[290px]" title={hs.stack}>
                    {hs.stack}
                  </span>
                  <span className="text-emerald-400 font-bold tabular-nums">{hs.cpu_pct.toFixed(1)}%</span>
                </div>
                <div className="h-2 w-full rounded bg-slate-950 border border-slate-800 overflow-hidden">
                  <div
                    className="h-full bg-gradient-to-r from-emerald-500 to-sky-400 transition-all"
                    style={{ width: `${Math.min(100, hs.cpu_pct)}%` }}
                  />
                </div>
              </div>
            ))}
          </div>

          {ioUring && (
            <div className="pt-3 border-t border-slate-800 flex items-center justify-between text-[11px] font-mono text-slate-400">
              <span>io_uring Syscall Batches Saved</span>
              <span className="text-emerald-300 font-bold tabular-nums">
                {ioUring.syscall_batches_saved.toLocaleString()}
              </span>
            </div>
          )}
        </div>
      </div>
    </div>
  );
};
