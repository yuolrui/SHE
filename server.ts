import express from 'express';
import http from 'http';
import path from 'path';
import fs from 'fs';
import { spawn, exec, ChildProcess } from 'child_process';
import { fileURLToPath } from 'url';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

const app = express();
app.use(express.json());

const PORT = parseInt(process.env.PORT || '3000', 10);
const C_SERVER_PORT = 8899;
const C_SERVER_DIR = path.resolve(__dirname, 'c_server');
const C_SERVER_BIN = path.resolve(C_SERVER_DIR, 'c_lighttpd');
const C_SERVER_WWW = path.resolve(C_SERVER_DIR, 'www');

let cServerProcess: ChildProcess | null = null;
let cServerStartTime: number | null = null;
let cServerLogs: string[] = [];

function appendLog(line: string) {
  const timestamp = new Date().toISOString().substring(11, 19);
  cServerLogs.push(`[${timestamp}] ${line}`);
  if (cServerLogs.length > 200) {
    cServerLogs.shift();
  }
}

function ensureCompiled(): Promise<{ success: boolean; output: string }> {
  return new Promise((resolve) => {
    exec('which gcc || (apt-get update && apt-get install -y gcc make)', (installErr) => {
      exec('make -C ' + C_SERVER_DIR, (err, stdout, stderr) => {
        const output = (stdout || '') + '\n' + (stderr || '');
        if (fs.existsSync(C_SERVER_BIN)) {
          try {
            fs.chmodSync(C_SERVER_BIN, 0o755);
          } catch (e) {}
        }
        resolve({ success: !err && fs.existsSync(C_SERVER_BIN), output });
      });
    });
  });
}

function startCServerProcess(port = C_SERVER_PORT): Promise<boolean> {
  return new Promise((resolve) => {
    if (cServerProcess && !cServerProcess.killed) {
      return resolve(true);
    }

    if (fs.existsSync(C_SERVER_BIN)) {
      try {
        fs.chmodSync(C_SERVER_BIN, 0o755);
      } catch (e) {}
      launchProcess(port, resolve);
    } else {
      appendLog('Binary not found, compiling before starting...');
      ensureCompiled().then(({ success, output }) => {
        appendLog(output.trim());
        if (!success) return resolve(false);
        launchProcess(port, resolve);
      });
    }
  });
}

function launchProcess(port: number, resolve: (val: boolean) => void) {
  try {
    if (fs.existsSync(C_SERVER_BIN)) {
      try {
        fs.chmodSync(C_SERVER_BIN, 0o755);
      } catch (e) {}
    }
    cServerProcess = spawn(C_SERVER_BIN, ['-p', port.toString(), '-d', C_SERVER_WWW, '-v'], {
      cwd: C_SERVER_DIR,
    });

    cServerStartTime = Date.now();
    appendLog(`Spawned c_lighttpd PID ${cServerProcess.pid} on port ${port}`);

    cServerProcess.stdout?.on('data', (data) => {
      const str = data.toString();
      str.split('\n').filter((l: string) => l.trim()).forEach((l: string) => appendLog(l));
    });

    cServerProcess.stderr?.on('data', (data) => {
      const str = data.toString();
      str.split('\n').filter((l: string) => l.trim()).forEach((l: string) => appendLog(`[STDERR] ${l}`));
    });

    cServerProcess.on('error', (err) => {
      appendLog(`Process error: ${err.message}`);
      cServerProcess = null;
      cServerStartTime = null;
    });

    cServerProcess.on('exit', (code, signal) => {
      appendLog(`Process exited with code ${code}, signal ${signal}`);
      cServerProcess = null;
      cServerStartTime = null;
    });

    // Wait slightly for socket bind
    setTimeout(() => {
      resolve(true);
    }, 400);
  } catch (err: any) {
    appendLog(`Failed to spawn: ${err.message}`);
    resolve(false);
  }
}

function stopCServerProcess(): Promise<boolean> {
  return new Promise((resolve) => {
    if (!cServerProcess) return resolve(true);
    appendLog(`Sending SIGTERM to PID ${cServerProcess.pid}`);
    cServerProcess.kill('SIGTERM');

    const timeout = setTimeout(() => {
      if (cServerProcess && !cServerProcess.killed) {
        cServerProcess.kill('SIGKILL');
      }
      cServerProcess = null;
      cServerStartTime = null;
      resolve(true);
    }, 1500);

    cServerProcess.on('exit', () => {
      clearTimeout(timeout);
      cServerProcess = null;
      cServerStartTime = null;
      resolve(true);
    });
  });
}

