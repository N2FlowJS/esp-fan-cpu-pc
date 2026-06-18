import React, { useState, useEffect } from "react";
import { SystemStatus, SnifferFilters } from "../../types";
import {
  Cpu,
  Wifi,
  Database,
  Info,
  RefreshCw,
  Activity,
  Monitor,
  Lock,
  Eye,
  EyeOff,
  Shield,
  Filter,
  Plus,
  X,
  Search,
  ChevronLeft,
  ChevronRight,
  ChevronUp,
  ChevronDown,
} from "lucide-react";
import { formatUptime, formatBytes } from "../../utils/formatters";
import { apiSaveWiFi, apiSavePassword, setLedConfig, apiGetSnifferFilters, apiSaveSnifferFilters, apiGetWiFiScan, apiToggleWiFi, apiGetFanCurve, apiSetFanCurve } from "../../utils/api";
import { useStore } from "../../store/useStore";

interface SystemTabProps {
  status: SystemStatus | null;
}

const MacFilterManager: React.FC = () => {
  const [filters, setFilters] = useState<SnifferFilters>({ whitelist: [], blacklist: [] });
  const [newMac, setNewMac] = useState('');
  const [type, setType] = useState<'white' | 'black'>('black');

  const load = async () => {
    const data = await apiGetSnifferFilters();
    if (data) setFilters(data);
  };

  useEffect(() => { load(); }, []);

  const add = async () => {
    if (!/^([0-9A-Fa-f]{2}[:-]){5}([0-9A-Fa-f]{2})$/.test(newMac)) return alert('Invalid MAC');
    const nw = type === 'white' ? [...filters.whitelist, newMac.toUpperCase()] : filters.whitelist;
    const nb = type === 'black' ? [...filters.blacklist, newMac.toUpperCase()] : filters.blacklist;
    await apiSaveSnifferFilters(nw, nb);
    setNewMac('');
    load();
  };

  const remove = async (t: 'white' | 'black', m: string) => {
    const nw = t === 'white' ? filters.whitelist.filter(x => x !== m) : filters.whitelist;
    const nb = t === 'black' ? filters.blacklist.filter(x => x !== m) : filters.blacklist;
    await apiSaveSnifferFilters(nw, nb);
    load();
  };

  return (
    <section className="section-card">
      <div className="flex items-center gap-2 mb-5">
        <div className="w-1.5 h-3.5 bg-accent rounded-full shadow-[0_0_10px_rgba(99,179,237,0.8)]" />
        <h2 className="text-[11px] font-extrabold tracking-widest text-accent uppercase">
          MAC FILTER MANAGEMENT
        </h2>
      </div>
      
      <div className="space-y-4">
        <div className="flex gap-2">
           <select 
            value={type} onChange={e => setType(e.target.value as any)}
            className="bg-black/40 border border-white/10 rounded-xl px-3 text-[9px] font-black text-accent outline-none"
           >
             <option value="black">BLACKLIST</option>
             <option value="white">WHITELIST</option>
           </select>
           <input 
            type="text" placeholder="XX:XX:XX:XX:XX:XX" value={newMac}
            onChange={e => setNewMac(e.target.value)}
            className="flex-1 bg-black/40 border border-white/10 rounded-xl px-4 py-3 text-xs font-mono text-white outline-none focus:border-accent/50"
           />
           <button onClick={add} className="p-3 rounded-xl bg-accent/10 border border-accent/30 text-accent hover:bg-accent/20 transition-all">
             <Plus size={18} />
           </button>
        </div>

        <div className="space-y-3">
           <div className="space-y-1.5">
             <div className="text-[8px] font-black text-red/60 uppercase tracking-widest ml-1">Blacklisted (Blocked)</div>
             <div className="flex flex-wrap gap-2">
                {filters.blacklist.map(m => (
                  <span key={m} className="px-2 py-1 rounded-lg bg-red/10 border border-red/20 text-[9px] font-mono text-red flex items-center gap-1.5">
                    {m} <button onClick={() => remove('black', m)}><X size={10} /></button>
                  </span>
                ))}
                {filters.blacklist.length === 0 && <span className="text-[8px] text-gray-700 italic ml-1">None</span>}
             </div>
           </div>

           <div className="space-y-1.5">
             <div className="text-[8px] font-black text-green/60 uppercase tracking-widest ml-1">Whitelisted (Priority)</div>
             <div className="flex flex-wrap gap-2">
                {filters.whitelist.map(m => (
                  <span key={m} className="px-2 py-1 rounded-lg bg-green/10 border border-green/20 text-[9px] font-mono text-green flex items-center gap-1.5">
                    {m} <button onClick={() => remove('white', m)}><X size={10} /></button>
                  </span>
                ))}
                {filters.whitelist.length === 0 && <span className="text-[8px] text-gray-700 italic ml-1">None (All allowed)</span>}
             </div>
           </div>
        </div>
      </div>
    </section>
  );
};

