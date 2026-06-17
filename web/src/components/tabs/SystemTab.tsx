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
} from "lucide-react";
import { formatUptime, formatBytes } from "../../utils/formatters";
import { apiSaveWiFi, apiSavePassword, setLedConfig, apiGetSnifferFilters, apiSaveSnifferFilters } from "../../utils/api";
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

export const SystemTab: React.FC<SystemTabProps> = ({ status }) => {
  const { setAuthenticated } = useStore();
  const [ssid, setSsid] = useState("");
  const [pass, setPass] = useState("");
  const [newWebPass, setNewWebPass] = useState("");
  const [showWifiPass, setShowWifiPass] = useState(false);
  const [showNewWebPass, setShowNewWebPass] = useState(false);

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

  const infoItems = [
    { label: "Chip", value: status?.chip, icon: Cpu },
    {
      label: "CPU",
      value: status?.cpu ? `${status.cpu} MHz` : "–",
      icon: Activity,
    },
    { label: "Uptime", value: formatUptime(status?.uptime), icon: RefreshCw },
    { label: "Heap", value: formatBytes(status?.freeHeap), icon: Database },
    { label: "Flash", value: formatBytes(status?.flash), icon: Info },
    { label: "SDK", value: status?.sdk, icon: Info },
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
      label: "Signal",
      value: status?.rssi ? `${status.rssi} dBm` : "N/A",
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
          {infoItems.map((item, i) => (
            <div
              key={i}
              className="flex flex-col gap-1 border-b border-white/[0.03] pb-2 sm:last:border-0"
            >
              <div className="flex items-center gap-1.5 text-gray-600">
                <item.icon size={10} />
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
          ))}
        </div>
      </section>

      <MacFilterManager />

      <section className="section-card">
        <div className="flex items-center gap-2 mb-5">
          <div className="w-1.5 h-3.5 bg-green rounded-full shadow-[0_0_10px_rgba(104,211,145,0.8)]" />
          <h2 className="text-[11px] font-extrabold tracking-widest text-green uppercase">
            WIFI PROVISIONING
          </h2>
        </div>
        <div className="space-y-4">
          <div className="space-y-1.5">
            <label className="text-[9px] font-black text-gray-600 uppercase ml-1">
              SSID / Network Name
            </label>
            <input
              type="text"
              placeholder="Enter SSID"
              value={ssid}
              onChange={(e) => setSsid(e.target.value)}
              className="w-full bg-black/40 border border-white/10 rounded-xl px-4 py-3 text-xs font-bold text-accent outline-none focus:border-accent/50 focus:bg-white/[0.05] transition-all"
            />
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
    </div>
  );
};
