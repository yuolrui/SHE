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

interface DpuFlow {
  flow_id: number;
  policy: string;
  packets: number;
  hw_active: boolean;
}

interface DpuSnapshot {
  dpu_device: string;
  arm_cores: number;
  waf_regex_rules: number;
  total_flows_offloaded: number;
  total_packets_fastpathed: number;
  flows: DpuFlow[];
}

interface ZkSnapshot {
  circuit_id: string;
  curve: string;
  proofs_generated: number;
  proofs_verified: number;
  avg_verify_us: number;
  last_claim: string;
  last_commitment_hex: string;
}

interface RdmaQp {
  qp_num: number;
  remote_rack: string;
  remote_rkey: string;
  dma_bytes: number;
  wr_posted: number;
}

interface RdmaSnapshot {
  hca_device: string;
  transport: string;
  link_gbps: number;
  total_wr_completed: number;
  total_dma_bytes: number;
  qps: RdmaQp[];
}

interface CpoLane {
  lane_id: number;
  wavelength_nm: number;
  tx_power_dbm: number;
  gbps: number;
  frames_switched: number;
  locked: boolean;
}

interface CpoSnapshot {
  fabric_name: string;
  total_lanes: number;
  aggregate_gbps: number;
  tuning_ops: number;
  avg_ber_exponent: number;
  lanes: CpoLane[];
}

interface BbrFlow {
  flow_id: number;
  phase: string;
  min_rtt_us: number;
  btlbw_mbps: number;
  pacing_gain: number;
  pacing_rate_bps: number;
  loss_rate_pct: number;
}

interface BbrSnapshot {
  algorithm: string;
  active_flows: number;
  total_pacing_updates: number;
  microbursts_smoothed: number;
  flows: BbrFlow[];
}

interface FormalSnapshot {
  solver_engine: string;
  invariants_checked: number;
  violations_detected: number;
  proof_runs: number;
  last_verdict: string;
  last_proof_digest: string;
}

interface AfxdpRing {
  ifname: string;
  queue_id: number;
  umem_frames: number;
  rx_packets: number;
  tx_packets: number;
  recycled: number;
  zero_copy: boolean;
}

interface AfxdpSnapshot {
  mode: string;
  frame_size: number;
  total_umem_frames: number;
  total_zero_copy_pkts: number;
  rings: AfxdpRing[];
}

interface FedCluster {
  cluster_id: string;
  region: string;
  status: string;
  sla_ms: number;
  epoch: number;
  policy_digest: string;
}

interface FederationSnapshot {
  mesh_id: string;
  global_epoch: number;
  failovers_executed: number;
  clusters: FedCluster[];
}

interface ReplayEvent {
  seq: number;
  method: string;
  path: string;
  status: number;
  trace_hash: string;
}

interface ReplaySnapshot {
  engine_name: string;
  events_recorded: number;
  replays_verified: number;
  recent_events: ReplayEvent[];
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
  const [dpu, setDpu] = useState<DpuSnapshot | null>(null);
  const [zk, setZk] = useState<ZkSnapshot | null>(null);
  const [rdma, setRdma] = useState<RdmaSnapshot | null>(null);
  const [cpo, setCpo] = useState<CpoSnapshot | null>(null);
  const [bbr, setBbr] = useState<BbrSnapshot | null>(null);
  const [formal, setFormal] = useState<FormalSnapshot | null>(null);
  const [afxdp, setAfxdp] = useState<AfxdpSnapshot | null>(null);
  const [federation, setFederation] = useState<FederationSnapshot | null>(null);
  const [replay, setReplay] = useState<ReplaySnapshot | null>(null);
  const [history, setHistory] = useState<TelemetryPoint[]>([]);
  const [autoRefresh, setAutoRefresh] = useState<boolean>(true);
  const [isRefreshing, setIsRefreshing] = useState<boolean>(false);
  const [actionBanner, setActionBanner] = useState<string | null>(null);
  const [isActionRunning, setIsActionRunning] = useState<boolean>(false);
  const [lastUpdated, setLastUpdated] = useState<string>('--:--:--');

