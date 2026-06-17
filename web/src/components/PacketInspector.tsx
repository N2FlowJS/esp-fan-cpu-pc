import React from 'react';
import { PacketLog } from '../types';
import { Search, Copy, X } from 'lucide-react';
import { getProtoColor, getRssiColor } from '../utils/formatters';
import { getMacVendor } from '../utils/macLookup';

interface PacketInspectorProps {
  packet: { log: PacketLog, seq: number };
  onClose: () => void;
}

export const PacketInspector: React.FC<PacketInspectorProps> = ({ packet, onClose }) => {
  const { log, seq } = packet;
  
  const timeStr = new Date(log.time * 1000).toLocaleTimeString();

  const handleCopy = () => {
    const text = `[${timeStr}] ${log.proto} | ${log.src} -> ${log.dst} | ${log.rssi}dBm | ${log.info}`;
    navigator.clipboard.writeText(text);
  };

  const getMacWithVendor = (addr: string) => {
    if (!addr) return 'N/A';
    if (!addr.includes(':')) return addr;
    const vendor = getMacVendor(addr);
    return vendor ? `${addr} (${vendor})` : addr;
  };

  return (
    <div className="fixed inset-0 z-[100] flex items-center justify-center p-5 bg-bg/95 backdrop-blur-2xl animate-in fade-in duration-300" onClick={onClose}>
      <div className="w-full max-w-sm bg-surface border border-white/20 rounded-3xl overflow-hidden shadow-2xl animate-in zoom-in-95 duration-300" onClick={e => e.stopPropagation()}>
        <div className="px-5 py-4 bg-accent/10 border-b border-white/10 flex items-center justify-between">
          <div className="flex items-center gap-2 text-accent">
            <Search size={16} />
            <span className="text-[11px] font-black tracking-widest uppercase">Packet Details</span>
          </div>
          <span className="text-[10px] font-mono text-gray-500 bg-white/5 px-2 py-0.5 rounded border border-white/10 italic">#{seq}</span>
        </div>
        
        <div className="p-5 space-y-4 font-mono text-[11px] max-h-[60vh] overflow-y-auto scrollbar-thin">
           {[
             { l: 'Time', v: timeStr, c: 'text-yellow-400' },
             { l: 'Protocol', v: log.proto, c: getProtoColor(log.proto) },
             { l: 'Subtype', v: log.subtype || 'N/A', c: 'text-purple-400 font-bold' },
             { l: 'Channel', v: log.channel !== undefined ? `Ch ${log.channel}` : 'N/A', c: 'text-orange-400 font-bold' },
             
             { l: log.src.includes(':') ? 'Source MAC' : 'Source IP', v: getMacWithVendor(log.src), c: 'text-cyan-400 font-bold' },
             ...(log.srcPort && log.srcPort > 0 ? [{ l: 'Source Port', v: log.srcPort.toString(), c: 'text-cyan-600 font-semibold' }] : []),
             ...(log.srcMac && log.srcMac !== log.src ? [{ l: 'Source MAC', v: getMacWithVendor(log.srcMac), c: 'text-cyan-600 font-semibold' }] : []),
             
             { l: log.dst.includes(':') || log.dst === 'Broadcast' ? 'Dest MAC' : 'Dest IP', v: getMacWithVendor(log.dst), c: 'text-cyan-400 font-bold' },
             ...(log.dstPort && log.dstPort > 0 ? [{ l: 'Dest Port', v: log.dstPort.toString(), c: 'text-cyan-600 font-semibold' }] : []),
             ...(log.dstMac && log.dstMac !== log.dst ? [{ l: 'Dest MAC', v: getMacWithVendor(log.dstMac), c: 'text-cyan-600 font-semibold' }] : []),
             
             ...(log.ttl && log.ttl > 0 ? [{ l: 'TTL / Hop Limit', v: log.ttl.toString(), c: 'text-yellow-600 font-semibold' }] : []),
             { l: 'Signal', v: `${log.rssi} dBm`, c: getRssiColor(log.rssi) },
             { l: 'Length', v: `${log.len} bytes`, c: 'text-gray-400' },
           ].map((row, i) => (
             <div key={i} className="flex border-b border-white/5 pb-2">
               <span className="w-24 text-gray-500 uppercase text-[9px] font-bold">{row.l}</span>
               <span className={`font-black break-all flex-1 ${row.c}`}>{row.v}</span>
             </div>
           ))}
           
           <div className="space-y-2 pt-2">
             <span className="text-gray-500 uppercase text-[9px] font-bold block">Decoded Payload</span>
             <div className="bg-black/60 border border-white/5 rounded-2xl p-4 text-green-400 leading-relaxed whitespace-pre-wrap min-h-[80px] max-h-40 overflow-y-auto scrollbar-thin">
               {log.info}
             </div>
           </div>

           {log.rawHex && (
             <div className="space-y-2 pt-2">
               <span className="text-gray-500 uppercase text-[9px] font-bold block">Raw Hex Dump</span>
               <div className="bg-black/80 border border-white/5 rounded-2xl p-3 text-[#eedc82] font-mono text-[9px] leading-relaxed whitespace-pre-wrap max-h-40 overflow-y-auto scrollbar-thin break-all">
                 {log.rawHex}
               </div>
             </div>
           )}
        </div>

        <div className="p-4 bg-white/[0.02] border-t border-white/5 flex gap-3">
           <button 
             onClick={handleCopy}
             className="flex-1 flex items-center justify-center gap-2 py-3.5 rounded-xl bg-white/5 border border-white/10 text-[10px] font-bold text-gray-400 active:bg-white/10 transition-all"
           >
             <Copy size={14} /> COPY DATA
           </button>
           <button 
             onClick={onClose} 
             className="flex-1 py-3.5 rounded-xl bg-accent text-bg font-black text-[10px] tracking-widest shadow-lg active:scale-95 transition-all"
           >
             CLOSE
           </button>
        </div>
      </div>
    </div>
  );
};
