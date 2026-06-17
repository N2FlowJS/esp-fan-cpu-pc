import React, { useState, useEffect } from 'react';
import { Monitor, AlertTriangle, Trash2, Search, X, Shield, Cpu, Activity, Wifi, Database, Filter, Save, Plus } from 'lucide-react';
import { getProtoColor, getRssiColor } from '../../utils/formatters';
import { useStore } from '../../store/useStore';
import { PacketLog, DeviceInfo, SnifferFilters } from '../../types';
import { apiSnifferControl, apiGetSnifferFilters, apiSaveSnifferFilters, apiRegisterOwnerMac } from '../../utils/api';
import { getMacVendor } from '../../utils/macLookup';

interface SnifferTabProps {
  onInspect: (log: PacketLog, seq: number) => void;
}

const FilterModal: React.FC<{ onClose: () => void }> = ({ onClose }) => {
  const [filters, setFilters] = useState<SnifferFilters>({ whitelist: [], blacklist: [] });
  const [loading, setLoading] = useState(true);
  const [newWhite, setNewWhite] = useState('');
  const [newBlack, setNewBlack] = useState('');

  useEffect(() => {
    apiGetSnifferFilters().then(data => {
      if (data) setFilters(data);
      setLoading(false);
    });
  }, []);

  const handleSave = async () => {
    await apiSaveSnifferFilters(filters.whitelist, filters.blacklist);
    onClose();
  };

  const addMAC = (type: 'white' | 'black') => {
    const val = type === 'white' ? newWhite : newBlack;
    if (!/^([0-9A-Fa-f]{2}[:-]){5}([0-9A-Fa-f]{2})$/.test(val)) return;
    
    setFilters(prev => ({
      ...prev,
      [type === 'white' ? 'whitelist' : 'blacklist']: [...prev[type === 'white' ? 'whitelist' : 'blacklist'], val.toUpperCase()]
    }));
    if (type === 'white') setNewWhite(''); else setNewBlack('');
  };

  const removeMAC = (type: 'white' | 'black', index: number) => {
    setFilters(prev => ({
      ...prev,
      [type === 'white' ? 'whitelist' : 'blacklist']: prev[type === 'white' ? 'whitelist' : 'blacklist'].filter((_, i) => i !== index)
    }));
  };

  if (loading) return null;

  return (
    <div className="fixed inset-0 z-[60] flex items-center justify-center p-4 bg-bg/80 backdrop-blur-sm animate-in fade-in duration-300">
      <div className="w-full max-w-md bg-[#0a0c12] border border-white/10 rounded-3xl shadow-2xl overflow-hidden flex flex-col animate-in zoom-in-95 duration-300">
        <div className="bg-white/[0.03] px-6 py-4 border-b border-white/10 flex items-center justify-between">
          <div className="flex items-center gap-2">
            <Filter size={14} className="text-accent" />
            <span className="text-[10px] font-black text-gray-400 tracking-widest uppercase">MAC FILTERS</span>
          </div>
          <button onClick={onClose} className="p-2 hover:bg-white/5 rounded-full transition-colors text-gray-500 hover:text-white">
            <X size={18} />
          </button>
        </div>

        <div className="p-6 space-y-6 overflow-y-auto max-h-[70vh] scrollbar-thin scrollbar-thumb-white/10">
          {/* Whitelist */}
          <div className="space-y-3">
            <div className="text-[10px] font-black text-green uppercase tracking-widest flex items-center gap-2">
              <Shield size={10} /> WHITELIST (ONLY ALLOWED)
            </div>
            <div className="flex gap-2">
              <input 
                type="text" value={newWhite} onChange={e => setNewWhite(e.target.value)}
                placeholder="XX:XX:XX:XX:XX:XX"
                className="flex-1 bg-white/[0.02] border border-white/10 rounded-xl px-4 py-2 text-xs font-mono text-white outline-none focus:border-green/40"
              />
              <button onClick={() => addMAC('white')} className="p-2 rounded-xl bg-green/20 text-green border border-green/20 hover:bg-green/30"><Plus size={18} /></button>
            </div>
            <div className="flex flex-wrap gap-2">
              {filters.whitelist.map((m, i) => (
                <span key={i} className="px-2 py-1 rounded-lg bg-green/10 border border-green/20 text-[10px] font-mono text-green flex items-center gap-1.5">
                  {m} <button onClick={() => removeMAC('white', i)}><X size={10} /></button>
                </span>
              ))}
              {filters.whitelist.length === 0 && <span className="text-[9px] text-gray-600 italic">No whitelist (all allowed unless blacklisted)</span>}
            </div>
          </div>

          {/* Blacklist */}
          <div className="space-y-3">
            <div className="text-[10px] font-black text-red uppercase tracking-widest flex items-center gap-2">
              <Shield size={10} /> BLACKLIST (BLOCKED)
            </div>
            <div className="flex gap-2">
              <input 
                type="text" value={newBlack} onChange={e => setNewBlack(e.target.value)}
                placeholder="XX:XX:XX:XX:XX:XX"
                className="flex-1 bg-white/[0.02] border border-white/10 rounded-xl px-4 py-2 text-xs font-mono text-white outline-none focus:border-red/40"
              />
              <button onClick={() => addMAC('black')} className="p-2 rounded-xl bg-red/20 text-red border border-red/20 hover:bg-red/30"><Plus size={18} /></button>
            </div>
            <div className="flex flex-wrap gap-2">
              {filters.blacklist.map((m, i) => (
                <span key={i} className="px-2 py-1 rounded-lg bg-red/10 border border-red/20 text-[10px] font-mono text-red flex items-center gap-1.5">
                  {m} <button onClick={() => removeMAC('black', i)}><X size={10} /></button>
                </span>
              ))}
              {filters.blacklist.length === 0 && <span className="text-[9px] text-gray-600 italic">No blacklist</span>}
            </div>
          </div>
        </div>

        <div className="p-6 bg-white/[0.01] border-t border-white/5 flex gap-3">
           <button 
            onClick={onClose}
            className="flex-1 py-3 rounded-2xl bg-white/5 border border-white/10 text-gray-500 font-black text-[10px] tracking-widest uppercase hover:bg-white/10 transition-all"
           >
             CANCEL
           </button>
           <button 
            onClick={handleSave}
            className="flex-1 py-3 rounded-2xl bg-accent text-bg font-black text-[10px] tracking-widest uppercase flex items-center justify-center gap-2 hover:brightness-110 transition-all"
           >
             <Save size={12} /> APPLY FILTERS
           </button>
        </div>
      </div>
    </div>
  );
};

