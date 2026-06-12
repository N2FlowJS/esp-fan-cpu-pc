import React, { useState } from 'react';
import { Monitor, AlertTriangle, Trash2, Search } from 'lucide-react';
import { getProtoColor, getRssiColor } from '../../utils/formatters';
import { useStore } from '../../store/useStore';
import { PacketLog } from '../../types';
import { apiSnifferControl } from '../../utils/api';

interface SnifferTabProps {
  onInspect: (log: PacketLog, seq: number) => void;
}

export const SnifferTab: React.FC<SnifferTabProps> = ({ onInspect }) => {
  const { sniffer, logs, clearLogs } = useStore();
  const [filterProto, setFilterProto] = useState('ALL');
  const [searchTerm, setSearchTerm] = useState('');

  const toggleSniffer = async () => {
    const active = !sniffer?.active;
    try {
      await apiSnifferControl(active, 0, true);
    } catch {}
  };

  const filteredLogs = logs.filter(log => {
    if (filterProto !== 'ALL') {
      if (filterProto === 'SECURITY') {
        if (log.proto !== 'DEAUTH' && log.proto !== 'DISASSOC') return false;
      } else if (filterProto === 'DNS') {
        if (log.proto !== 'DNS' && log.proto !== 'mDNS') return false;
      } else if (!log.proto.startsWith(filterProto)) return false;
    }
    if (searchTerm) {
      const hay = `${log.src} ${log.dst} ${log.info} ${log.proto}`.toLowerCase();
      if (!hay.includes(searchTerm.toLowerCase())) return false;
    }
    return true;
  });

  return (
    <div className="space-y-4 animate-fade-up">
      <section className="section-card">
        <div className="flex items-center gap-2 mb-4">
          <div className="w-1.5 h-3.5 bg-accent rounded-full shadow-[0_0_10px_rgba(99,179,237,0.8)]" />
          <h2 className="text-[11px] font-extrabold tracking-widest text-accent uppercase italic flex items-center gap-2">
            <Monitor size={12} /> WIFI MONITOR
          </h2>
          {sniffer?.active && <span className="ml-auto flex items-center gap-1.5 text-[9px] font-black text-green animate-pulse">● LIVE</span>}
        </div>

        {sniffer?.jammingAlert && (
          <div className="mb-4 p-3 rounded-xl bg-red/10 border border-red/50 text-red text-center text-xs font-black animate-pulse flex items-center justify-center gap-2">
            <AlertTriangle size={14} /> ALERT: DEAUTH FLOOD!
          </div>
        )}

        <button 
          onClick={toggleSniffer}
          className={`w-full py-4 rounded-2xl font-black tracking-[0.2em] text-xs transition-all active:scale-[0.98] ${
            sniffer?.active 
              ? 'bg-red text-bg shadow-[0_8px_24px_rgba(252,129,129,0.3)]' 
              : 'bg-accent text-bg shadow-[0_8px_24px_rgba(99,179,237,0.3)] hover:brightness-110'
          }`}
        >
          {sniffer?.active ? '⏹ STOP SNIFFER' : '⚡ START SNIFFER'}
        </button>
      </section>

      {sniffer?.active && (
        <div className="animate-in fade-in slide-in-from-bottom-4 duration-700 space-y-4">
          <div className="grid grid-cols-4 gap-2 mx-4">
            {[
              { l: 'TOTAL', v: sniffer.packets, c: '' },
              { l: 'BCN', v: sniffer.beacons, c: 'text-accent' },
              { l: 'PRB', v: sniffer.probes, c: 'text-green' },
              { l: 'DTH', v: sniffer.deauths, c: 'text-red' },
              { l: 'ARP', v: sniffer.arp, c: 'text-[#f6e05e]' },
              { l: 'DNS', v: sniffer.dns, c: 'text-[#76e4f7]' },
              { l: 'TCP', v: sniffer.tcp, c: 'text-accent' },
              { l: 'UDP', v: sniffer.udp, c: 'text-[#a3bffa]' },
            ].map((s, i) => (
              <div key={i} className="bg-black/30 border border-white/5 rounded-xl p-2.5 text-center shadow-inner">
                <div className={`text-sm font-black leading-none mb-1 ${s.c}`}>{s.v}</div>
                <div className="text-[8px] font-bold text-gray-600 uppercase tracking-tighter">{s.l}</div>
              </div>
            ))}
          </div>

          {/* Card 2: Detected Devices */}
          <div className="mx-4 bg-[#050709] border border-white/10 rounded-2xl overflow-hidden shadow-2xl">
            <div className="bg-white/[0.03] px-4 py-3 border-b border-white/10 flex items-center justify-between">
              <span className="text-[10px] font-black text-accent tracking-widest uppercase">
                DETECTED DEVICES ({sniffer.devices?.length || 0})
              </span>
            </div>
            <div className="h-48 overflow-y-auto font-mono text-[10px] scrollbar-thin scrollbar-thumb-white/10">
              <table className="w-full border-collapse table-fixed">
                <thead className="sticky top-0 bg-[#050709]/95 backdrop-blur z-10 text-[9px] font-bold text-gray-600 border-b border-white/5">
                  <tr className="text-left">
                    <th className="p-2 w-14">TYPE</th>
                    <th className="p-2 w-28">MAC</th>
                    <th className="p-2">SSID / INFO</th>
                    <th className="p-2 w-12 text-right">RSSI</th>
                  </tr>
                </thead>
                <tbody>
                  {(sniffer.devices || []).map((dev, i) => (
                    <tr key={i} className="border-b border-white/[0.02] hover:bg-accent/5 transition-colors">
                      <td className={`p-2 font-black tracking-tighter ${dev.isAP ? 'text-green' : 'text-accent'}`}>
                        {dev.isAP ? 'AP' : 'STA'}
                      </td>
                      <td className="p-2 text-gray-400 text-[9px] truncate">{dev.mac}</td>
                      <td className="p-2 text-gray-500 truncate">
                        {dev.isAP 
                          ? `${dev.ssid || '<Hidden SSID>'} (${dev.security || 'Open'}) ch:${dev.channel || '?'}`
                          : dev.ssid ? `Searching: ${dev.ssid}` : 'Probing...'
                        }
                      </td>
                      <td className={`p-2 text-right font-black ${getRssiColor(dev.rssi)}`}>
                        {dev.rssi}
                      </td>
                    </tr>
                  ))}
                  {(!sniffer.devices || sniffer.devices.length === 0) && (
                    <tr>
                      <td colSpan={4} className="p-4 text-center text-gray-700 font-bold uppercase tracking-widest">
                        No devices captured yet
                      </td>
                    </tr>
                  )}
                </tbody>
              </table>
            </div>
          </div>

          {/* Card 3: Real-time Packets */}
          <div className="mx-4 bg-[#050709] border border-white/10 rounded-2xl overflow-hidden shadow-2xl">
            <div className="bg-white/[0.03] px-4 py-3 border-b border-white/10 flex items-center justify-between">
               <div className="flex items-center gap-3">
                  <div className="flex items-center gap-1.5 px-2 py-0.5 rounded bg-black/40 border border-white/5">
                    <Search size={10} className="text-gray-600" />
                    <input 
                      type="text" placeholder="Filter..." value={searchTerm}
                      onChange={e => setSearchTerm(e.target.value)}
                      className="bg-transparent border-none outline-none text-[9px] font-bold text-accent w-20 placeholder:text-gray-700"
                    />
                  </div>
               </div>
               <div className="flex gap-2">
                  <select 
                    className="bg-black/80 text-[9px] font-black border border-white/10 rounded-lg px-2 py-1 outline-none text-accent focus:border-accent/40"
                    value={filterProto} onChange={(e) => setFilterProto(e.target.value)}
                  >
                     <option value="ALL">ALL</option>
                     <option value="TCP">TCP</option>
                     <option value="UDP">UDP</option>
                     <option value="DNS">DNS</option>
                     <option value="ARP">ARP</option>
                     <option value="SECURITY">⚠ DTH</option>
                  </select>
                  <button onClick={clearLogs} className="p-1.5 rounded-lg bg-red/5 border border-red/20 text-red hover:bg-red/10"><Trash2 size={12} /></button>
               </div>
            </div>
            
            <div className="h-80 overflow-y-auto font-mono text-[10px] scrollbar-thin scrollbar-thumb-white/10">
              <table className="w-full border-collapse table-fixed">
                <thead className="sticky top-0 bg-[#050709]/95 backdrop-blur z-10 text-[9px] font-bold text-gray-600 border-b border-white/5">
                  <tr className="text-left">
                    <th className="p-2 w-16">PROTO</th>
                    <th className="p-2 w-8 text-center">CH</th>
                    <th className="p-2 w-28 text-center">SRC → DST</th>
                    <th className="p-2">INFO</th>
                    <th className="p-2 w-10 text-right">RSSI</th>
                  </tr>
                </thead>
                <tbody>
                  {filteredLogs.map((log, i) => (
                    <tr 
                      key={i} 
                      onClick={() => onInspect(log, logs.length - i)}
                      className="border-b border-white/[0.02] hover:bg-accent/5 active:bg-accent/10 cursor-pointer group transition-colors"
                    >
                      <td className={`p-2 font-black tracking-tighter ${getProtoColor(log.proto)}`}>
                        {log.proto}
                        {log.subtype && <span className="block text-[8px] font-normal text-gray-500 truncate">{log.subtype}</span>}
                      </td>
                      <td className="p-2 text-gray-400 text-[9px] text-center font-bold">{log.channel ?? '-'}</td>
                      <td className="p-2 text-gray-400 text-[9px] truncate text-center" title={`${log.src} → ${log.dst}`}>
                        <div>{log.src}</div>
                        {log.dst && <div className="text-[8px] text-gray-600">→ {log.dst}</div>}
                      </td>
                      <td className="p-2 text-gray-500 truncate group-hover:text-gray-300">{log.info}</td>
                      <td className={`p-2 text-right font-black ${getRssiColor(log.rssi)}`}>{log.rssi}</td>
                    </tr>
                  ))}
                </tbody>
              </table>
            </div>
            
            <div className="bg-white/[0.02] px-4 py-1.5 border-t border-white/5 flex justify-between items-center text-[8px] font-bold text-gray-600 uppercase tracking-widest">
               <span>Buffer: {logs.length}/500</span>
               <span className="text-accent/40 animate-pulse">STATUS: SSE CAPTURING</span>
            </div>
          </div>
        </div>
      )}
    </div>
  );
};
