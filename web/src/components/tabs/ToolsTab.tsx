import React, { useState, useRef, useEffect } from "react";
import {
  Terminal,
  Play,
  Square,
  Globe,
  Shield,
  Activity,
  Trash2,
  Send,
  Search,
} from "lucide-react";
import { useStore } from "../../store/useStore";
import { apiPing } from "../../utils/api";

export const ToolsTab: React.FC = () => {
  const { pingResults, clearPingResults } = useStore();

  // Ping State
  const [target, setTarget] = useState("8.8.8.8");
  const [count, setCount] = useState(4);
  const [loading, setLoading] = useState(false);
  const scrollRef = useRef<HTMLDivElement>(null);

  // DNS State
  const [dnsTarget, setDnsTarget] = useState("google.com");
  const [dnsLoading, setDnsLoading] = useState(false);
  const [dnsResult, setDnsResult] = useState<string | null>(null);

  // Port Scanner State
  const [scanTarget, setScanTarget] = useState("192.168.1.1");
  const [scanPorts, setScanPorts] = useState("80,443,8080");

  // Auto-scroll to bottom of ping results
  useEffect(() => {
    if (scrollRef.current) {
      scrollRef.current.scrollTop = scrollRef.current.scrollHeight;
    }
  }, [pingResults]);

  const handleStartPing = async () => {
    if (!target) return;
    setLoading(true);
    clearPingResults();
    try {
      await apiPing(target, count);
    } catch (err) {
      console.error("Ping failed:", err);
    } finally {
      setLoading(false);
    }
  };

  const handleStopPing = async () => {
    try {
      await apiPing("", 0, true);
    } catch {}
  };

  const handleDnsLookup = async () => {
    if (!dnsTarget) return;
    setDnsLoading(true);
    setDnsResult(null);
    try {
      // Re-using the ping API endpoint slightly to force a DNS resolution
      // The ESP32's ping implementation resolves the IP before sending.
      // By sending 1 ping with a very short timeout, we can extract the resolved IP.
      await apiPing(dnsTarget, 1);
      setTimeout(() => {
        const latestResult = useStore
          .getState()
          .pingResults.find((r) => r.type === "result" && r.ip);
        if (latestResult && latestResult.ip) {
          setDnsResult(`Resolved ${dnsTarget} -> ${latestResult.ip}`);
        } else {
          setDnsResult(`Failed to resolve ${dnsTarget}. Host unknown.`);
        }
        setDnsLoading(false);
      }, 1500);
    } catch {
      setDnsResult("DNS Lookup request failed.");
      setDnsLoading(false);
    }
  };

  return (
    <div className="space-y-4 animate-fade-up">
      <section className="section-card">
        <div className="flex items-center gap-2 mb-4">
          <div className="w-1.5 h-3.5 bg-accent rounded-full shadow-[0_0_10px_rgba(99,179,237,0.8)]" />
          <h2 className="text-[11px] font-extrabold tracking-widest text-accent uppercase italic flex items-center gap-2">
            <Terminal size={12} /> NETWORK UTILITIES
          </h2>
        </div>

        <div className="space-y-6">
          {/* Top Row: DNS and Port Scanner UI */}
          <div className="grid grid-cols-1 lg:grid-cols-2 gap-4">
            {/* DNS Lookup Tool */}
            <div className="bg-black/20 border border-white/5 rounded-2xl p-5 space-y-4">
              <div className="flex items-center justify-between mb-2">
                <div className="flex items-center gap-2">
                  <Search size={14} className="text-[#f6e05e]" />
                  <span className="text-[10px] font-black text-gray-400 uppercase tracking-widest">
                    DNS LOOKUP
                  </span>
                </div>
              </div>
              <div className="space-y-1.5">
                <label className="text-[8px] font-black text-gray-600 uppercase ml-1">
                  Domain Name (mDNS supported)
                </label>
                <div className="flex gap-2">
                  <input
                    type="text"
                    value={dnsTarget}
                    onChange={(e) => setDnsTarget(e.target.value)}
                    placeholder="esp32.local"
                    className="flex-1 bg-black/40 border border-white/10 rounded-xl px-4 py-3 text-xs font-mono text-white outline-none focus:border-[#f6e05e]/40"
                  />
                </div>
                <button
                  onClick={handleDnsLookup}
                  disabled={dnsLoading || !dnsTarget}
                  className="px-4 py-3 rounded-xl bg-[#f6e05e]/10 border border-[#f6e05e]/30 text-[#f6e05e] font-black tracking-widest hover:bg-[#f6e05e]/20 transition-all disabled:opacity-50"
                >
                  {dnsLoading ? (
                    <div className="w-4 h-4 border-2 border-current border-t-transparent rounded-full animate-spin mx-auto" />
                  ) : (
                    "RESOLVE"
                  )}
                </button>
              </div>

              <div className="h-12 bg-black/40 border border-white/5 rounded-xl px-4 flex items-center">
                {dnsResult ? (
                  <span
                    className={`text-[10px] font-mono font-bold ${dnsResult.includes("Failed") ? "text-red/80" : "text-[#f6e05e]"}`}
                  >
                    {dnsResult}
                  </span>
                ) : (
                  <span className="text-[9px] font-bold text-gray-700 italic uppercase">
                    Result will appear here...
                  </span>
                )}
              </div>
            </div>

            {/* Port Scanner (UI Only for now) */}
            <div className="bg-black/20 border border-white/5 rounded-2xl p-5 space-y-4 opacity-50 grayscale pointer-events-none">
              <div className="flex items-center justify-between mb-2">
                <div className="flex items-center gap-2">
                  <Shield size={14} className="text-purple-400" />
                  <span className="text-[10px] font-black text-gray-400 uppercase tracking-widest">
                    TCP PORT SCANNER
                  </span>
                </div>
                <span className="text-[8px] font-black text-gray-500 uppercase tracking-tighter border border-gray-600 px-1.5 py-0.5 rounded">
                  SOON
                </span>
              </div>
              <div className="flex gap-2">
                <div className="flex-1 space-y-1.5">
                  <label className="text-[8px] font-black text-gray-600 uppercase ml-1">
                    Target IP
                  </label>
                  <input
                    type="text"
                    value={scanTarget}
                    readOnly
                    className="w-full bg-black/40 border border-white/10 rounded-xl px-4 py-3 text-xs font-mono text-gray-500 outline-none"
                  />
                </div>
                <div className="w-24 space-y-1.5">
                  <label className="text-[8px] font-black text-gray-600 uppercase ml-1">
                    Ports
                  </label>
                  <input
                    type="text"
                    value={scanPorts}
                    readOnly
                    className="w-full bg-black/40 border border-white/10 rounded-xl px-3 py-3 text-xs font-mono text-gray-500 outline-none"
                  />
                </div>
              </div>
              <button className="w-full py-3 rounded-xl bg-purple-500/10 border border-purple-500/30 text-purple-400 font-black tracking-widest uppercase">
                START SCAN
              </button>
            </div>
          </div>

          {/* Ping Tool */}
          <div className="bg-black/20 border border-white/5 rounded-2xl p-5 space-y-4">
            <div className="flex items-center gap-2 mb-2">
              <Globe size={14} className="text-accent" />
              <span className="text-[10px] font-black text-gray-400 uppercase tracking-widest">
                ICMP PING TOOL
              </span>
            </div>

            <div className="grid grid-cols-1 sm:grid-cols-3 gap-3">
              <div className="sm:col-span-2 space-y-1.5">
                <label className="text-[8px] font-black text-gray-600 uppercase ml-1">
                  Target Hostname / IP
                </label>
                <div className="relative">
                  <input
                    type="text"
                    value={target}
                    onChange={(e) => setTarget(e.target.value)}
                    placeholder="e.g. google.com or 8.8.8.8"
                    className="w-full bg-black/40 border border-white/10 rounded-xl px-4 py-3 text-xs font-mono text-white outline-none focus:border-accent/40"
                  />
                </div>
                <div className="absolute right-4 top-1/2 -translate-y-1/2 text-gray-700">
                  <Send size={14} />
                </div>
              </div>
              <div className="space-y-1.5">
                <label className="text-[8px] font-black text-gray-600 uppercase ml-1">
                  Count
                </label>
                <input
                  type="number"
                  min="1"
                  max="100"
                  value={count}
                  onChange={(e) => setCount(parseInt(e.target.value))}
                  className="w-full bg-black/40 border border-white/10 rounded-xl px-4 py-3 text-xs font-black text-white outline-none focus:border-accent/40"
                />
              </div>
            </div>

            <div className="flex gap-2">
              <button
                onClick={handleStartPing}
                disabled={loading || !target}
                className="flex-1 py-3.5 rounded-xl bg-accent/10 border border-accent/30 text-accent font-black text-[10px] tracking-[0.2em] uppercase hover:bg-accent/20 transition-all flex items-center justify-center gap-2 active:scale-[0.98] disabled:opacity-50"
              >
                {loading ? (
                  <div className="w-3 h-3 border-2 border-current border-t-transparent rounded-full animate-spin" />
                ) : (
                  <>
                    <Play size={12} fill="currentColor" /> EXECUTE PING
                  </>
                )}
              </button>
              <button
                onClick={handleStopPing}
                className="px-6 py-3.5 rounded-xl bg-red/5 border border-red/20 text-red font-black text-[10px] tracking-[0.2em] uppercase hover:bg-red/10 transition-all active:scale-[0.98]"
              >
                <Square size={12} fill="currentColor" />
              </button>
            </div>

            {/* Ping Output Console */}
            <div className="space-y-1.5">
              <div className="flex justify-between items-center px-1">
                <label className="text-[8px] font-black text-gray-600 uppercase">
                  CONSOLE OUTPUT
                </label>
                <button
                  onClick={clearPingResults}
                  className="text-[8px] font-black text-red/40 hover:text-red transition-colors flex items-center gap-1"
                >
                  <Trash2 size={8} /> CLEAR
                </button>
              </div>
              <div
                ref={scrollRef}
                className="w-full h-48 bg-black/60 border border-white/5 rounded-xl p-4 font-mono text-[10px] overflow-y-auto scrollbar-thin scrollbar-thumb-white/10"
              >
                {pingResults.length === 0 ? (
                  <div className="h-full flex items-center justify-center text-gray-800 italic uppercase font-black tracking-widest">
                    Waiting for command...
                  </div>
                ) : (
                  <div className="space-y-1">
                    {pingResults.map((res, i) => {
                      if (res.type === "result") {
                        return (
                          <div key={i} className="flex gap-2">
                            <span className="text-gray-600">[{i + 1}]</span>
                            {res.success ? (
                              <span className="text-green/80">
                                {res.bytes} bytes from {res.ip}: icmp_seq=
                                {res.seq} ttl={res.ttl} time={res.time} ms
                              </span>
                            ) : (
                              <span className="text-red/80">
                                Request timeout for icmp_seq {res.seq}
                              </span>
                            )}
                          </div>
                        );
                      } else {
                        return (
                          <div
                            key={i}
                            className="pt-2 mt-2 border-t border-white/5 text-accent font-black uppercase tracking-tighter"
                          >
                            --- {res.transmitted} packets transmitted,{" "}
                            {res.received} received,{" "}
                            {Math.floor(
                              (res.received! / res.transmitted!) * 100,
                            )}
                            % success, time {res.time_ms}ms ---
                          </div>
                        );
                      }
                    })}
                  </div>
                )}
              </div>
            </div>
          </div>
        </div>
      </section>
    </div>
  );
};