interface SnifferTabProps {
  onInspect: (log: PacketLog, seq: number) => void;
}

const formatUptime = (seconds: number) => {
  const h = Math.floor(seconds / 3600);
  const m = Math.floor((seconds % 3600) / 60);
  const s = Math.floor(seconds % 60);
  const ms = Math.floor((seconds % 1) * 1000);
  return `${h.toString().padStart(2, '0')}:${m.toString().padStart(2, '0')}:${s.toString().padStart(2, '0')}.${ms.toString().padStart(3, '0')}`;
};

const DeviceModal: React.FC<{ device: DeviceInfo, onClose: () => void, uptime: number, onUpdateFilters?: () => void }> = ({ device, onClose, uptime, onUpdateFilters }) => {
  const lastSeenSec = device.lastSeen ? (uptime - device.lastSeen) : 0;
  const lastSeenStr = lastSeenSec < 1 ? 'Just now' : `${Math.floor(lastSeenSec)}s ago`;

  const handleQuickFilter = async (type: 'white' | 'black') => {
    const data = await apiGetSnifferFilters();
    if (!data) return;
    
    const list = type === 'white' ? data.whitelist : data.blacklist;
    if (list.includes(device.mac)) {
      alert('Device already in list');
      return;
    }
    
    const newWhite = type === 'white' ? [...data.whitelist, device.mac] : data.whitelist;
    const newBlack = type === 'black' ? [...data.blacklist, device.mac] : data.blacklist;
    
    await apiSaveSnifferFilters(newWhite, newBlack);
    alert(`Added ${device.mac} to ${type === 'white' ? 'Whitelist' : 'Blacklist'}`);
    if (onUpdateFilters) onUpdateFilters();
    onClose();
  };

  return (
    <div className="fixed inset-0 z-[60] flex items-center justify-center p-4 bg-bg/80 backdrop-blur-sm animate-in fade-in duration-300">
      <div 
        className="w-full max-w-sm bg-[#0a0c12] border border-white/10 rounded-3xl shadow-2xl overflow-hidden flex flex-col animate-in zoom-in-95 duration-300"
        onClick={(e) => e.stopPropagation()}
      >
        <div className="bg-white/[0.03] px-6 py-4 border-b border-white/10 flex items-center justify-between">
          <div className="flex items-center gap-2">
             <div className={`w-2 h-2 rounded-full animate-pulse ${device.isAP ? 'bg-green' : 'bg-accent'}`} />
             <span className="text-[10px] font-black text-gray-400 tracking-widest uppercase">DEVICE DETAILS</span>
          </div>
          <button onClick={onClose} className="p-2 hover:bg-white/5 rounded-full transition-colors text-gray-500 hover:text-white">
            <X size={18} />
          </button>
        </div>

        <div className="p-6 space-y-6 overflow-y-auto max-h-[70vh] scrollbar-thin scrollbar-thumb-white/10">
          <div className="text-center">
            <div className="text-[10px] font-bold text-gray-600 uppercase tracking-widest mb-1">MAC ADDRESS</div>
            <div className="text-xl font-black text-white tracking-wider font-mono">{device.mac}</div>
            <div className="mt-2 inline-block px-3 py-1 rounded-full bg-accent/10 border border-accent/20 text-accent text-[9px] font-black tracking-widest uppercase">
              {getMacVendor(device.mac) || 'Unknown Vendor'}
            </div>
          </div>

          <div className="grid grid-cols-2 gap-2">
            <button 
              onClick={() => handleQuickFilter('white')}
              className="py-2.5 rounded-xl bg-green/10 border border-green/20 text-green text-[9px] font-black tracking-widest uppercase hover:bg-green/20 transition-all flex items-center justify-center gap-2"
            >
              <Shield size={12} /> + Whitelist
            </button>
            <button 
              onClick={() => handleQuickFilter('black')}
              className="py-2.5 rounded-xl bg-red/10 border border-red/20 text-red text-[9px] font-black tracking-widest uppercase hover:bg-red/20 transition-all flex items-center justify-center gap-2"
            >
              <Shield size={12} /> + Blacklist
            </button>
          </div>

          <div className="grid grid-cols-2 gap-3">
             <div className="bg-white/[0.02] border border-white/5 rounded-2xl p-4">
                <div className="flex items-center gap-2 mb-2 text-gray-500 uppercase tracking-widest">
                  <Shield size={10} />
                  <span className="text-[8px] font-black">Type</span>
                </div>
                <div className={`text-sm font-black ${device.isAP ? 'text-green' : 'text-accent'}`}>{device.isAP ? 'Access Point' : 'Station'}</div>
             </div>
             <div className="bg-white/[0.02] border border-white/5 rounded-2xl p-4">
                <div className="flex items-center gap-2 mb-2 text-gray-500 uppercase tracking-widest">
                  <Activity size={10} />
                  <span className="text-[8px] font-black">Signal</span>
                </div>
                <div className={`text-sm font-black ${getRssiColor(device.rssi)}`}>{device.rssi} dBm</div>
             </div>
          </div>

          <div className="space-y-3">
             <div className="bg-white/[0.02] border border-white/5 rounded-2xl p-4 flex items-center justify-between">
                <div className="flex items-center gap-2 text-gray-500 uppercase tracking-widest">
                  <Wifi size={10} />
                  <span className="text-[8px] font-black">SSID / INFO</span>
                </div>
                <div className="text-xs font-bold text-gray-300">
                  {device.isAP ? (device.ssid || '<Hidden>') : (device.ssid ? `Probe: ${device.ssid}` : 'Scanning')}
                </div>
             </div>
             
             {device.isAP && (
               <>
                 <div className="bg-white/[0.02] border border-white/5 rounded-2xl p-4 flex items-center justify-between">
                    <div className="flex items-center gap-2 text-gray-500 uppercase tracking-widest">
                      <Shield size={10} />
                      <span className="text-[8px] font-black">Security</span>
                    </div>
                    <div className="text-xs font-bold text-gray-300">{device.security || 'Open'}</div>
                 </div>
                 <div className="bg-white/[0.02] border border-white/5 rounded-2xl p-4 flex items-center justify-between">
                    <div className="flex items-center gap-2 text-gray-500 uppercase tracking-widest">
                      <Database size={10} />
                      <span className="text-[8px] font-black">Channel</span>
                    </div>
                    <div className="text-xs font-bold text-gray-300">CH {device.channel}</div>
                 </div>
                 {device.wifiGen && (
                   <div className="bg-white/[0.02] border border-white/5 rounded-2xl p-4 flex items-center justify-between">
                      <div className="flex items-center gap-2 text-gray-500 uppercase tracking-widest">
                        <Cpu size={10} />
                        <span className="text-[8px] font-black">Standard</span>
                      </div>
                      <div className="text-xs font-bold text-accent">{device.wifiGen}</div>
                   </div>
                 )}
                 {device.clients !== undefined && device.clients >= 0 && (
                   <div className="bg-white/[0.02] border border-white/5 rounded-2xl p-4 flex items-center justify-between">
                      <div className="flex items-center gap-2 text-gray-500 uppercase tracking-widest">
                        <Monitor size={10} />
                        <span className="text-[8px] font-black">Active Clients</span>
                      </div>
                      <div className="text-xs font-bold text-green">{device.clients}</div>
                   </div>
                 )}
               </>
             )}

             <div className="bg-white/[0.02] border border-white/5 rounded-2xl p-4 flex items-center justify-between">
                <div className="flex items-center gap-2 text-gray-500 uppercase tracking-widest">
                  <Activity size={10} />
                  <span className="text-[8px] font-black">Packets</span>
                </div>
                <div className="text-xs font-bold text-gray-300">{device.packetCount || 1}</div>
             </div>

             <div className="bg-white/[0.02] border border-white/5 rounded-2xl p-4 flex items-center justify-between">
                <div className="flex items-center gap-2 text-gray-500 uppercase tracking-widest">
                  <X size={10} />
                  <span className="text-[8px] font-black">Last Seen</span>
                </div>
                <div className="text-xs font-bold text-accent">{lastSeenStr}</div>
             </div>
          </div>
        </div>

        <div className="p-6 bg-white/[0.01] border-t border-white/5">
           <button 
            onClick={onClose}
            className="w-full py-4 rounded-2xl bg-white/5 border border-white/10 text-gray-300 font-black text-[10px] tracking-[0.2em] uppercase hover:bg-white/10 transition-all active:scale-[0.98]"
           >
             CLOSE OVERLAY
           </button>
        </div>
      </div>
    </div>
  );
};