const FanCurveEditor: React.FC = () => {
  const [curve, setCurve] = useState<{ temp: number, speed: number }[]>([]);
  const [loading, setLoading] = useState(true);

  const load = async () => {
    setLoading(true);
    const data = await apiGetFanCurve();
    if (data) setCurve(data);
    setLoading(false);
  };

  useEffect(() => { load(); }, []);

  const updatePoint = (index: number, field: 'temp' | 'speed', val: number) => {
    const next = [...curve];
    next[index] = { ...next[index], [field]: val };
    setCurve(next);
  };

  const addPoint = () => {
    if (curve.length >= 10) return;
    const last = curve[curve.length - 1] || { temp: 30, speed: 20 };
    setCurve([...curve, { temp: last.temp + 10, speed: Math.min(100, last.speed + 15) }]);
  };

  const removePoint = (index: number) => {
    setCurve(curve.filter((_, i) => i !== index));
  };

  const save = async () => {
    // Sort curve by temp before saving
    const sorted = [...curve].sort((a, b) => a.temp - b.temp);
    await apiSetFanCurve(sorted);
    setCurve(sorted);
    alert('Fan curve updated!');
  };

  return (
    <section className="section-card">
      <div className="flex items-center gap-2 mb-5">
        <div className="w-1.5 h-3.5 bg-warm rounded-full shadow-[0_0_10px_rgba(246,173,85,0.8)]" />
        <h2 className="text-[11px] font-extrabold tracking-widest text-warm uppercase">
          DYNAMIC FAN CURVE EDITOR
        </h2>
      </div>

      <div className="space-y-3">
        {loading ? (
          <div className="text-center py-4 text-[10px] text-gray-600 font-black animate-pulse uppercase">Loading curve data...</div>
        ) : (
          <>
            <div className="grid grid-cols-12 gap-2 px-1 text-[8px] font-black text-gray-600 uppercase tracking-widest mb-1">
               <div className="col-span-5">Temp (°C)</div>
               <div className="col-span-5">Speed (%)</div>
               <div className="col-span-2 text-center">Del</div>
            </div>
            
            <div className="space-y-2">
              {curve.map((p, i) => (
                <div key={i} className="grid grid-cols-12 gap-2 items-center">
                   <div className="col-span-5 relative">
                      <input 
                        type="number" value={p.temp} 
                        onChange={e => updatePoint(i, 'temp', parseFloat(e.target.value))}
                        className="w-full bg-black/40 border border-white/10 rounded-xl px-3 py-2.5 text-xs font-bold text-warm outline-none focus:border-warm/40"
                      />
                   </div>
                   <div className="col-span-5 relative">
                      <input 
                        type="number" value={p.speed} 
                        onChange={e => updatePoint(i, 'speed', parseInt(e.target.value))}
                        className="w-full bg-black/40 border border-white/10 rounded-xl px-3 py-2.5 text-xs font-bold text-accent outline-none focus:border-accent/40"
                      />
                   </div>
                   <div className="col-span-2 flex justify-center">
                      <button 
                        onClick={() => removePoint(i)}
                        className="p-2.5 rounded-xl bg-red/5 border border-red/20 text-red hover:bg-red/10 transition-all"
                      >
                        <X size={14} />
                      </button>
                   </div>
                </div>
              ))}
            </div>

            <div className="flex gap-2 mt-4">
               <button 
                onClick={addPoint}
                className="flex-1 py-3 rounded-2xl bg-white/5 border border-white/10 text-gray-500 font-black text-[10px] tracking-widest uppercase hover:bg-white/10 transition-all flex items-center justify-center gap-2"
               >
                 <Plus size={12} /> ADD POINT
               </button>
               <button 
                onClick={save}
                className="flex-1 py-3 rounded-2xl bg-warm text-bg font-black text-[10px] tracking-widest uppercase hover:brightness-110 transition-all flex items-center justify-center gap-2 shadow-lg shadow-warm/20"
               >
                 <RefreshCw size={12} /> SAVE CURVE
               </button>
            </div>
          </>
        )}
      </div>
    </section>
  );
};