// ----------------- API Endpoints -----------------

app.get('/api/c-server/status', (_req, res) => {
  const isRunning = Boolean(cServerProcess && !cServerProcess.killed);
  const binaryExists = fs.existsSync(C_SERVER_BIN);
  const uptime = isRunning && cServerStartTime ? Math.floor((Date.now() - cServerStartTime) / 1000) : 0;

  res.json({
    isRunning,
    binaryExists,
    pid: isRunning ? cServerProcess?.pid : null,
    port: C_SERVER_PORT,
    uptime,
    docRoot: C_SERVER_WWW,
    logs: cServerLogs.slice(-50),
  });
});

app.post('/api/c-server/start', async (_req, res) => {
  const success = await startCServerProcess();
  res.json({
    success,
    isRunning: Boolean(cServerProcess && !cServerProcess.killed),
    pid: cServerProcess?.pid || null,
  });
});

app.post('/api/c-server/stop', async (_req, res) => {
  await stopCServerProcess();
  res.json({ success: true, isRunning: false });
});

app.post('/api/c-server/compile', (_req, res) => {
  const startTime = Date.now();
  appendLog('Executing: make clean && make');

  exec('make clean && make', { cwd: C_SERVER_DIR }, (err, stdout, stderr) => {
    const elapsed = Date.now() - startTime;
    const output = (stdout || '') + '\n' + (stderr || '');
    const success = !err;

    appendLog(`Compile ${success ? 'SUCCEEDED' : 'FAILED'} in ${elapsed}ms`);

    let binarySize = 0;
    if (fs.existsSync(C_SERVER_BIN)) {
      binarySize = fs.statSync(C_SERVER_BIN).size;
    }

    res.json({
      success,
      output,
      elapsed,
      binarySize,
      exitCode: err ? (err as any).code || 1 : 0,
    });
  });
});

app.post('/api/c-server/request', async (req, res) => {
  const { method = 'GET', path: reqPath = '/', headers = {}, body = '' } = req.body;

  // Auto-start if stopped
  if (!cServerProcess || cServerProcess.killed) {
    await startCServerProcess();
  }

  const startTime = process.hrtime();

  const options: http.RequestOptions = {
    hostname: '127.0.0.1',
    port: C_SERVER_PORT,
    path: reqPath,
    method: method.toUpperCase(),
    headers: {
      'User-Agent': 'c-lighttpd-workbench/1.0',
      ...headers,
    },
    timeout: 3000,
  };

  const clientReq = http.request(options, (clientRes) => {
    const chunks: Buffer[] = [];

    clientRes.on('data', (chunk) => {
      chunks.push(chunk);
    });

    clientRes.on('end', () => {
      const diff = process.hrtime(startTime);
      const latencyMs = Number((diff[0] * 1000 + diff[1] / 1e6).toFixed(2));
      const bodyBuffer = Buffer.concat(chunks);
      const isText = (clientRes.headers['content-type'] || '').includes('text') ||
                     (clientRes.headers['content-type'] || '').includes('json') ||
                     (clientRes.headers['content-type'] || '').includes('javascript');

      res.json({
        statusCode: clientRes.statusCode,
        statusMessage: clientRes.statusMessage,
        headers: clientRes.headers,
        body: isText ? bodyBuffer.toString('utf-8') : `[Binary stream: ${bodyBuffer.length} bytes]`,
        bodyLength: bodyBuffer.length,
        latencyMs,
      });
    });
  });

  clientReq.on('error', (err) => {
    res.status(502).json({
      error: `Could not connect to c_lighttpd on port ${C_SERVER_PORT}: ${err.message}`,
    });
  });

  if (body && (method === 'POST' || method === 'PUT')) {
    clientReq.write(body);
  }
  clientReq.end();
});