const renderAddr = (addr: string, mac?: string) => {
  const isMac = addr.includes(':');
  const resolvedMac = !isMac && mac ? mac : (isMac ? addr : '');
  const vendor = resolvedMac ? getMacVendor(resolvedMac) : '';
  
  return (
    <div className="flex flex-col truncate text-left leading-tight">
      <span className="font-bold text-gray-300">{addr}</span>
      {resolvedMac && (
        <span className="text-[7.5px] text-gray-500 truncate mt-0.5">
          {resolvedMac !== addr && `${resolvedMac}`}
          {vendor && <span className="text-accent/70 ml-1 font-semibold">({vendor})</span>}
        </span>
      )}
    </div>
  );
};

const PacketRow = React.memo(({ log, seq, onInspect }: { log: PacketLog, seq: number, onInspect: (log: PacketLog, seq: number) => void }) => {
  return (
    <tr 
      onClick={() => onInspect(log, seq)}
      className="border-b border-white/[0.02] hover:bg-accent/5 active:bg-accent/10 cursor-pointer group transition-colors"
    >
      <td className="p-2 text-gray-500 font-medium whitespace-nowrap">
        {formatUptime(log.time)}
      </td>
      <td className={`p-2 font-black tracking-tighter ${getProtoColor(log.proto)}`}>
        {log.proto}
      </td>
      <td className="p-2 text-gray-400 font-bold tracking-tighter truncate">
        {log.subtype || '-'}
      </td>
      <td className="p-2 text-gray-400 text-[9px] truncate">
        {renderAddr(log.src, log.srcMac)}
      </td>
      <td className="p-2 text-gray-400 text-[9px] text-center font-bold">
        {log.srcPort && log.srcPort > 0 ? log.srcPort : '-'}
      </td>
      <td className="p-2 text-gray-400 text-[9px] truncate">
        {renderAddr(log.dst, log.dstMac)}
      </td>
      <td className="p-2 text-gray-400 text-[9px] text-center font-bold">
        {log.dstPort && log.dstPort > 0 ? log.dstPort : '-'}
      </td>
      <td className="p-2 text-gray-400 text-[9px] text-center font-bold">{log.channel ?? '-'}</td>
      <td className="p-2 text-gray-400 text-[9px] text-right font-medium whitespace-nowrap">{log.len} B</td>
      <td className="p-2 text-gray-400 text-[9px] text-center font-bold">{log.ttl && log.ttl > 0 ? log.ttl : '-'}</td>
      <td className={`p-2 text-right font-black whitespace-nowrap ${getRssiColor(log.rssi)}`}>{log.rssi} dBm</td>
      <td className="p-2 text-gray-500 truncate group-hover:text-gray-300" title={log.info}>{log.info}</td>
    </tr>
  );
});

