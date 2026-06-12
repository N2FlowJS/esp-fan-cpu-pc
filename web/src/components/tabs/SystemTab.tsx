import React, { useState } from "react";
import { SystemStatus } from "../../types";
import {
  Cpu,
  Wifi,
  Database,
  Info,
  RefreshCw,
  Activity,
  Monitor,
} from "lucide-react";
import { formatUptime, formatBytes } from "../../utils/formatters";
import { apiSaveWiFi } from "../../utils/api";

interface SystemTabProps {
  status: SystemStatus | null;
}

export const SystemTab: React.FC<SystemTabProps> = ({ status }) => {
  const [ssid, setSsid] = useState("");
  const [pass, setPass] = useState("");

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

        <div className="grid grid-cols-2 gap-x-6 gap-y-4">
          {infoItems.map((item, i) => (
            <div
              key={i}
              className="flex flex-col gap-1 border-b border-white/[0.03] pb-2"
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
            <input
              type="password"
              placeholder="••••••••"
              value={pass}
              onChange={(e) => setPass(e.target.value)}
              className="w-full bg-black/40 border border-white/10 rounded-xl px-4 py-3 text-xs font-bold text-accent outline-none focus:border-accent/50 focus:bg-white/[0.05] transition-all"
            />
          </div>
          <button
            onClick={saveWiFi}
            className="w-full py-4 rounded-2xl bg-green/10 border border-green/30 text-green font-black text-[10px] tracking-[0.2em] shadow-lg active:scale-[0.98] hover:bg-green/20 transition-all mt-2"
          >
            SAVE & REBOOT DEVICE
          </button>
        </div>
      </section>
    </div>
  );
};