app.post('/api/c-server/benchmark', async (req, res) => {
  const totalRequests = Math.min(Math.max(parseInt(req.body.requests || '100', 10), 10), 1000);
  const concurrency = Math.min(Math.max(parseInt(req.body.concurrency || '10', 10), 1), 50);
  const targetPath = req.body.path || '/index.html';

  if (!cServerProcess || cServerProcess.killed) {
    await startCServerProcess();
  }

  const latencies: number[] = [];
  let successful = 0;
  let failed = 0;
  let totalBytes = 0;
  let currentReq = 0;

  const agent = new http.Agent({
    keepAlive: true,
    maxSockets: concurrency,
  });

  const benchmarkStart = Date.now();

  function makeRequest(): Promise<void> {
    return new Promise((resolve) => {
      const reqStart = process.hrtime();
      const clientReq = http.request(
        {
          hostname: '127.0.0.1',
          port: C_SERVER_PORT,
          path: targetPath,
          method: 'GET',
          agent,
          timeout: 2000,
        },
        (clientRes) => {
          let bytes = 0;
          clientRes.on('data', (chunk) => {
            bytes += chunk.length;
          });
          clientRes.on('end', () => {
            const diff = process.hrtime(reqStart);
            const lat = diff[0] * 1000 + diff[1] / 1e6;
            latencies.push(lat);
            successful++;
            totalBytes += bytes;
            resolve();
          });
        }
      );

      clientReq.on('error', () => {
        failed++;
        resolve();
      });

      clientReq.end();
    });
  }

  // Run concurrency workers
  const workers = Array.from({ length: concurrency }, async () => {
    while (currentReq < totalRequests) {
      currentReq++;
      await makeRequest();
    }
  });

  await Promise.all(workers);
  const benchmarkElapsed = (Date.now() - benchmarkStart) / 1000;

  latencies.sort((a, b) => a - b);
  const avg = latencies.length ? latencies.reduce((a, b) => a + b, 0) / latencies.length : 0;
  const p50 = latencies.length ? latencies[Math.floor(latencies.length * 0.5)] : 0;
  const p95 = latencies.length ? latencies[Math.floor(latencies.length * 0.95)] : 0;
  const min = latencies.length ? latencies[0] : 0;
  const max = latencies.length ? latencies[latencies.length - 1] : 0;
  const rps = benchmarkElapsed > 0 ? Number((successful / benchmarkElapsed).toFixed(1)) : 0;

  res.json({
    totalRequests,
    concurrency,
    successful,
    failed,
    totalBytes,
    durationSeconds: Number(benchmarkElapsed.toFixed(3)),
    rps,
    latency: {
      avg: Number(avg.toFixed(2)),
      p50: Number(p50.toFixed(2)),
      p95: Number(p95.toFixed(2)),
      min: Number(min.toFixed(2)),
      max: Number(max.toFixed(2)),
    },
  });
});

app.get('/api/c-server/files', (_req, res) => {
  const fileEntries = [
    { folder: 'core', relPath: 'src/core/server.h', name: 'server.h' },
    { folder: 'core', relPath: 'src/core/server.c', name: 'server.c' },
    { folder: 'core', relPath: 'src/core/connection.h', name: 'connection.h' },
    { folder: 'core', relPath: 'src/core/connection.c', name: 'connection.c' },
    { folder: 'core', relPath: 'src/core/chunkqueue.h', name: 'chunkqueue.h' },
    { folder: 'core', relPath: 'src/core/chunkqueue.c', name: 'chunkqueue.c' },
    { folder: 'core', relPath: 'src/core/buffer.h', name: 'buffer.h' },
    { folder: 'core', relPath: 'src/core/buffer.c', name: 'buffer.c' },
    { folder: 'event', relPath: 'src/event/fdevent.h', name: 'fdevent.h' },
    { folder: 'event', relPath: 'src/event/fdevent.c', name: 'fdevent.c' },
    { folder: 'net', relPath: 'src/net/network.h', name: 'network.h' },
    { folder: 'net', relPath: 'src/net/network.c', name: 'network.c' },
    { folder: 'http', relPath: 'src/http/http_parser.h', name: 'http_parser.h' },
    { folder: 'http', relPath: 'src/http/http_parser.c', name: 'http_parser.c' },
    { folder: 'http', relPath: 'src/http/mime_types.h', name: 'mime_types.h' },
    { folder: 'http', relPath: 'src/http/mime_types.c', name: 'mime_types.c' },
    { folder: 'modules', relPath: 'src/modules/plugin.h', name: 'plugin.h' },
    { folder: 'modules', relPath: 'src/modules/plugin.c', name: 'plugin.c' },
    { folder: 'modules', relPath: 'src/modules/mod_status.h', name: 'mod_status.h' },
    { folder: 'modules', relPath: 'src/modules/mod_status.c', name: 'mod_status.c' },
    { folder: 'modules', relPath: 'src/modules/mod_indexfile.h', name: 'mod_indexfile.h' },
    { folder: 'modules', relPath: 'src/modules/mod_indexfile.c', name: 'mod_indexfile.c' },
    { folder: 'modules', relPath: 'src/modules/mod_dirlisting.h', name: 'mod_dirlisting.h' },
    { folder: 'modules', relPath: 'src/modules/mod_dirlisting.c', name: 'mod_dirlisting.c' },
    { folder: 'modules', relPath: 'src/modules/mod_accesslog.h', name: 'mod_accesslog.h' },
    { folder: 'modules', relPath: 'src/modules/mod_accesslog.c', name: 'mod_accesslog.c' },
    { folder: 'main', relPath: 'src/main.c', name: 'main.c' },
    { folder: 'build', relPath: 'Makefile', name: 'Makefile' },
  ];

  const files = fileEntries.map(({ folder, relPath, name }) => {
    const filePath = path.join(C_SERVER_DIR, relPath);
    if (!fs.existsSync(filePath)) {
      return { path: relPath, name, folder, content: '', lines: 0, size: 0 };
    }
    const content = fs.readFileSync(filePath, 'utf-8');
    return {
      path: relPath,
      name,
      folder,
      content,
      lines: content.split('\n').length,
      size: Buffer.byteLength(content, 'utf-8'),
    };
  });

  res.json({ files });
});

