import React, { useState } from 'react';
import { Globe, Search, Play, Loader2, CheckCircle, XCircle, Terminal } from 'lucide-react';
import { apiGetStatus } from '../../utils/api';

const NetworkTab = () => {
  const [host, setHost] = useState('');
  const [port, setPort] = useState('80');
  const [results, setResults] = useState<{ type: string, msg: string, success?: boolean }[]>([]);
  const [loading, setLoading] = useState(false);

  const addResult = (type: string, msg: string, success?: boolean) => {
    setResults(prev => [{ type, msg, success }, ...prev].slice(0, 50));
  };

  const runPing = async () => {
    if (!host) return;
    setLoading(true);
    addResult('PING', `Initiating ping request to ${host}...`);
    
    try {
      const res = await fetch(`/api/ping?host=${encodeURIComponent(host)}&token=${localStorage.getItem('session_token')}`);
      const data = await res.json();
      addResult('PING', data.message, data.success);
    } catch (err) {
      addResult('PING', 'Request failed', false);
    } finally {
      setLoading(false);
    }
  };

  const runDNS = async () => {
    if (!host) return;
    setLoading(true);
    addResult('DNS', `Looking up ${host}...`);
    
    try {
      const res = await fetch(`/api/nslookup?host=${encodeURIComponent(host)}&token=${localStorage.getItem('session_token')}`);
      const data = await res.json();
      addResult('DNS', data.message, data.success);
    } catch (err) {
      addResult('DNS', 'Lookup failed', false);
    } finally {
      setLoading(false);
    }
  };

  const runScan = async () => {
    if (!host || !port) return;
    setLoading(true);
    addResult('SCAN', `Scanning ${host}:${port}...`);
    
    try {
      const res = await fetch(`/api/scan?host=${encodeURIComponent(host)}&port=${port}&token=${localStorage.getItem('session_token')}`);
      const data = await res.json();
      addResult('SCAN', data.message, data.success);
    } catch (err) {
      addResult('SCAN', 'Scan failed', false);
    } finally {
      setLoading(false);
    }
  };

  return (
    <div className="space-y-4 animate-fade-up">
      <section className="section-card">
        <div className="flex items-center gap-2 mb-4">
          <div className="w-1.5 h-3.5 bg-accent rounded-full shadow-[0_0_10px_rgba(99,179,237,0.8)]" />
          <h2 className="text-[11px] font-extrabold tracking-widest text-accent uppercase italic flex items-center gap-2">
            <Globe size={12} /> NETWORK DIAGNOSTICS
          </h2>
        </div>

        <div className="space-y-4">
          <div className="flex gap-2">
            <div className="relative flex-1">
              <input 
                type="text" 
                placeholder="Hostname or IP" 
                value={host}
                onChange={(e) => setHost(e.target.value)}
                className="w-full bg-white/[0.03] border border-white/10 rounded-2xl px-5 py-4 text-xs font-bold text-white outline-none focus:border-accent/40 transition-all placeholder:text-gray-700"
              />
              <div className="absolute right-3 top-3">
                <button 
                  onClick={runDNS}
                  disabled={loading || !host}
                  className="p-2 rounded-xl bg-white/5 hover:bg-white/10 text-gray-400 hover:text-white transition-all disabled:opacity-50"
                  title="DNS Lookup"
                >
                  <Search size={16} />
                </button>
              </div>
            </div>
            <div className="w-24">
              <input 
                type="number" 
                placeholder="Port" 
                value={port}
                onChange={(e) => setPort(e.target.value)}
                className="w-full bg-white/[0.03] border border-white/10 rounded-2xl px-4 py-4 text-xs font-bold text-white outline-none focus:border-accent/40 transition-all placeholder:text-gray-700"
              />
            </div>
          </div>

          <div className="grid grid-cols-2 gap-3">
            <button 
              onClick={runPing}
              disabled={loading || !host}
              className="flex-1 py-4 rounded-2xl bg-accent text-bg font-black text-[10px] tracking-widest uppercase flex items-center justify-center gap-2 hover:brightness-110 active:scale-95 transition-all disabled:opacity-50"
            >
              {loading ? <Loader2 size={14} className="animate-spin" /> : <Play size={14} />} 
              Run Ping
            </button>
            <button 
              onClick={runScan}
              disabled={loading || !host || !port}
              className="flex-1 py-4 rounded-2xl bg-white/5 border border-white/10 text-accent font-black text-[10px] tracking-widest uppercase flex items-center justify-center gap-2 hover:bg-accent/10 active:scale-95 transition-all disabled:opacity-50"
            >
              {loading ? <Loader2 size={14} className="animate-spin" /> : <Search size={14} />} 
              Port Scan
            </button>
          </div>
        </div>
      </section>

      <section className="bg-[#050709] border border-white/10 rounded-2xl overflow-hidden shadow-2xl flex flex-col h-[400px]">
        <div className="bg-white/[0.03] px-4 py-3 border-b border-white/10 flex items-center justify-between">
          <div className="flex items-center gap-2 text-[10px] font-black text-gray-400 tracking-widest uppercase">
            <Terminal size={12} className="text-accent" /> OUTPUT CONSOLE
          </div>
          <button onClick={() => setResults([])} className="text-[8px] font-black text-gray-600 hover:text-red transition-colors uppercase">Clear</button>
        </div>

        <div className="flex-1 overflow-y-auto p-4 space-y-2 font-mono scrollbar-thin scrollbar-thumb-white/10">
          {results.map((r, i) => (
            <div key={i} className="flex gap-3 animate-in slide-in-from-left-2 duration-300">
              <span className="text-[9px] font-black text-gray-700 shrink-0 w-12">[{r.type}]</span>
              <div className="flex-1">
                <span className={`text-[10px] font-bold ${r.success === true ? 'text-green' : r.success === false ? 'text-red' : 'text-gray-400'}`}>
                  {r.msg}
                </span>
              </div>
              {r.success !== undefined && (
                <div className="shrink-0">
                  {r.success ? <CheckCircle size={10} className="text-green" /> : <XCircle size={10} className="text-red" />}
                </div>
              )}
            </div>
          ))}
          {results.length === 0 && (
            <div className="h-full flex items-center justify-center text-gray-800 text-[10px] font-black tracking-widest uppercase">
              Ready for Diagnostics
            </div>
          )}
        </div>
      </section>
    </div>
  );
};

export default NetworkTab;