const DeviceCard = ({ dev, status, onClick }: { dev: DeviceInfo, status: any, onClick: () => void }) => {
  const lastSeenSec = dev.lastSeen ? (status?.uptime || 0) - dev.lastSeen : 0;
  const lastSeenStr = lastSeenSec < 1 ? 'now' : `${Math.floor(lastSeenSec)}s`;
  return (
    <div 
      onClick={onClick}
      className="bg-white/[0.03] border border-white/10 rounded-2xl p-4 space-y-3 active:scale-[0.98] transition-all"
    >
      <div className="flex justify-between items-start">
        <div className="flex items-center gap-2">
          <div className={`px-1.5 py-0.5 rounded text-[8px] font-black tracking-tighter ${dev.isAP ? 'bg-green/20 text-green' : 'bg-accent/20 text-accent'}`}>
            {dev.isAP ? 'AP' : 'STATION'}
          </div>
          <span className="text-[10px] font-black text-white font-mono">{dev.mac}</span>
        </div>
        <div className={`text-[10px] font-black ${getRssiColor(dev.rssi)}`}>{dev.rssi} dBm</div>
      </div>
      <div className="text-xs font-bold text-gray-400 truncate">
        {dev.isAP ? (dev.ssid || '<Hidden SSID>') : (dev.ssid ? `Probe: ${dev.ssid}` : 'Scanning...')}
      </div>
      <div className="flex flex-wrap gap-2 text-[8px] font-black text-gray-600 uppercase">
        <span className="bg-black/40 px-2 py-0.5 rounded border border-white/5">CH {dev.channel || '?'}</span>
        {dev.wifiGen && <span className="bg-accent/10 text-accent px-2 py-0.5 rounded border border-accent/20">{dev.wifiGen}</span>}
        <span className="bg-black/40 px-2 py-0.5 rounded border border-white/5">{dev.packetCount || 1} PKTS</span>
        <span className="ml-auto text-accent/60">{lastSeenStr}</span>
      </div>
    </div>
  );
};