export const SystemTab: React.FC<SystemTabProps> = ({ status }) => {
  const { setAuthenticated } = useStore();
  const [ssid, setSsid] = useState("");
  const [pass, setPass] = useState("");
  const [newWebPass, setNewWebPass] = useState("");
  const [showWifiPass, setShowWifiPass] = useState(false);
  const [showNewWebPass, setShowNewWebPass] = useState(false);

  const [networks, setNetworks] = useState<any[]>([]);
  const [isScanning, setIsScanning] = useState(false);
  const [networkSearch, setNetworkSearch] = useState("");
  const [currentPage, setCurrentPage] = useState(1);
  const [pageSize, setPageSize] = useState(5);
  const [sortDir, setSortConfig] = useState<'asc' | 'desc'>('desc');

  const scanWiFi = async () => {
    setIsScanning(true);
    try {
      const data = await apiGetWiFiScan();
      if (Array.isArray(data)) {
        setNetworks(data);
        setIsScanning(false);
      } else if (data?.status === "scanning") {
        setTimeout(scanWiFi, 2000);
      } else {
        setIsScanning(false);
      }
    } catch {
      setIsScanning(false);
    }
  };

  useEffect(() => {
    scanWiFi();
  }, []);

  const filteredNetworks = React.useMemo(() => {
    let result = (networks || []).filter(net => 
      (net?.ssid || "").toLowerCase().includes((networkSearch || "").toLowerCase())
    );
    
    result.sort((a, b) => {
      const modifier = sortDir === 'asc' ? 1 : -1;
      return (a.rssi - b.rssi) * modifier;
    });

    return result;
  }, [networks, networkSearch, sortDir]);

  useEffect(() => {
    setCurrentPage(1);
  }, [networkSearch, pageSize]);

  const totalPages = Math.ceil(filteredNetworks.length / pageSize);
  const paginatedNetworks = filteredNetworks.slice((currentPage - 1) * pageSize, currentPage * pageSize);

  // Pre-populate SSID and Password fields with configured network if not edited by the user
  useEffect(() => {
    if (status?.staSSID && !ssid) {
      setSsid(status.staSSID);
    }
    if (status?.staPass && !pass) {
      setPass(status.staPass);
    }
  }, [status?.staSSID, status?.staPass]);

  const saveWiFi = async () => {
    if (!ssid) return alert("Please enter SSID");
    if (!confirm("ESP32 will save and reboot. Continue?")) return;
    try {
      await apiSaveWiFi(ssid, pass);
      alert("Settings sent. Please wait for reboot.");
    } catch {
      alert("Failed to save config!");
    }
  };

  const savePassword = async () => {
    if (newWebPass.length < 4) return alert("Password too short (min 4 chars)");
    if (!confirm("Change access key? You will need to login again.")) return;
    try {
      await apiSavePassword(newWebPass);
      alert("Password updated! Logging out...");
      setAuthenticated(false);
      window.location.reload();
    } catch (err: any) {
      alert(err.message || "Failed to update password!");
    }
  };

  const saveLedPin = async (pin: number) => {
    try {
      await setLedConfig(status?.ledMode || 'auto', status?.ledColor, status?.ledBrightness, pin);
      alert(`LED pin updated to ${pin}`);
    } catch {
      alert("Failed to update LED pin!");
    }
  };

  const toggleWiFi = async () => {
    const newState = !status?.staEnabled;
    if (!confirm(`Turn WiFi Station ${newState ? 'ON' : 'OFF'}? Device will reboot.`)) return;
    try {
      await apiToggleWiFi(newState);
      alert("Settings sent. Please wait for reboot.");
    } catch {
      alert("Failed to toggle WiFi!");
    }
  };

  const infoItems = [
    { label: "Chip", value: status?.chip ? `${status.chip} (Rev ${status.chipRev})` : null, icon: Cpu },
    {
      label: "CPU",
      value: status?.cpu ? `${status.cpu} MHz` : null,
      icon: Activity,
    },
    { label: "Uptime", value: status?.uptime != null ? formatUptime(status.uptime) : null, icon: RefreshCw },
    { label: "Heap", value: status?.freeHeap != null ? formatBytes(status.freeHeap) : null, icon: Database },
    { label: "PSRAM", value: status?.totalPsram ? `${formatBytes(status.freePsram)} / ${formatBytes(status.totalPsram)}` : null, icon: Database },
    { label: "Flash", value: status?.flash != null ? formatBytes(status.flash) : null, icon: Info },
    { label: "Storage", value: status?.spiffsTotal != null ? `${formatBytes(status.spiffsUsed)} / ${formatBytes(status.spiffsTotal)}` : null, icon: Database },
    { label: "SDK", value: status?.sdk, icon: Info },
    { label: "Firmware", value: status?.version, icon: Info },
    {
      label: "SSID",
      value: status?.staSSID || "Not Configured",
      icon: Wifi,
    },
    {
      label: "STA IP",
      value: status?.staIP || "N/A",
      icon: Monitor,
      accent: true,
    },
    {
      label: "AP IP",
      value: status?.apIP || "192.168.4.1",
      icon: Wifi,
    },
    {
      label: "Signal",
      value: status?.rssi != null && status.rssi !== 0 ? `${status.rssi} dBm` : null,
      icon: Wifi,
    },
  ];

  return (
    <div className="space-y-4 animate-fade-up">
      <section className="section-card">
        <div className="flex items-center gap-2 mb-5">
          <div className="w-1.5 h-3.5 bg-accent rounded-full shadow-[0_0_10px_rgba(99,179,237,0.8)]" />
          <h2 className="text-[11px] font-extrabold tracking-widest text-accent uppercase">
            SYSTEM INFORMATION
          </h2>
        </div>

        <div className="grid grid-cols-1 sm:grid-cols-2 gap-x-6 gap-y-4">
          {infoItems.map((item, i) => {
            const IconComponent = item.icon;
            return (
              <div
                key={i}
                className="flex flex-col gap-1 border-b border-white/[0.03] pb-2 sm:last:border-0"
              >
                <div className="flex items-center gap-1.5 text-gray-600">
                  {IconComponent && <IconComponent size={10} />}
                  <span className="text-[8px] font-black uppercase tracking-wider">
                    {item.label}
                  </span>
                </div>
                <span
                  className={`text-[10px] font-black ${item.accent ? "text-accent" : "text-gray-300"} truncate`}
                >
                  {item.value || "–"}
                </span>
              </div>
            );
          })}
        </div>
      </section>

      <FanCurveEditor />

      <MacFilterManager />

      <section className="section-card">
        <div className="flex items-center gap-2 mb-5">
          <div className="w-1.5 h-3.5 bg-green rounded-full shadow-[0_0_10px_rgba(104,211,145,0.8)]" />
          <h2 className="text-[11px] font-extrabold tracking-widest text-green uppercase flex-1">
            WIFI PROVISIONING
          </h2>
          <button 
            onClick={scanWiFi}
            disabled={isScanning}
            className={`p-2 rounded-lg text-green transition-all ${isScanning ? 'animate-spin' : 'hover:bg-white/10'}`}
          >
            <RefreshCw size={14} />
          </button>
        </div>
        <div className="space-y-4">
          <div className="flex items-center justify-between p-4 bg-black/40 border border-white/10 rounded-2xl mb-4">
            <div className="flex items-center gap-3">
              <div className={`p-2 rounded-xl ${status?.staEnabled ? 'bg-green/10 text-green' : 'bg-red/10 text-red'}`}>
                <Wifi size={20} />
              </div>
              <div>
                <div className="text-[10px] font-black text-white uppercase tracking-widest">WIFI STATION</div>
                <div className={`text-[8px] font-bold uppercase tracking-tighter ${status?.staEnabled ? 'text-green' : 'text-red'}`}>
                  {status?.staEnabled ? 'ENABLED' : 'DISABLED'}
                </div>
              </div>
            </div>
            <button
              onClick={toggleWiFi}
              className={`px-5 py-2.5 rounded-xl font-black text-[9px] tracking-widest uppercase transition-all active:scale-95 ${
                status?.staEnabled 
                  ? 'bg-red/10 border border-red/30 text-red hover:bg-red/20' 
                  : 'bg-green/10 border border-green/30 text-green hover:bg-green/20'
              }`}
            >
              {status?.staEnabled ? 'TURN OFF' : 'TURN ON'}
            </button>
          </div>

          <div className="space-y-1.5">
            <div className="flex items-center justify-between mb-2 px-1">
              <div className="flex items-center gap-3">
                <label className="text-[9px] font-black text-gray-600 uppercase flex items-center gap-2">
                  <span>Select Network</span>
                  {(networks || []).length > 0 && <span className="text-accent/60 italic">({filteredNetworks.length} found)</span>}
                </label>
                <select
                  className="bg-black/60 text-[8px] font-bold border border-white/5 rounded-lg px-1.5 py-0.5 outline-none text-gray-400 focus:border-accent/40"
                  value={pageSize}
                  onChange={(e) => setPageSize(parseInt(e.target.value))}
                >
                  {[5, 10, 20].map((v) => (
                    <option key={v} value={v}>
                      {v} / PAGE
                    </option>
                  ))}
                </select>
                <button
                  onClick={() => setSortConfig(s => s === 'asc' ? 'desc' : 'asc')}
                  className="bg-black/60 text-[8px] font-bold border border-white/5 rounded-lg px-1.5 py-0.5 text-gray-400 hover:text-accent flex items-center gap-1"
                >
                  RSSI {sortDir === 'asc' ? <ChevronUp size={8}/> : <ChevronDown size={8}/>}
                </button>
              </div>
              {totalPages > 1 && (
                <div className="flex items-center gap-1 bg-black/40 rounded-lg p-0.5 border border-white/5">
                  <button
                    disabled={currentPage === 1}
                    onClick={() => setCurrentPage(p => Math.max(1, p - 1))}
                    className="p-1 rounded hover:bg-white/5 text-gray-500 hover:text-white disabled:opacity-10 transition-all"
                  >
                    <ChevronLeft size={12} />
                  </button>
                  <span className="text-[8px] font-black text-gray-500 min-w-[30px] text-center">
                    {currentPage} / {totalPages}
                  </span>
                  <button
                    disabled={currentPage >= totalPages}
                    onClick={() => setCurrentPage(p => Math.min(totalPages, p + 1))}
                    className="p-1 rounded hover:bg-white/5 text-gray-500 hover:text-white disabled:opacity-10 transition-all"
                  >
                    <ChevronRight size={12} />
                  </button>
                </div>
              )}
            </div>

            <div className="relative mb-2">
              <input
                type="text"
                placeholder="Search network name..."
                value={networkSearch}
                onChange={(e) => setNetworkSearch(e.target.value)}
                className="w-full bg-black/60 border border-white/5 rounded-xl px-4 py-2.5 text-[10px] font-bold text-accent outline-none focus:border-accent/30 transition-all placeholder:text-gray-700"
              />
              <Search size={12} className="absolute right-4 top-1/2 -translate-y-1/2 text-gray-700" />
            </div>

            <div className="grid grid-cols-1 gap-2 max-h-[300px] overflow-y-auto pr-1 scrollbar-thin scrollbar-thumb-white/10">
              {paginatedNetworks.map((net, i) => (
                <button
                  key={i}
                  onClick={() => setSsid(net?.ssid || "")}
                  className={`flex items-center justify-between px-4 py-3 rounded-xl border transition-all text-left ${ssid === net?.ssid ? 'bg-green/10 border-green/40 text-green' : 'bg-black/40 border-white/10 text-gray-400 hover:border-white/20'}`}
                >
                  <div className="flex items-center gap-2">
                    <Wifi size={14} className={ssid === net?.ssid ? 'text-green' : 'text-gray-600'} />
                    <span className="text-xs font-bold truncate max-w-[150px]">{net?.ssid || '<Hidden>'}</span>
                    {net?.secure && <Lock size={10} className="text-gray-700" />}
                  </div>
                  <span className="text-[9px] font-black opacity-60">{net?.rssi || '?'} dBm</span>
                </button>
              ))}
              {filteredNetworks.length === 0 && !isScanning && (
                <div className="text-center py-4 text-[10px] text-gray-600 font-bold uppercase italic">
                  {networkSearch ? "No networks match your search" : "No networks found. Try scanning again."}
                </div>
              )}
              {isScanning && (
                <div className="text-center py-4 text-[10px] text-green font-black uppercase animate-pulse">
                  Scanning nearby networks...
                </div>
              )}
            </div>

            <div className="mt-3">
              <label className="text-[9px] font-black text-gray-600 uppercase ml-1">
                Or Manual Entry
              </label>
              <input
                type="text"
                placeholder="Enter SSID manually"
                value={ssid}
                onChange={(e) => setSsid(e.target.value)}
                className="w-full bg-black/40 border border-white/10 rounded-xl px-4 py-3 mt-1 text-xs font-bold text-accent outline-none focus:border-accent/50 focus:bg-white/[0.05] transition-all"
              />
            </div>
          </div>

          <div className="space-y-1.5">
            <label className="text-[9px] font-black text-gray-600 uppercase ml-1">
              Password
            </label>
            <div className="relative">
              <input
                type={showWifiPass ? "text" : "password"}
                placeholder="Enter WiFi Password"
                value={pass}
                onChange={(e) => setPass(e.target.value)}
                className="w-full bg-black/40 border border-white/10 rounded-xl px-4 py-3 pr-12 text-xs font-bold text-accent outline-none focus:border-accent/50 focus:bg-white/[0.05] transition-all"
              />
              <button
                type="button"
                onClick={() => setShowWifiPass(!showWifiPass)}
                className="absolute inset-y-0 right-0 pr-4 flex items-center text-gray-500 hover:text-accent transition-colors focus:outline-none"
              >
                {showWifiPass ? <EyeOff size={16} /> : <Eye size={16} />}
              </button>
            </div>
          </div>
          <button
            onClick={saveWiFi}
            className="w-full py-4 rounded-2xl bg-green/10 border border-green/30 text-green font-black text-[10px] tracking-[0.2em] shadow-lg active:scale-[0.98] hover:bg-green/20 transition-all mt-2"
          >
            SAVE & REBOOT DEVICE
          </button>
        </div>
      </section>

      <section className="section-card">
        <div className="flex items-center gap-2 mb-5">
          <div className="w-1.5 h-3.5 bg-accent rounded-full shadow-[0_0_10px_rgba(99,179,237,0.8)]" />
          <h2 className="text-[11px] font-extrabold tracking-widest text-accent uppercase">
            SECURITY & ACCESS
          </h2>
        </div>
        <div className="space-y-4">
          <div className="space-y-1.5">
            <label className="text-[9px] font-black text-gray-600 uppercase ml-1 flex items-center gap-1">
              <Lock size={8} /> New Access Key
            </label>
            <div className="relative">
              <input
                type={showNewWebPass ? "text" : "password"}
                placeholder="Min 4 characters"
                value={newWebPass}
                onChange={(e) => setNewWebPass(e.target.value)}
                className="w-full bg-black/40 border border-white/10 rounded-xl px-4 py-3 pr-12 text-xs font-bold text-accent outline-none focus:border-accent/50 focus:bg-white/[0.05] transition-all"
              />
              <button
                type="button"
                onClick={() => setShowNewWebPass(!showNewWebPass)}
                className="absolute inset-y-0 right-0 pr-4 flex items-center text-gray-500 hover:text-accent transition-colors focus:outline-none"
              >
                {showNewWebPass ? <EyeOff size={16} /> : <Eye size={16} />}
              </button>
            </div>
          </div>
          <button
            onClick={savePassword}
            className="w-full py-4 rounded-2xl bg-accent/10 border border-accent/30 text-accent font-black text-[10px] tracking-[0.2em] shadow-lg active:scale-[0.98] hover:bg-accent/20 transition-all mt-2"
          >
            UPDATE ACCESS KEY
          </button>
        </div>
      </section>

      <section className="section-card">
        <div className="flex items-center gap-2 mb-5">
          <div className="w-1.5 h-3.5 bg-purple-500 rounded-full shadow-[0_0_10px_rgba(168,85,247,0.8)]" />
          <h2 className="text-[11px] font-extrabold tracking-widest text-purple-400 uppercase">
            LED HARDWARE CONFIG
          </h2>
        </div>
        <div className="space-y-4">
          <div className="space-y-3">
             <label className="text-[9px] font-black text-gray-600 uppercase ml-1 block">
               Built-in RGB LED Pin (Common: 48, 38, 18)
             </label>
             <div className="grid grid-cols-4 gap-2">
                {[48, 38, 18, 47].map(pin => (
                  <button 
                    key={pin}
                    onClick={() => saveLedPin(pin)}
                    className={`py-3 rounded-xl border text-[10px] font-black transition-all ${status?.ledPin === pin ? 'bg-purple-500 border-purple-400 text-white shadow-lg shadow-purple-500/20' : 'bg-black/40 border-white/10 text-gray-500 hover:text-white'}`}
                  >
                    GPIO {pin}
                  </button>
                ))}
             </div>
             <div className="mt-2 flex gap-2">
                <input 
                  type="number" 
                  placeholder="Custom Pin"
                  onKeyDown={(e) => {
                    if (e.key === 'Enter') {
                      saveLedPin(parseInt((e.target as HTMLInputElement).value));
                    }
                  }}
                  className="flex-1 bg-black/40 border border-white/10 rounded-xl px-4 py-3 text-xs font-bold text-purple-400 outline-none focus:border-purple-500/50 transition-all"
                />
             </div>
          </div>
          <p className="text-[8px] font-bold text-gray-600 italic leading-relaxed px-1">
            * If your LED doesn't light up, try different GPIO pins from the list above. Pin 48 is standard for ESP32-S3 DevKitC.
          </p>
        </div>
      </section>

      <section className="section-card border-red/20 bg-red/5">
        <div className="flex items-center gap-2 mb-5">
          <div className="w-1.5 h-3.5 bg-red rounded-full shadow-[0_0_10px_rgba(245,101,101,0.8)]" />
          <h2 className="text-[11px] font-extrabold tracking-widest text-red uppercase">
            ADVANCED OPTIONS
          </h2>
        </div>
        <div className="space-y-4">
          <div className="p-3 bg-black/40 border border-white/5 rounded-xl">
             <div className="text-[9px] font-black text-gray-400 uppercase mb-1">Clear Local Storage</div>
             <p className="text-[8px] text-gray-500 leading-relaxed mb-3">
               This will wipe all locally saved UI state, tokens, and preferences. Use this if the web interface is behaving unexpectedly. You will be logged out.
             </p>
             <button
               onClick={() => {
                 if (window.confirm("Are you sure you want to wipe all local web data? This will log you out.")) {
                   useStore.getState().clearAllState();
                   useStore.persist.clearStorage();
                   localStorage.clear();
                   sessionStorage.clear();
                   window.location.href = '/';
                 }
               }}
               className="px-4 py-2.5 rounded-xl bg-red/10 border border-red/30 text-red text-[9px] font-black tracking-widest uppercase hover:bg-red/20 transition-all active:scale-[0.98]"
             >
               CLEAR WEB DATA
             </button>
          </div>
        </div>
      </section>
    </div>
  );
};