app.get('/api/c-server/logs/access', (_req, res) => {
  const logPaths = [
    path.join(C_SERVER_DIR, 'logs', 'access.log'),
    path.join(C_SERVER_DIR, 'access.log'),
  ];
  let logContent = '';
  for (const p of logPaths) {
    if (fs.existsSync(p)) {
      try {
        logContent = fs.readFileSync(p, 'utf-8');
        break;
      } catch (e) {}
    }
  }

  const lines = logContent
    .split('\n')
    .filter((l) => l.trim().length > 0)
    .slice(-100);

  res.json({ lines, count: lines.length });
});

app.get('/api/c-server/plugins', (_req, res) => {
  res.json({
    plugins: [
      {
        name: 'mod_status',
        version: '1.0',
        hooks: ['handle_subrequest_start'],
        desc: 'Internal runtime performance metrics & online HTML/JSON status monitor (/server-status)',
        active: true,
      },
      {
        name: 'mod_indexfile',
        version: '1.0',
        hooks: ['handle_subrequest_start'],
        desc: 'Configurable fallback cascade for index files (index.html, index.htm, default.html)',
        active: true,
      },
      {
        name: 'mod_dirlisting',
        version: '1.0',
        hooks: ['handle_subrequest_start'],
        desc: 'Automatic interactive HTML directory indexing for directories without index files',
        active: true,
      },
      {
        name: 'mod_accesslog',
        version: '1.0',
        hooks: ['init', 'handle_response_done', 'cleanup'],
        desc: 'Apache / Lighttpd standard Combined Log Format (CLF) request logging to logs/access.log',
        active: true,
      },
    ],
  });
});

app.get('/api/c-server/download', (_req, res) => {
  exec('tar -czf - -C ' + path.dirname(C_SERVER_DIR) + ' c_server', { encoding: 'buffer', maxBuffer: 10 * 1024 * 1024 }, (err, stdout) => {
    if (err) {
      return res.status(500).send('Error creating tarball');
    }
    res.setHeader('Content-Type', 'application/gzip');
    res.setHeader('Content-Disposition', 'attachment; filename="c-lighttpd-server.tar.gz"');
    res.send(stdout);
  });
});

// ----------------- Vite Integration -----------------

async function startServer() {
  // Start C server immediately if binary already exists, or compile first
  if (fs.existsSync(C_SERVER_BIN)) {
    startCServerProcess();
  } else {
    ensureCompiled().then(({ success }) => {
      if (success) {
        startCServerProcess();
      }
    });
  }

  if (process.env.NODE_ENV === 'production') {
    const distPath = path.resolve(__dirname, 'dist');
    app.use(express.static(distPath));
    app.get('*', (_req, res) => {
      res.sendFile(path.resolve(distPath, 'index.html'));
    });
  } else {
    const { createServer: createViteServer } = await import('vite');
    const vite = await createViteServer({
      server: { middlewareMode: true },
      appType: 'spa',
    });
    app.use(vite.middlewares);
  }

  app.listen(PORT, '0.0.0.0', () => {
    console.log(`Web application running on http://0.0.0.0:${PORT}`);
  });
}

startServer();