const PacketCard = ({ log, seq, onInspect }: { log: PacketLog, seq: number, onInspect: (log: PacketLog, seq: number) => void }) => {
  return (
    <div 
      onClick={() => onInspect(log, seq)}
      className="bg-white/[0.02] border border-white/5 rounded-2xl p-4 space-y-2 active:bg-white/[0.05] transition-all"
    >
      <div className="flex justify-between items-center">
        <div className="flex items-center gap-2">
          <span className={`text-[9px] font-black px-1.5 py-0.5 rounded ${getProtoColor(log.proto)} bg-black/40 border border-white/5`}>
            {log.proto}
          </span>
          {log.subtype && <span className="text-[8px] font-bold text-gray-500 uppercase">{log.subtype}</span>}
        </div>
        <span className="text-[8px] font-medium text-gray-600 font-mono">{formatUptime(log.time)}</span>
      </div>
      <div className="grid grid-cols-2 gap-x-4 gap-y-1">
        <div>
          <span className="text-[7px] font-black text-gray-600 uppercase block">Source</span>
          <span className="text-[9px] font-bold text-gray-400 truncate block">{log.src}</span>
        </div>
        <div>
          <span className="text-[7px] font-black text-gray-600 uppercase block">Destination</span>
          <span className="text-[9px] font-bold text-gray-400 truncate block">{log.dst}</span>
        </div>
      </div>
      <div className="text-[9px] text-gray-500 truncate italic">
        {log.info}
      </div>
      <div className="flex justify-between items-center pt-1 border-t border-white/[0.03] text-[8px] font-black text-gray-600">
        <div className="flex gap-2">
          <span>{log.len} BYTES</span>
          {log.channel && <span>CH {log.channel}</span>}
          {log.ttl && <span>TTL {log.ttl}</span>}
        </div>
        <span className={getRssiColor(log.rssi)}>{log.rssi} dBm</span>
      </div>
    </div>
  );
};