  const fetchAllTelemetry = useCallback(async () => {
    setIsRefreshing(true);
    try {
      const [mRes, shRes, sbRes, ppRes, mtRes, cmRes, urRes, xdsRes, prRes, dpuRes, zkRes, rdmaRes, cpoRes, bbrRes, fmRes, afxRes, fedRes, repRes] = await Promise.all([
        queryCEngine<MetricsSnapshot>('/metrics-json'),
        queryCEngine<SelfHealSnapshot>('/self-heal-status?json=1'),
        queryCEngine<SandboxSnapshot>('/sandbox-status?json=1'),
        queryCEngine<ProxyPoolSnapshot>('/proxy-pool-status?json=1'),
        queryCEngine<MtlsSnapshot>('/mtls-status?json=1'),
        queryCEngine<CloudMeshSnapshot>('/cloud-mesh-status'),
        queryCEngine<IoUringSnapshot>('/uring-status?json=1'),
        queryCEngine<XdsSnapshot>('/xds-status?json=1'),
        queryCEngine<ProfilerSnapshot>('/profiler-status?json=1'),
        queryCEngine<DpuSnapshot>('/dpu-status?json=1'),
        queryCEngine<ZkSnapshot>('/zk-status?json=1'),
        queryCEngine<RdmaSnapshot>('/rdma-status?json=1'),
        queryCEngine<CpoSnapshot>('/cpo-status?json=1'),
        queryCEngine<BbrSnapshot>('/bbr-status?json=1'),
        queryCEngine<FormalSnapshot>('/formal-status?json=1'),
        queryCEngine<AfxdpSnapshot>('/afxdp-status?json=1'),
        queryCEngine<FederationSnapshot>('/federation-status?json=1'),
        queryCEngine<ReplaySnapshot>('/replay-status?json=1'),
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
      if (dpuRes.data) setDpu(dpuRes.data);
      if (zkRes.data) setZk(zkRes.data);
      if (rdmaRes.data) setRdma(rdmaRes.data);
      if (cpoRes.data) setCpo(cpoRes.data);
      if (bbrRes.data) setBbr(bbrRes.data);
      if (fmRes.data) setFormal(fmRes.data);
      if (afxRes.data) setAfxdp(afxRes.data);
      if (fedRes.data) setFederation(fedRes.data);
      if (repRes.data) setReplay(repRes.data);
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

  const handleRunDpuZkRdma = async () => {
    setIsActionRunning(true);
    setActionBanner(null);
    try {
      const [dpuRes, zkRes, rdmaRes] = await Promise.all([
        queryCEngine<{ nic_flow_id: number; total_flows_offloaded: number }>('/dpu/offload?flow_id=1024&policy=strict-waf'),
        queryCEngine<{ zk_commitment_sha256: string; verify_time_us: number }>('/zk/prove?claim=eu-gdpr-sovereign-tenant'),
        queryCEngine<{ qp_num: number; dma_bytes_posted: number }>('/rdma/post-send?qp=201&bytes=1048576'),
      ]);
      if (dpuRes.data && zkRes.data && rdmaRes.data) {
        setActionBanner(
          `DOCA Flow #${dpuRes.data.nic_flow_id} offloaded · Groth16 ZK proved (${zkRes.data.verify_time_us}us) · RDMA QP#${rdmaRes.data.qp_num} DMA +${formatBytes(rdmaRes.data.dma_bytes_posted)}`
        );
      }
      await fetchAllTelemetry();
    } finally {
      setIsActionRunning(false);
    }
  };

  const handleRunCpoBbrFormal = async () => {
    setIsActionRunning(true);
    setActionBanner(null);
    try {
      const [cpoRes, bbrRes, fmRes] = await Promise.all([
        queryCEngine<{ lane_id: number; aggregate_gbps: number; target_power_dbm: number }>('/cpo/tune?lane=1&power=-2'),
        queryCEngine<{ flow_id: number; phase: string; pacing_rate_bps: number }>('/bbr/pace?flow=101&rtt_us=180&bw_mbps=40000'),
        queryCEngine<{ status: string; rules_verified: number; violations_detected: number }>('/formal/verify'),
      ]);
      if (cpoRes.data && bbrRes.data && fmRes.data) {
        setActionBanner(
          `CPO Lane #${cpoRes.data.lane_id} tuned (${cpoRes.data.aggregate_gbps} Gbps) · BBRv3 ${bbrRes.data.phase} (${(bbrRes.data.pacing_rate_bps / 1e9).toFixed(1)} Gbps) · Formal FSM ${fmRes.data.status} (${fmRes.data.rules_verified} rules, ${fmRes.data.violations_detected} violations)`
        );
      }
      await fetchAllTelemetry();
    } finally {
      setIsActionRunning(false);
    }
  };

  const handleRunAfxdpFedReplay = async () => {
    setIsActionRunning(true);
    setActionBanner(null);
    try {
      const [afxRes, fedRes, repRes] = await Promise.all([
        queryCEngine<{ ifname: string; queue_id: number; umem_frames: number }>('/afxdp/bind?ifname=eth0&queue=0'),
        queryCEngine<{ cluster_id: string; global_epoch: number }>('/federation/sync?cluster=eu-west-mesh&policy=zero-trust-strict'),
        queryCEngine<{ seq_verified: number; deterministic_hash: string }>('/replay/verify'),
      ]);
      if (afxRes.data && fedRes.data && repRes.data) {
        setActionBanner(
          `AF_XDP ${afxRes.data.ifname}:Q${afxRes.data.queue_id} (${afxRes.data.umem_frames} UMEM frames) · Federation ${fedRes.data.cluster_id} Epoch #${fedRes.data.global_epoch} · Replay #${repRes.data.seq_verified} (${repRes.data.deterministic_hash})`
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
            onClick={handleRunDpuZkRdma}
            disabled={isActionRunning}
            className="flex items-center gap-1.5 px-3 py-1.5 text-xs font-semibold text-amber-950 bg-amber-400 hover:bg-amber-300 rounded-md transition-colors disabled:opacity-50"
          >
            <Cpu className="w-3.5 h-3.5" />
            <span>DPU · ZK · RDMA</span>
          </button>

          <button
            onClick={handleRunCpoBbrFormal}
            disabled={isActionRunning}
            className="flex items-center gap-1.5 px-3 py-1.5 text-xs font-semibold text-cyan-950 bg-cyan-400 hover:bg-cyan-300 rounded-md transition-colors disabled:opacity-50"
          >
            <ShieldCheck className="w-3.5 h-3.5" />
            <span>CPO · BBRv3 · Formal</span>
          </button>

          <button
            onClick={handleRunAfxdpFedReplay}
            disabled={isActionRunning}
            className="flex items-center gap-1.5 px-3 py-1.5 text-xs font-semibold text-indigo-950 bg-indigo-400 hover:bg-indigo-300 rounded-md transition-colors disabled:opacity-50"
          >
            <Network className="w-3.5 h-3.5" />
            <span>AF_XDP · Fed · Replay</span>
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

      {/* Phase 16: Hardware SmartNIC DPU, Zero-Knowledge Attestation & RDMA RoCEv2 */}
      <div className="grid grid-cols-1 lg:grid-cols-12 gap-6">
        <div className="lg:col-span-6 bg-slate-900/70 border border-slate-800 rounded-xl p-5 space-y-4">
          <div className="flex items-center justify-between">
            <div>
              <h3 className="text-sm font-bold text-white">
                SmartNIC DPU Offload & ZK Compliance (mod_dpu_offload + mod_zk_attest)
              </h3>
              <p className="text-[11px] font-mono text-slate-400">
                {dpu ? `${dpu.dpu_device} · ${dpu.arm_cores} ARM cores · ${dpu.waf_regex_rules} WAF regex rules` : 'Loading DPU telemetry...'}
              </p>
            </div>
            <span className="text-[10px] font-mono px-2 py-0.5 rounded bg-amber-950 text-amber-300 border border-amber-800">
              {dpu ? `${dpu.total_flows_offloaded.toLocaleString()} HW Flows` : '--'}
            </span>
          </div>

          <div className="grid grid-cols-2 gap-3 text-xs font-mono">
            <div className="bg-slate-950/80 border border-slate-800 rounded-lg p-3 space-y-1">
              <span className="text-slate-400 text-[11px]">Packets HW-Fastpathed</span>
              <div className="text-base font-bold text-emerald-400 tabular-nums">
                {dpu ? dpu.total_packets_fastpathed.toLocaleString() : '--'}
              </div>
            </div>
            <div className="bg-slate-950/80 border border-slate-800 rounded-lg p-3 space-y-1">
              <span className="text-slate-400 text-[11px]">ZK Proofs Verified ({zk?.avg_verify_us ?? 0} us)</span>
              <div className="text-base font-bold text-sky-400 tabular-nums">
                {zk ? zk.proofs_verified.toLocaleString() : '--'}
              </div>
            </div>
          </div>

          <div className="bg-slate-950/80 border border-slate-800 rounded-lg p-3 space-y-1.5 text-xs font-mono">
            <div className="flex items-center justify-between">
              <span className="text-slate-400">Circuit / Claim</span>
              <span className="text-purple-300">{zk ? `${zk.circuit_id} · ${zk.last_claim}` : '--'}</span>
            </div>
            <div className="flex items-center justify-between gap-2">
              <span className="text-slate-400 shrink-0">SHA-256 Commitment</span>
              <span className="text-emerald-300 truncate" title={zk?.last_commitment_hex}>
                {zk?.last_commitment_hex || '--'}
              </span>
            </div>
          </div>
        </div>

        <div className="lg:col-span-6 bg-slate-900/70 border border-slate-800 rounded-xl overflow-hidden">
          <div className="px-5 py-3.5 bg-slate-900 border-b border-slate-800 flex items-center justify-between">
            <div>
              <h3 className="text-sm font-bold text-white">RDMA RoCEv2 Cross-Rack DMA (mod_rdma)</h3>
              <p className="text-[11px] font-mono text-slate-400">
                {rdma ? `${rdma.hca_device} · ${rdma.transport} @ ${rdma.link_gbps} Gbps` : 'Loading RDMA HCA...'}
              </p>
            </div>
            <span className="text-[11px] font-mono text-emerald-300 font-bold tabular-nums">
              {rdma ? `${formatBytes(rdma.total_dma_bytes)} DMA` : '--'}
            </span>
          </div>

          <div className="overflow-x-auto">
            <table className="w-full text-left border-collapse font-mono text-xs">
              <thead>
                <tr className="border-b border-slate-800 bg-slate-950/60 text-slate-400 text-[11px]">
                  <th className="px-4 py-2 font-medium">QP</th>
                  <th className="px-4 py-2 font-medium">Remote Rack</th>
                  <th className="px-4 py-2 font-medium">Remote Key</th>
                  <th className="px-4 py-2 font-medium text-right">WRs</th>
                  <th className="px-4 py-2 font-medium text-right">DMA Sent</th>
                </tr>
              </thead>
              <tbody>
                {(rdma?.qps || []).map((qp) => (
                  <tr key={qp.qp_num} className="border-b border-slate-800/60 hover:bg-slate-800/30">
                    <td className="px-4 py-2 text-amber-300 font-bold">#{qp.qp_num}</td>
                    <td className="px-4 py-2 text-slate-200">{qp.remote_rack}</td>
                    <td className="px-4 py-2 text-slate-400">{qp.remote_rkey}</td>
                    <td className="px-4 py-2 text-right text-slate-300 tabular-nums">{qp.wr_posted.toLocaleString()}</td>
                    <td className="px-4 py-2 text-right text-emerald-300 tabular-nums">{formatBytes(qp.dma_bytes)}</td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
        </div>
      </div>

      {/* Phase 17: Exascale Co-Packaged Optics, BBRv3 Neural Pacing & Formal Proof Kernel */}
      <div className="grid grid-cols-1 lg:grid-cols-12 gap-6">
        <div className="lg:col-span-6 bg-slate-900/70 border border-slate-800 rounded-xl p-5 space-y-4">
          <div className="flex items-center justify-between">
            <div>
              <h3 className="text-sm font-bold text-white">
                Co-Packaged Optics & Formal Proof Kernel (mod_cpo + mod_formal_fsm)
              </h3>
              <p className="text-[11px] font-mono text-slate-400">
                {cpo ? `${cpo.fabric_name} · ${cpo.total_lanes}x400G Lanes · BER 10^(${cpo.avg_ber_exponent})` : 'Loading CPO silicon photonics...'}
              </p>
            </div>
            <span className="text-[10px] font-mono px-2 py-0.5 rounded bg-cyan-950 text-cyan-300 border border-cyan-800">
              {cpo ? `${cpo.aggregate_gbps} Gbps` : '--'}
            </span>
          </div>

          <div className="grid grid-cols-2 gap-3 text-xs font-mono">
            <div className="bg-slate-950/80 border border-slate-800 rounded-lg p-3 space-y-1">
              <span className="text-slate-400 text-[11px]">FSM Invariants Verified</span>
              <div className="text-base font-bold text-emerald-400 tabular-nums">
                {formal ? formal.invariants_checked.toLocaleString() : '--'}
              </div>
              <span className="text-[10px] text-slate-500">
                {formal ? `${formal.violations_detected} violations · ${formal.proof_runs.toLocaleString()} QED runs` : ''}
              </span>
            </div>
            <div className="bg-slate-950/80 border border-slate-800 rounded-lg p-3 space-y-1">
              <span className="text-slate-400 text-[11px]">Optical Lane Tuning Ops</span>
              <div className="text-base font-bold text-cyan-400 tabular-nums">
                {cpo ? cpo.tuning_ops.toLocaleString() : '--'}
              </div>
              <span className="text-[10px] text-slate-500">
                {formal ? formal.last_verdict : 'Awaiting solver...'}
              </span>
            </div>
          </div>

          <div className="bg-slate-950/80 border border-slate-800 rounded-lg p-3 space-y-1.5 text-xs font-mono">
            <div className="flex items-center justify-between">
              <span className="text-slate-400">Solver Engine</span>
              <span className="text-cyan-300">{formal?.solver_engine || '--'}</span>
            </div>
            <div className="flex items-center justify-between gap-2">
              <span className="text-slate-400 shrink-0">Proof Digest SHA-256</span>
              <span className="text-emerald-300 truncate" title={formal?.last_proof_digest}>
                {formal?.last_proof_digest || '--'}
              </span>
            </div>
          </div>
        </div>

        <div className="lg:col-span-6 bg-slate-900/70 border border-slate-800 rounded-xl overflow-hidden">
          <div className="px-5 py-3.5 bg-slate-900 border-b border-slate-800 flex items-center justify-between">
            <div>
              <h3 className="text-sm font-bold text-white">Autonomous eBPF BBRv3 Neural Pacing (mod_bbrv3)</h3>
              <p className="text-[11px] font-mono text-slate-400">
                {bbr ? `${bbr.algorithm} · ${bbr.total_pacing_updates.toLocaleString()} updates` : 'Loading BBRv3 telemetry...'}
              </p>
            </div>
            <span className="text-[11px] font-mono text-cyan-300 font-bold tabular-nums">
              {bbr ? `${bbr.microbursts_smoothed.toLocaleString()} bursts smoothed` : '--'}
            </span>
          </div>

          <div className="overflow-x-auto">
            <table className="w-full text-left border-collapse font-mono text-xs">
              <thead>
                <tr className="border-b border-slate-800 bg-slate-950/60 text-slate-400 text-[11px]">
                  <th className="px-4 py-2 font-medium">Flow</th>
                  <th className="px-4 py-2 font-medium">State Phase</th>
                  <th className="px-4 py-2 font-medium">MinRTT</th>
                  <th className="px-4 py-2 font-medium text-right">Gain</th>
                  <th className="px-4 py-2 font-medium text-right">Pacing Rate</th>
                </tr>
              </thead>
              <tbody>
                {(bbr?.flows || []).map((fl) => (
                  <tr key={fl.flow_id} className="border-b border-slate-800/60 hover:bg-slate-800/30">
                    <td className="px-4 py-2 text-cyan-300 font-bold">#{fl.flow_id}</td>
                    <td className="px-4 py-2 text-slate-200">{fl.phase}</td>
                    <td className="px-4 py-2 text-sky-300 tabular-nums">{fl.min_rtt_us} us</td>
                    <td className="px-4 py-2 text-right text-slate-300 tabular-nums">{fl.pacing_gain.toFixed(2)}x</td>
                    <td className="px-4 py-2 text-right text-emerald-300 tabular-nums">
                      {(fl.pacing_rate_bps / 1e9).toFixed(1)} Gbps
                    </td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
        </div>
      </div>

      {/* Phase 18: AF_XDP UMEM Zero-Copy, Multi-Cluster Federation & Deterministic Replay */}
      <div className="grid grid-cols-1 lg:grid-cols-12 gap-6">
        <div className="lg:col-span-6 bg-slate-900/70 border border-slate-800 rounded-xl p-5 space-y-4">
          <div className="flex items-center justify-between">
            <div>
              <h3 className="text-sm font-bold text-white">
                AF_XDP UMEM Ring & Replay Tracer (mod_afxdp + mod_replay)
              </h3>
              <p className="text-[11px] font-mono text-slate-400">
                {afxdp ? `${afxdp.mode} · ${afxdp.total_umem_frames.toLocaleString()} frames · ${afxdp.total_zero_copy_pkts.toLocaleString()} ZC pkts` : 'Loading AF_XDP UMEM...'}
              </p>
            </div>
            <span className="text-[10px] font-mono px-2 py-0.5 rounded bg-indigo-950 text-indigo-300 border border-indigo-800">
              {replay ? `${replay.events_recorded} Traced` : '--'}
            </span>
          </div>

          <div className="grid grid-cols-2 gap-3 text-xs font-mono">
            <div className="bg-slate-950/80 border border-slate-800 rounded-lg p-3 space-y-1">
              <span className="text-slate-400 text-[11px]">AF_XDP Zero-Copy Rings</span>
              <div className="text-base font-bold text-indigo-300 tabular-nums">
                {afxdp ? `${afxdp.rings.length} Active Queues` : '--'}
              </div>
              <span className="text-[10px] text-slate-500">
                {afxdp ? `${afxdp.frame_size}B aligned UMEM frames` : ''}
              </span>
            </div>
            <div className="bg-slate-950/80 border border-slate-800 rounded-lg p-3 space-y-1">
              <span className="text-slate-400 text-[11px]">Deterministic Replays Verified</span>
              <div className="text-base font-bold text-emerald-400 tabular-nums">
                {replay ? replay.replays_verified.toLocaleString() : '--'}
              </div>
              <span className="text-[10px] text-slate-500">
                {replay?.recent_events?.[replay.recent_events.length - 1]
                  ? `Latest: ${replay.recent_events[replay.recent_events.length - 1].path}`
                  : 'FNV1a-64 trace ring'}
              </span>
            </div>
          </div>
        </div>

        <div className="lg:col-span-6 bg-slate-900/70 border border-slate-800 rounded-xl overflow-hidden">
          <div className="px-5 py-3.5 bg-slate-900 border-b border-slate-800 flex items-center justify-between">
            <div>
              <h3 className="text-sm font-bold text-white">Multi-Cluster Policy Federation (mod_federation)</h3>
              <p className="text-[11px] font-mono text-slate-400">
                {federation ? `${federation.mesh_id} · Global Epoch #${federation.global_epoch}` : 'Loading Federation mesh...'}
              </p>
            </div>
            <span className="text-[11px] font-mono text-indigo-300 font-bold tabular-nums">
              {federation ? `${federation.failovers_executed} failovers` : '--'}
            </span>
          </div>

          <div className="overflow-x-auto">
            <table className="w-full text-left border-collapse font-mono text-xs">
              <thead>
                <tr className="border-b border-slate-800 bg-slate-950/60 text-slate-400 text-[11px]">
                  <th className="px-4 py-2 font-medium">Cluster</th>
                  <th className="px-4 py-2 font-medium">Region</th>
                  <th className="px-4 py-2 font-medium">SLA</th>
                  <th className="px-4 py-2 font-medium">Epoch</th>
                  <th className="px-4 py-2 font-medium">SHA-256 Policy</th>
                </tr>
              </thead>
              <tbody>
                {(federation?.clusters || []).map((cl) => (
                  <tr key={cl.cluster_id} className="border-b border-slate-800/60 hover:bg-slate-800/30">
                    <td className="px-4 py-2 text-indigo-300 font-bold">{cl.cluster_id}</td>
                    <td className="px-4 py-2 text-slate-200">{cl.region}</td>
                    <td className="px-4 py-2 text-emerald-400 tabular-nums">{cl.sla_ms}ms</td>
                    <td className="px-4 py-2 text-slate-300 tabular-nums">#{cl.epoch}</td>
                    <td className="px-4 py-2 text-slate-400 truncate max-w-[140px]" title={cl.policy_digest}>
                      {cl.policy_digest}
                    </td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
        </div>
      </div>
    </div>
  );
};