export const SnifferTab: React.FC<SnifferTabProps> = ({ onInspect }) => {
  const { sniffer, logs, clearLogs, status } = useStore();
  const [filterProto, setFilterProto] = useState('ALL');
  const [searchTerm, setSearchTerm] = useState('');
  const [selectedDevice, setSelectedDevice] = useState<DeviceInfo | null>(null);
  const [showFilters, setShowFilters] = useState(false);
  const [activeFilters, setActiveFilters] = useState<SnifferFilters>({ whitelist: [], blacklist: [] });

  const refreshFilters = () => {
    apiGetSnifferFilters().then(data => {
      if (data) setActiveFilters(data);
    });
  };

  useEffect(() => {
    refreshFilters();
  }, [showFilters]); // Refresh when modal closes

  const isFilteringActive = activeFilters.whitelist.length > 0 || activeFilters.blacklist.length > 0;

  const toggleSniffer = async () => {
    const active = !sniffer?.active;
    try {
      await apiSnifferControl(active, 0, true);
    } catch {}
  };

  // Dynamically build protocol list from logs
  const availableProtos = React.useMemo(() => {
    const protos = new Set<string>();
    logs.forEach(l => protos.add(l.proto));
    
    // Add special groupings if they exist in data
    const list = Array.from(protos).sort();
    
    // Check if we have security related packets to justify the group
    const hasSecurity = logs.some(l => l.proto === 'DEAUTH' || l.proto === 'DISASSOC' || l.proto === 'WPA HS');
    const hasDns = logs.some(l => l.proto === 'DNS' || l.proto === 'mDNS');

    return { list, hasSecurity, hasDns };
  }, [logs]);

  const filteredLogs = logs.filter(log => {
    if (filterProto !== 'ALL') {
      if (filterProto === 'GROUP:SECURITY') {
        if (log.proto !== 'DEAUTH' && log.proto !== 'DISASSOC' && log.proto !== 'WPA HS') return false;
      } else if (filterProto === 'GROUP:DNS') {
        if (log.proto !== 'DNS' && log.proto !== 'mDNS') return false;
      } else if (log.proto !== filterProto) return false;
    }
    if (searchTerm) {
      const srcMacVal = log.srcMac || (log.src.includes(':') ? log.src : '');
      const dstMacVal = log.dstMac || (log.dst.includes(':') ? log.dst : '');
      const srcVendor = srcMacVal ? getMacVendor(srcMacVal) : '';
      const dstVendor = dstMacVal ? getMacVendor(dstMacVal) : '';
      const srcPortStr = log.srcPort ? log.srcPort.toString() : '';
      const dstPortStr = log.dstPort ? log.dstPort.toString() : '';
      const subtypeStr = log.subtype || '';
      
      const hay = `
        ${log.src} ${log.dst} ${log.info} ${log.proto} 
        ${srcMacVal} ${dstMacVal} ${srcVendor} ${dstVendor} 
        ${srcPortStr} ${dstPortStr} ${subtypeStr}
      `.toLowerCase();
      
      if (!hay.includes(searchTerm.toLowerCase())) return false;
    }
    return true;
  });

  return (
    <div className="space-y-4 animate-fade-up">
      {selectedDevice && (
        <DeviceModal 
          device={selectedDevice} 
          onClose={() => setSelectedDevice(null)} 
          uptime={status?.uptime || 0}
          onUpdateFilters={refreshFilters}
        />
      )}
      {showFilters && (
        <FilterModal onClose={() => setShowFilters(false)} />
      )}
      
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

        <div className="flex gap-3">
          <button 
            onClick={toggleSniffer}
            className={`flex-1 py-4 rounded-2xl font-black tracking-[0.2em] text-xs transition-all active:scale-[0.98] ${
              sniffer?.active 
                ? 'bg-red text-bg shadow-[0_8px_24px_rgba(252,129,129,0.3)]' 
                : 'bg-accent text-bg shadow-[0_8px_24px_rgba(99,179,237,0.3)] hover:brightness-110'
            }`}
          >
            {sniffer?.active ? '⏹ STOP SNIFFER' : '⚡ START SNIFFER'}
          </button>
          
          <button 
            onClick={() => setShowFilters(true)}
            className={`px-6 rounded-2xl border transition-all flex flex-col items-center justify-center relative ${
              isFilteringActive 
                ? 'bg-green/10 border-green/30 text-green shadow-[0_0_15px_rgba(72,187,120,0.2)]' 
                : 'bg-white/5 border-white/10 text-accent hover:bg-white/10'
            }`}
            title="MAC Filters"
          >
            <Filter size={18} />
            {isFilteringActive && (
              <span className="absolute -top-1 -right-1 flex h-3 w-3">
                <span className="animate-ping absolute inline-flex h-full w-full rounded-full bg-green opacity-75"></span>
                <span className="relative inline-flex rounded-full h-3 w-3 bg-green"></span>
              </span>
            )}
            <span className="text-[7px] font-black mt-1 uppercase tracking-tighter">
              {isFilteringActive ? 'ACTIVE' : 'FILTERS'}
            </span>
          </button>
        </div>
      </section>

      {sniffer?.active && (
        <div className="animate-in fade-in slide-in-from-bottom-4 duration-700 space-y-4">
          {isFilteringActive && (
            <div className="mx-4 p-2.5 rounded-xl bg-green/5 border border-green/20 flex items-center justify-between">
              <div className="flex items-center gap-2">
                <Shield size={12} className="text-green" />
                <span className="text-[9px] font-black text-green/70 uppercase tracking-widest">
                  MAC FILTERING ACTIVE: {activeFilters.whitelist.length} Whitelist / {activeFilters.blacklist.length} Blacklist
                </span>
              </div>
              <button onClick={() => setShowFilters(true)} className="text-[8px] font-black text-accent uppercase hover:underline">Edit Filters</button>
            </div>
          )}

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

          {/* Card 2 & 2B: Devices & Spectrum/Protocol Analysis */}
          <div className="grid grid-cols-1 lg:grid-cols-2 gap-4 mx-4">
            {/* Card 2: Detected Devices */}
            <div className="bg-[#050709] border border-white/10 rounded-2xl overflow-hidden shadow-2xl flex flex-col min-h-[340px] max-h-[500px]">
              <div className="bg-white/[0.03] px-4 py-3 border-b border-white/10 flex items-center justify-between">
                <span className="text-[10px] font-black text-accent tracking-widest uppercase">
                  DETECTED DEVICES ({sniffer.devices?.length || 0})
                </span>
              </div>
              <div className="flex-1 overflow-y-auto font-mono text-[10px] scrollbar-thin scrollbar-thumb-white/10">
                {/* Desktop Table View */}
                <div className="hidden sm:block">
                  <table className="w-full border-collapse table-fixed min-w-[700px]">
                    <thead className="sticky top-0 bg-[#050709]/95 backdrop-blur z-10 text-[9px] font-bold text-gray-600 border-b border-white/5">
                      <tr className="text-left">
                        <th className="p-2 w-8 text-center">T</th>
                        <th className="p-2 w-28">MAC / VENDOR</th>
                        <th className="p-2">SSID / INFO</th>
                        <th className="p-2 w-10 text-center">CH</th>
                        <th className="p-2 w-12 text-center">SEC</th>
                        <th className="p-2 w-8 text-center">GN</th>
                        <th className="p-2 w-10 text-right">PKTS</th>
                        <th className="p-2 w-12 text-right">RSSI</th>
                        <th className="p-2 w-14 text-right">SEEN</th>
                      </tr>
                    </thead>
                    <tbody>
                      {(sniffer.devices || []).map((dev, i) => (
                        <tr 
                          key={i} 
                          onClick={() => setSelectedDevice(dev)}
                          className="border-b border-white/[0.02] hover:bg-accent/5 transition-colors cursor-pointer group"
                        >
                          <td className={`p-2 text-center font-black tracking-tighter ${dev.isAP ? 'text-green' : 'text-accent'}`}>
                            {dev.isAP ? 'AP' : 'ST'}
                          </td>
                          <td className="p-2 text-gray-400 text-[9px] truncate">
                            <div className="group-hover:text-white transition-colors">{dev.mac}</div>
                            {getMacVendor(dev.mac) && (
                              <span className="text-[7.5px] font-bold text-gray-500 bg-white/[0.02] px-1 py-0.2 rounded border border-white/5 uppercase tracking-tighter mt-0.5 inline-block truncate max-w-full">
                                {getMacVendor(dev.mac)}
                              </span>
                            )}
                          </td>
                          <td className="p-2 text-gray-500 truncate group-hover:text-gray-400">
                            {dev.isAP ? (dev.ssid || '<Hidden SSID>') : (dev.ssid ? `Probe: ${dev.ssid}` : 'Scanning...')}
                          </td>
                          <td className="p-2 text-center text-gray-400 font-bold">{dev.channel || '?'}</td>
                          <td className="p-2 text-center text-[8px] text-gray-500 font-medium">
                            {dev.security ? dev.security.split(' ')[0] : (dev.isAP ? 'Open' : '-')}
                          </td>
                          <td className="p-2 text-center text-accent/80 font-black">{dev.wifiGen ? dev.wifiGen.replace('WiFi ', '') : '-'}</td>
                          <td className="p-2 text-right font-bold text-gray-600 group-hover:text-gray-400">
                            {dev.packetCount || 1}
                          </td>
                          <td className={`p-2 text-right font-black ${getRssiColor(dev.rssi)}`}>
                            {dev.rssi}
                          </td>
                          <td className="p-2 text-right text-accent/60 font-medium">
                            {dev.lastSeen ? `${Math.floor((status?.uptime || 0) - dev.lastSeen)}s` : 'now'}
                          </td>
                        </tr>
                      ))}
                    </tbody>
                  </table>
                </div>
                {/* Mobile Card View */}
                <div className="sm:hidden p-3 space-y-3">
                  {(sniffer.devices || []).map((dev, i) => (
                    <DeviceCard key={i} dev={dev} status={status} onClick={() => setSelectedDevice(dev)} />
                  ))}
                  {(!sniffer.devices || sniffer.devices.length === 0) && (
                    <div className="p-8 text-center text-gray-700 font-bold uppercase tracking-widest text-[9px]">
                      No devices captured yet
                    </div>
                  )}
                </div>
              </div>
            </div>

            {/* Card 2B: Spectrum & Protocol Analysis */}
            <div className="bg-[#050709] border border-white/10 rounded-2xl overflow-hidden shadow-2xl flex flex-col min-h-[340px]">
              <div className="bg-white/[0.03] px-4 py-3 border-b border-white/10">
                <span className="text-[10px] font-black text-accent tracking-widest uppercase">
                  SPECTRUM & PROTOCOL ANALYSIS
                </span>
              </div>
              <div className="flex-1 overflow-y-auto p-4 space-y-4 scrollbar-thin scrollbar-thumb-white/10">
                {/* Channel Occupancy */}
                <div className="space-y-2">
                  <span className="text-[8px] font-black text-gray-500 uppercase tracking-widest block">Channel Congestion (1-13)</span>
                  <div className="grid gap-1" style={{ gridTemplateColumns: 'repeat(13, minmax(0, 1fr))' }}>
                    {Array.from({ length: 13 }, (_, i) => {
                      const ch = i + 1;
                      const count = (sniffer.devices || []).filter(d => d.isAP && d.channel === ch).length;
                      const isActive = count > 0;
                      return (
                        <div key={ch} className={`flex flex-col items-center p-1 rounded-lg border transition-all ${
                          isActive 
                            ? 'bg-accent/10 border-accent/30 text-accent' 
                            : 'bg-black/20 border-white/5 text-gray-700'
                        }`}>
                          <span className="text-[7px] font-black">CH</span>
                          <span className="text-[9px] font-black leading-none">{ch}</span>
                          <span className={`text-[8px] font-bold mt-1 leading-none ${isActive ? 'text-green' : 'text-transparent'}`}>
                            {count}
                          </span>
                        </div>
                      );
                    })}
                  </div>
                </div>

                {/* Detailed Protocol Breakdown */}
                <div className="space-y-2 border-t border-white/5 pt-3">
                  <span className="text-[8px] font-black text-gray-500 uppercase tracking-widest block">Deep Packet Inspection Stats</span>
                  <div className="grid grid-cols-1 xs:grid-cols-2 gap-2 text-[10px] font-mono">
                    {[
                      { label: 'WPA HS (EAPOL)', value: sniffer.eapol || 0, color: 'text-yellow-400', desc: 'Handshake packets' },
                      { label: 'QUIC (HTTP/3)', value: sniffer.quic || 0, color: 'text-[#a3bffa]', desc: 'UDP Encrypted Web' },
                      { label: 'mDNS / ZeroConf', value: sniffer.mdns || 0, color: 'text-accent', desc: 'Local discovery' },
                      { label: 'DHCP (Bootp)', value: sniffer.dhcp || 0, color: 'text-green', desc: 'IP assignments' },
                      { label: 'MQTT (IoT)', value: sniffer.mqtt || 0, color: 'text-purple-400', desc: 'IoT Telemetry' },
                      { label: 'ICMP (Ping)', value: sniffer.icmp || 0, color: 'text-orange-400', desc: 'Network diagnostics' },
                      { label: 'SSDP (UPnP)', value: sniffer.ssdp || 0, color: 'text-pink-400', desc: 'Plug & Play discovery' },
                      { label: 'LLMNR / NBNS', value: (sniffer.llmnr || 0) + (sniffer.nbns || 0), color: 'text-[#f6ad55]', desc: 'Name resolution' },
                      { label: 'Other Frames', value: sniffer.other || 0, color: 'text-gray-500', desc: 'Generic packets' },
                    ].map((p, idx) => (
                      <div key={idx} className="bg-white/[0.02] border border-white/5 rounded-xl p-2 flex items-center justify-between">
                        <div>
                          <div className="text-[9px] font-bold text-gray-400 uppercase leading-none mb-0.5">{p.label}</div>
                          <div className="text-[7px] text-gray-600 leading-none">{p.desc}</div>
                        </div>
                        <span className={`text-xs font-black ${p.value > 0 ? p.color : 'text-gray-700'}`}>
                          {p.value}
                        </span>
                      </div>
                    ))}
                  </div>
                </div>
              </div>
            </div>
          </div>

          {/* Card 3: Real-time Packets */}
          <div className="mx-4 bg-[#050709] border border-white/10 rounded-2xl overflow-hidden shadow-2xl flex flex-col h-[600px]">
            <div className="bg-white/[0.03] px-4 py-3 border-b border-white/10 flex flex-col sm:flex-row items-center justify-between gap-3">
               <div className="flex items-center gap-3 w-full sm:w-auto">
                  <span className="text-[10px] font-black text-accent tracking-widest uppercase hidden md:block">
                    PACKET STREAM
                  </span>
                  <div className="flex items-center gap-1.5 px-2 py-0.5 rounded bg-black/40 border border-white/5 flex-1 sm:flex-none">
                    <Search size={10} className="text-gray-600" />
                    <input 
                      type="text" placeholder="Filter..." value={searchTerm}
                      onChange={e => setSearchTerm(e.target.value)}
                      className="bg-transparent border-none outline-none text-[9px] font-bold text-accent w-full sm:w-20 placeholder:text-gray-700"
                    />
                  </div>
               </div>
               <div className="flex gap-2 w-full sm:w-auto">
                  <select 
                    className="flex-1 sm:flex-none bg-black/80 text-[9px] font-black border border-white/10 rounded-lg px-2 py-2 sm:py-1 outline-none text-accent focus:border-accent/40"
                    value={filterProto} onChange={(e) => setFilterProto(e.target.value)}
                  >
                     <option value="ALL">ALL PROTOS</option>
                     {availableProtos.hasSecurity && <option value="GROUP:SECURITY">🛡 SECURITY</option>}
                     {availableProtos.hasDns && <option value="GROUP:DNS">🌐 DNS</option>}
                     {availableProtos.list.map(p => (
                       <option key={p} value={p}>{p}</option>
                     ))}
                  </select>
                  <button onClick={clearLogs} className="p-2 sm:p-1.5 rounded-lg bg-red/5 border border-red/20 text-red hover:bg-red/10"><Trash2 size={12} /></button>
               </div>
            </div>
            
            <div className="flex-1 overflow-auto font-mono text-[10px] scrollbar-thin scrollbar-thumb-white/10">
              {/* Desktop Table View */}
              <div className="hidden md:block">
                <table className="w-full border-collapse table-fixed min-w-[1100px]">
                  <thead className="sticky top-0 bg-[#050709]/95 backdrop-blur z-10 text-[9px] font-bold text-gray-600 border-b border-white/5">
                    <tr className="text-left">
                      <th className="p-2 w-[85px]">TIME</th>
                      <th className="p-2 w-[50px]">PROTO</th>
                      <th className="p-2 w-[60px]">METH</th>
                      <th className="p-2 w-[140px]">SOURCE</th>
                      <th className="p-2 w-[45px] text-center">S-PORT</th>
                      <th className="p-2 w-[140px]">DESTINATION</th>
                      <th className="p-2 w-[45px] text-center">D-PORT</th>
                      <th className="p-2 w-[35px] text-center">CH</th>
                      <th className="p-2 w-[55px] text-right">LEN</th>
                      <th className="p-2 w-[40px] text-center">TTL</th>
                      <th className="p-2 w-[60px] text-right">RSSI</th>
                      <th className="p-2">INFO / PAYLOAD</th>
                    </tr>
                  </thead>
                  <tbody>
                    {filteredLogs.map((log, i) => (
                      <PacketRow 
                        key={`${log.time}-${i}`} 
                        log={log} 
                        seq={logs.length - i} 
                        onInspect={onInspect} 
                      />
                    ))}
                  </tbody>
                </table>
              </div>
              {/* Mobile Card View */}
              <div className="md:hidden p-3 space-y-3">
                {filteredLogs.map((log, i) => (
                  <PacketCard 
                    key={`${log.time}-${i}`} 
                    log={log} 
                    seq={logs.length - i} 
                    onInspect={onInspect} 
                  />
                ))}
              </div>
            </div>
            
            <div className="bg-white/[0.02] px-4 py-1.5 border-t border-white/5 flex justify-between items-center text-[8px] font-bold text-gray-600 uppercase tracking-widest">
               <span>Buffer: {logs.length}/500</span>
               <span className="text-accent/40 animate-pulse">SSE ACTIVE</span>
            </div>
          </div>
        </div>
      )}
    </div>
  );
};
