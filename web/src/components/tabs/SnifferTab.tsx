import React, { useState, useEffect } from "react";
import {
  Monitor,
  AlertTriangle,
  Trash2,
  Search,
  X,
  Shield,
  Cpu,
  Activity,
  Wifi,
  Database,
  Filter,
  Save,
  Plus,
  Target,
  Skull,
  ChevronLeft,
  ChevronRight,
  ChevronUp,
  ChevronDown,
} from "lucide-react";
import { getProtoColor, getRssiColor } from "../../utils/formatters";
import { useStore } from "../../store/useStore";
import { PacketLog, DeviceInfo, SnifferFilters } from "../../types";
import {
  apiSnifferControl,
  apiGetSnifferFilters,
  apiSaveSnifferFilters,
  apiRegisterOwnerMac,
  apiStressControl,
  apiPing,
} from "../../utils/api";
import { getMacVendor } from "../../utils/macLookup";

interface SnifferTabProps {
  onInspect: (log: PacketLog, seq: number) => void;
}

const FilterModal: React.FC<{ onClose: () => void }> = ({ onClose }) => {
  const [filters, setFilters] = useState<SnifferFilters>({
    whitelist: [],
    blacklist: [],
  });
  const [loading, setLoading] = useState(true);
  const [newWhite, setNewWhite] = useState("");
  const [newBlack, setNewBlack] = useState("");

  useEffect(() => {
    let mounted = true;
    apiGetSnifferFilters()
      .then((data) => {
        if (!mounted) return;
        if (data) setFilters(data);
        setLoading(false);
      })
      .catch(() => setLoading(false));
    return () => {
      mounted = false;
    };
  }, []);

  const handleSave = async () => {
    await apiSaveSnifferFilters(filters.whitelist, filters.blacklist);
    onClose();
  };

  const addMAC = (type: "white" | "black") => {
    const val = (type === "white" ? newWhite : newBlack).trim();
    if (!/^([0-9A-Fa-f]{2}[:-]){5}([0-9A-Fa-f]{2})$/.test(val)) return;
    const upper = val.toUpperCase();
    setFilters((prev) => ({
      ...prev,
      [type === "white" ? "whitelist" : "blacklist"]: [
        ...prev[type === "white" ? "whitelist" : "blacklist"],
        upper,
      ],
    }));
    if (type === "white") setNewWhite("");
    else setNewBlack("");
  };

  const removeMAC = (type: "white" | "black", index: number) => {
    setFilters((prev) => ({
      ...prev,
      [type === "white" ? "whitelist" : "blacklist"]: prev[
        type === "white" ? "whitelist" : "blacklist"
      ].filter((_, i) => i !== index),
    }));
  };

  if (loading) {
    return (
      <div className="fixed inset-0 z-[60] flex items-center justify-center p-4 bg-bg/80 backdrop-blur-sm">
        <div className="text-white font-black">Loading filters...</div>
      </div>
    );
  }

  return (
    <div className="fixed inset-0 z-[60] flex items-center justify-center p-4 bg-bg/80 backdrop-blur-sm animate-in fade-in duration-300">
      <div className="w-full max-w-2xl bg-[#0a0c12] border border-white/10 rounded-3xl shadow-2xl overflow-hidden flex flex-col">
        <div className="bg-white/[0.03] px-6 py-4 border-b border-white/10 flex items-center justify-between">
          <div className="text-[10px] font-black text-gray-300 uppercase tracking-widest">
            MAC Filters
          </div>
          <div className="flex items-center gap-2">
            <button
              onClick={handleSave}
              className="px-3 py-2 bg-accent text-bg rounded-lg font-black"
            >
              Save
            </button>
            <button
              onClick={onClose}
              className="px-3 py-2 bg-white/5 text-gray-300 rounded-lg"
            >
              Close
            </button>
          </div>
        </div>

        <div className="p-4 grid grid-cols-1 sm:grid-cols-2 gap-4">
          <div>
            <div className="text-[9px] font-black text-gray-500 uppercase mb-2">
              Whitelist
            </div>
            <div className="space-y-2 max-h-40 overflow-y-auto pr-1 scrollbar-thin scrollbar-thumb-white/10">
              {filters.whitelist.map((m, i) => (
                <div
                  key={i}
                  className="flex items-center justify-between bg-white/[0.02] p-2 rounded"
                >
                  <div className="font-mono text-sm">{m}</div>
                  <button
                    onClick={() => removeMAC("white", i)}
                    className="text-red p-1"
                  >
                    Remove
                  </button>
                </div>
              ))}
            </div>
            <div className="mt-2 flex gap-2">
              <input
                value={newWhite}
                onChange={(e) => setNewWhite(e.target.value)}
                placeholder="AA:BB:CC:DD:EE:FF"
                className="flex-1 bg-black/40 border border-white/5 rounded px-2 py-1"
              />
              <button
                onClick={() => addMAC("white")}
                className="px-3 py-1 bg-green text-bg rounded"
              >
                Add
              </button>
            </div>
          </div>

          <div>
            <div className="text-[9px] font-black text-gray-500 uppercase mb-2">
              Blacklist
            </div>
            <div className="space-y-2 max-h-40 overflow-y-auto pr-1 scrollbar-thin scrollbar-thumb-white/10">
              {filters.blacklist.map((m, i) => (
                <div
                  key={i}
                  className="flex items-center justify-between bg-white/[0.02] p-2 rounded"
                >
                  <div className="font-mono text-sm">{m}</div>
                  <button
                    onClick={() => removeMAC("black", i)}
                    className="text-red p-1"
                  >
                    Remove
                  </button>
                </div>
              ))}
            </div>
            <div className="mt-2 flex gap-2">
              <input
                value={newBlack}
                onChange={(e) => setNewBlack(e.target.value)}
                placeholder="AA:BB:CC:DD:EE:FF"
                className="flex-1 bg-black/40 border border-white/5 rounded px-2 py-1"
              />
              <button
                onClick={() => addMAC("black")}
                className="px-3 py-1 bg-red text-bg rounded"
              >
                Add
              </button>
            </div>
          </div>
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
  return `${h.toString().padStart(2, "0")}:${m.toString().padStart(2, "0")}:${s.toString().padStart(2, "0")}.${ms.toString().padStart(3, "0")}`;
};

const DeviceModal = React.memo(
  ({
    device,
    allDevices,
    onClose,
    uptime,
    onUpdateFilters,
  }: {
    device: DeviceInfo;
    allDevices: DeviceInfo[];
    onClose: () => void;
    uptime: number;
    onUpdateFilters?: () => void;
  }) => {
    const stress = useStore((state) => state.stress);
    const [activeTab, setActiveTab] = useState<
      "info" | "connections" | "stress"
    >("info");
    const [loading, setLoading] = useState(false);
    const [rate, setRate] = useState(10);

    const lastSeenSec = device.lastSeen ? uptime - device.lastSeen : 0;
    const lastSeenStr =
      lastSeenSec < 1 ? "Just now" : `${Math.floor(lastSeenSec)}s ago`;

    const isStressActive = stress?.active;

    // Approximation of connections:
    // If it's an AP, find stations on the same channel.
    // If it's a Station, find APs on the same channel.
    const potentialConnections = (allDevices || []).filter(
      (d) =>
        d.mac !== device.mac &&
        d.isAP !== device.isAP &&
        d.channel === device.channel,
    );

    const handleQuickFilter = async (type: "white" | "black") => {
      const data = await apiGetSnifferFilters();
      if (!data) return;

      const list = type === "white" ? data.whitelist : data.blacklist;
      if (list.includes(device.mac)) {
        alert("Device already in list");
        return;
      }

      const newWhite =
        type === "white" ? [...data.whitelist, device.mac] : data.whitelist;
      const newBlack =
        type === "black" ? [...data.blacklist, device.mac] : data.blacklist;

      await apiSaveSnifferFilters(newWhite, newBlack);
      alert(
        `Added ${device.mac} to ${type === "white" ? "Whitelist" : "Blacklist"}`,
      );
      if (onUpdateFilters) onUpdateFilters();
      onClose();
    };

    const handleToggleStress = async () => {
      setLoading(true);
      try {
        if (isStressActive) {
          await apiStressControl(false);
        } else {
          await apiStressControl(
            true,
            "deauth",
            device.mac,
            "FF:FF:FF:FF:FF:FF",
            device.channel || 1,
            rate,
          );
        }
      } catch (err) {
        console.error("Stress control failed:", err);
      } finally {
        setLoading(false);
      }
    };

    return (
      <div className="fixed inset-0 z-[60] flex items-center justify-center p-4 bg-bg/80 backdrop-blur-sm animate-in fade-in duration-300">
        <div
          className="w-full max-w-sm bg-[#0a0c12] border border-white/10 rounded-3xl shadow-2xl overflow-hidden flex flex-col animate-in zoom-in-95 duration-300"
          onClick={(e) => e.stopPropagation()}
        >
          <div className="bg-white/[0.03] px-6 py-4 border-b border-white/10 flex items-center justify-between">
            <div className="flex items-center gap-2">
              <div
                className={`w-2 h-2 rounded-full animate-pulse ${device.isAP ? "bg-green" : "bg-accent"}`}
              />
              <span className="text-[10px] font-black text-gray-400 tracking-widest uppercase">
                DEVICE ANALYZER
              </span>
            </div>
            <button
              onClick={onClose}
              className="p-2 hover:bg-white/5 rounded-full transition-colors text-gray-500 hover:text-white"
            >
              <X size={18} />
            </button>
          </div>

          {/* Modal Tabs */}
          <div className="flex border-b border-white/5 bg-white/[0.01]">
            <button
              onClick={() => setActiveTab("info")}
              className={`flex-1 py-3 text-[9px] font-black tracking-[0.2em] uppercase transition-all ${activeTab === "info" ? "text-accent border-b-2 border-accent bg-accent/5" : "text-gray-600 hover:text-gray-400"}`}
            >
              DEVICE INFO
            </button>
            <button
              onClick={() => setActiveTab("connections")}
              className={`flex-1 py-3 text-[9px] font-black tracking-[0.2em] uppercase transition-all flex items-center justify-center gap-1 ${activeTab === "connections" ? "text-green border-b-2 border-green bg-green/5" : "text-gray-600 hover:text-gray-400"}`}
            >
              {device.isAP ? "STATIONS" : "ACCESS POINTS"}{" "}
              <span className="bg-white/10 px-1.5 rounded-full text-[7px]">
                {potentialConnections.length}
              </span>
            </button>
            <button
              onClick={() => setActiveTab("stress")}
              className={`flex-1 py-3 text-[9px] font-black tracking-[0.2em] uppercase transition-all ${activeTab === "stress" ? "text-red border-b-2 border-red bg-red/5" : "text-gray-600 hover:text-gray-400"}`}
            >
              STRESS TEST
            </button>
          </div>

          <div className="p-6 space-y-6 overflow-y-auto max-h-[60vh] scrollbar-thin scrollbar-thumb-white/10">
            <div className="text-center">
              <div className="text-[10px] font-bold text-gray-600 uppercase tracking-widest mb-1">
                MAC ADDRESS
              </div>
              <div className="text-xl font-black text-white tracking-wider font-mono">
                {device.mac}
              </div>
              <div className="mt-2 inline-block px-3 py-1 rounded-full bg-accent/10 border border-accent/20 text-accent text-[9px] font-black tracking-widest uppercase">
                {getMacVendor(device.mac) || "Unknown Vendor"}
              </div>
            </div>

            {activeTab === "info" ? (
              <div className="space-y-6 animate-in fade-in slide-in-from-bottom-2 duration-300">
                <div className="grid grid-cols-2 gap-2">
                  <button
                    onClick={() => handleQuickFilter("white")}
                    className="py-2.5 rounded-xl bg-green/10 border border-green/20 text-green text-[9px] font-black tracking-widest uppercase hover:bg-green/20 transition-all flex items-center justify-center gap-2"
                  >
                    <Shield size={12} /> + Whitelist
                  </button>
                  <button
                    onClick={() => handleQuickFilter("black")}
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
                    <div
                      className={`text-sm font-black ${device.isAP ? "text-green" : "text-accent"}`}
                    >
                      {device.isAP ? "Access Point" : "Station"}
                    </div>
                  </div>
                  <div className="bg-white/[0.02] border border-white/5 rounded-2xl p-4">
                    <div className="flex items-center gap-2 mb-2 text-gray-500 uppercase tracking-widest">
                      <Activity size={10} />
                      <span className="text-[8px] font-black">Signal</span>
                    </div>
                    <div
                      className={`text-sm font-black ${getRssiColor(device.rssi)}`}
                    >
                      {device.rssi} dBm
                    </div>
                  </div>
                </div>

                <div className="space-y-3">
                  <div className="bg-white/[0.02] border border-white/5 rounded-2xl p-4 flex items-center justify-between">
                    <div className="flex items-center gap-2 text-gray-500 uppercase tracking-widest">
                      <Wifi size={10} />
                      <span className="text-[8px] font-black">SSID / INFO</span>
                    </div>
                    <div className="text-xs font-bold text-gray-300">
                      {device.isAP
                        ? device.ssid || "<Hidden>"
                        : device.ssid
                          ? `Probe: ${device.ssid}`
                          : "Scanning"}
                    </div>
                  </div>

                  {/* Station AP Association Display */}
                  {!device.isAP && (
                    <div className="bg-white/[0.02] border border-white/5 rounded-2xl p-4 flex items-center justify-between">
                      <div className="flex items-center gap-2 text-gray-500 uppercase tracking-widest">
                        <Monitor size={10} />
                        <span className="text-[8px] font-black">
                          {device.bssid ? "Confirmed AP" : "Likely AP"}
                        </span>
                      </div>
                      <div
                        className={`text-[10px] font-bold truncate max-w-[150px] text-right ${device.bssid ? "text-green" : "text-accent"}`}
                      >
                        {(() => {
                          if (device.bssid) {
                            const ap = allDevices.find(
                              (d) => d.mac === device.bssid,
                            );
                            return ap?.ssid ? `${ap.ssid}` : device.bssid;
                          }
                          if (potentialConnections.length > 0) {
                            return (
                              potentialConnections[0].ssid ||
                              potentialConnections[0].mac
                            );
                          }
                          return "Unknown";
                        })()}
                      </div>
                    </div>
                  )}

                  {device.isAP && (
                    <>
                      <div className="bg-white/[0.02] border border-white/5 rounded-2xl p-4 flex items-center justify-between">
                        <div className="flex items-center gap-2 text-gray-500 uppercase tracking-widest">
                          <Shield size={10} />
                          <span className="text-[8px] font-black">
                            Security
                          </span>
                        </div>
                        <div className="text-xs font-bold text-gray-300">
                          {device.security || "Open"}
                        </div>
                      </div>
                      <div className="bg-white/[0.02] border border-white/5 rounded-2xl p-4 flex items-center justify-between">
                        <div className="flex items-center gap-2 text-gray-500 uppercase tracking-widest">
                          <Database size={10} />
                          <span className="text-[8px] font-black">Channel</span>
                        </div>
                        <div className="text-xs font-bold text-gray-300">
                          CH {device.channel}
                        </div>
                      </div>
                      {device.wifiGen && (
                        <div className="bg-white/[0.02] border border-white/5 rounded-2xl p-4 flex items-center justify-between">
                          <div className="flex items-center gap-2 text-gray-500 uppercase tracking-widest">
                            <Cpu size={10} />
                            <span className="text-[8px] font-black">
                              Standard
                            </span>
                          </div>
                          <div className="text-xs font-bold text-accent">
                            {device.wifiGen}
                          </div>
                        </div>
                      )}
                      {device.clients !== undefined && device.clients >= 0 && (
                        <div className="bg-white/[0.02] border border-white/5 rounded-2xl p-4 flex items-center justify-between">
                          <div className="flex items-center gap-2 text-gray-500 uppercase tracking-widest">
                            <Monitor size={10} />
                            <span className="text-[8px] font-black">
                              Active Clients
                            </span>
                          </div>
                          <div className="text-xs font-bold text-green">
                            {device.clients}
                          </div>
                        </div>
                      )}
                    </>
                  )}

                  {/* Channel field for Stations if not AP */}
                  {!device.isAP && (
                    <div className="bg-white/[0.02] border border-white/5 rounded-2xl p-4 flex items-center justify-between">
                      <div className="flex items-center gap-2 text-gray-500 uppercase tracking-widest">
                        <Database size={10} />
                        <span className="text-[8px] font-black">Channel</span>
                      </div>
                      <div className="text-xs font-bold text-gray-300">
                        CH {device.channel || "?"}
                      </div>
                    </div>
                  )}

                  <div className="bg-white/[0.02] border border-white/5 rounded-2xl p-4 flex items-center justify-between">
                    <div className="flex items-center gap-2 text-gray-500 uppercase tracking-widest">
                      <Activity size={10} />
                      <span className="text-[8px] font-black">Packets</span>
                    </div>
                    <div className="text-xs font-bold text-gray-300">
                      {device.packetCount || 1}
                    </div>
                  </div>

                  <div className="bg-white/[0.02] border border-white/5 rounded-2xl p-4 flex items-center justify-between">
                    <div className="flex items-center gap-2 text-gray-500 uppercase tracking-widest">
                      <X size={10} />
                      <span className="text-[8px] font-black">Last Seen</span>
                    </div>
                    <div className="text-xs font-bold text-accent">
                      {lastSeenStr}
                    </div>
                  </div>
                </div>
              </div>
            ) : activeTab === "connections" ? (
              <div className="space-y-4 animate-in fade-in slide-in-from-bottom-2 duration-300">
                <div className="p-4 rounded-2xl bg-white/[0.02] border border-white/5">
                  <div className="flex items-center gap-2 mb-2">
                    <Monitor
                      size={14}
                      className={device.isAP ? "text-green" : "text-accent"}
                    />
                    <span className="text-[9px] font-black text-gray-400 uppercase tracking-widest">
                      Potential{" "}
                      {device.isAP ? "Connected Stations" : "Access Points"}
                    </span>
                  </div>
                  <p className="text-[8px] font-bold text-gray-600 leading-relaxed mb-4">
                    Approximation based on devices actively transmitting on
                    Channel {device.channel || "?"}. True association requires
                    capturing handshake or targeted data frames.
                  </p>

                  {potentialConnections.length === 0 ? (
                    <div className="text-center py-6 text-[10px] font-black text-gray-600 uppercase italic border border-white/5 rounded-xl bg-black/20">
                      No matching devices detected on Channel{" "}
                      {device.channel || "?"}
                    </div>
                  ) : (
                    <div className="space-y-2 max-h-[300px] overflow-y-auto pr-1 scrollbar-thin scrollbar-thumb-white/10">
                      {potentialConnections.map((conn) => {
                        const vendor = getMacVendor(conn.mac);
                        const cLastSeenSec = conn.lastSeen
                          ? uptime - conn.lastSeen
                          : 0;
                        const cLastSeenStr =
                          cLastSeenSec < 1
                            ? "now"
                            : `${Math.floor(cLastSeenSec)}s ago`;

                        return (
                          <div
                            key={conn.mac}
                            className="flex flex-col bg-black/40 border border-white/5 rounded-xl p-3 hover:bg-white/[0.05] transition-colors"
                          >
                            <div className="flex justify-between items-start mb-1">
                              <span className="font-mono text-xs font-black text-white">
                                {conn.mac}
                              </span>
                              <span
                                className={`text-[9px] font-black ${getRssiColor(conn.rssi)}`}
                              >
                                {conn.rssi} dBm
                              </span>
                            </div>
                            <div className="flex justify-between items-center text-[8px] font-bold text-gray-500 uppercase">
                              <span className="truncate max-w-[150px]">
                                {vendor || "Unknown Vendor"}
                              </span>
                              <span className="text-accent/60 flex gap-2">
                                <span>{conn.packetCount} PKTS</span>
                                <span>{cLastSeenStr}</span>
                              </span>
                            </div>
                            {conn.ssid && (
                              <div
                                className={`text-[8px] font-bold mt-1 truncate ${conn.isAP ? "text-green" : "text-gray-400"}`}
                              >
                                {conn.isAP
                                  ? `SSID: ${conn.ssid}`
                                  : `Probing: ${conn.ssid}`}
                              </div>
                            )}
                          </div>
                        );
                      })}
                    </div>
                  )}
                </div>
              </div>
            ) : (
              <div className="space-y-6 animate-in fade-in slide-in-from-bottom-2 duration-300">
                <div className="p-4 rounded-2xl bg-red/5 border border-red/10 flex items-center gap-3">
                  <AlertTriangle size={20} className="text-red animate-pulse" />
                  <p className="text-[9px] font-bold text-red/80 uppercase tracking-tighter">
                    Stress testing will target {device.mac} on CH{" "}
                    {device.channel || "?"}.
                  </p>
                </div>

                <div className="space-y-4">
                  <div className="space-y-2">
                    <label className="text-[9px] font-black text-gray-500 uppercase tracking-widest block">
                      PACKET RATE (PKTS/S)
                    </label>
                    <input
                      type="number"
                      min="1"
                      max="100"
                      value={rate}
                      onChange={(e) => setRate(parseInt(e.target.value))}
                      disabled={isStressActive}
                      className="w-full bg-black/40 border border-white/10 rounded-xl px-4 py-3 text-xs font-black text-white outline-none focus:border-red/40 disabled:opacity-50"
                    />
                  </div>

                  <div className="space-y-2">
                    <label className="text-[9px] font-black text-gray-500 uppercase tracking-widest block">
                      TEST TYPE
                    </label>
                    <div className="bg-black/40 border border-white/10 rounded-xl px-4 py-3 text-xs font-bold text-accent uppercase">
                      DEAUTH STRESS
                    </div>
                    <p className="text-[7px] text-gray-600 font-bold uppercase tracking-widest leading-relaxed">
                      Defaulting to Deauth for targeted stress. For Beacon/Probe
                      floods, use the STRESS tab.
                    </p>
                  </div>

                  <button
                    onClick={handleToggleStress}
                    disabled={loading}
                    className={`w-full py-4 rounded-2xl font-black tracking-[0.3em] text-[10px] transition-all flex items-center justify-center gap-3 active:scale-[0.98] ${
                      isStressActive
                        ? "bg-red text-bg shadow-[0_8px_24px_rgba(252,129,129,0.3)]"
                        : "bg-white/5 border border-red/30 text-red hover:bg-red/10"
                    }`}
                  >
                    {loading ? (
                      <div className="w-4 h-4 border-2 border-current border-t-transparent rounded-full animate-spin" />
                    ) : isStressActive ? (
                      "STOP ATTACK"
                    ) : (
                      "START STRESS TEST"
                    )}
                  </button>
                </div>

                {isStressActive && (
                  <div className="text-center">
                    <span className="text-[8px] font-black text-red animate-pulse uppercase tracking-[0.2em]">
                      TRANSMITTING MALICIOUS FRAMES...
                    </span>
                  </div>
                )}
              </div>
            )}
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
  },
);

import { DataTable, Column } from "../DataTable";

const renderAddr = (addr: string, mac?: string) => {
  const isMac = addr.includes(":");
  const resolvedMac = !isMac && mac ? mac : isMac ? addr : "";
  const vendor = resolvedMac ? getMacVendor(resolvedMac) : "";

  return (
    <div className="flex flex-col truncate text-left leading-tight">
      <span className="font-bold text-gray-300">{addr}</span>
      {resolvedMac && (
        <span className="text-[7.5px] text-gray-500 truncate mt-0.5">
          {resolvedMac !== addr && `${resolvedMac}`}
          {vendor && (
            <span className="text-accent/70 ml-1 font-semibold">
              ({vendor})
            </span>
          )}
        </span>
      )}
    </div>
  );
};

const DeviceCard = ({
  dev,
  status,
  allDevices,
  onClick,
}: {
  dev: DeviceInfo;
  status: any;
  allDevices?: DeviceInfo[];
  onClick: () => void;
}) => {
  const lastSeenSec = dev.lastSeen ? (status?.uptime || 0) - dev.lastSeen : 0;
  const lastSeenStr = lastSeenSec < 1 ? "now" : `${Math.floor(lastSeenSec)}s`;

  let likelyApStr = "";
  if (!dev.isAP && allDevices) {
    const potentialAPs = allDevices.filter(
      (d) => d.isAP && d.channel === dev.channel,
    );
    if (potentialAPs.length > 0) {
      likelyApStr = potentialAPs[0].ssid || "Hidden AP";
    }
  }

  return (
    <div
      onClick={onClick}
      className="bg-white/[0.03] border border-white/10 rounded-2xl p-4 space-y-3 active:scale-[0.98] transition-all"
    >
      <div className="flex justify-between items-start">
        <div className="flex items-center gap-2">
          <div
            className={`px-1.5 py-0.5 rounded text-[8px] font-black tracking-tighter ${dev.isAP ? "bg-green/20 text-green" : "bg-accent/20 text-accent"}`}
          >
            {dev.isAP ? "AP" : "STATION"}
          </div>
          <span className="text-[10px] font-black text-white font-mono">
            {dev.mac}
          </span>
        </div>
        <div className={`text-[10px] font-black ${getRssiColor(dev.rssi)}`}>
          {dev.rssi} dBm
        </div>
      </div>
      <div className="text-xs font-bold text-gray-400 truncate">
        {dev.isAP
          ? dev.ssid || "<Hidden SSID>"
          : dev.ssid
            ? `Probe: ${dev.ssid}`
            : likelyApStr
              ? `AP: ${likelyApStr}`
              : "Scanning..."}
      </div>
      <div className="flex flex-wrap gap-2 text-[8px] font-black text-gray-600 uppercase">
        <span className="bg-black/40 px-2 py-0.5 rounded border border-white/5">
          CH {dev.channel || "?"}
        </span>
        {dev.wifiGen && (
          <span className="bg-accent/10 text-accent px-2 py-0.5 rounded border border-accent/20">
            {dev.wifiGen}
          </span>
        )}
        <span className="bg-black/40 px-2 py-0.5 rounded border border-white/5">
          {dev.packetCount || 1} PKTS
        </span>
        <span className="ml-auto text-accent/60">{lastSeenStr}</span>
      </div>
    </div>
  );
};

const PacketCard = ({
  log,
  seq,
  onInspect,
}: {
  log: PacketLog;
  seq: number;
  onInspect: (log: PacketLog, seq: number) => void;
}) => {
  return (
    <div
      onClick={() => onInspect(log, seq)}
      className="bg-white/[0.02] border border-white/5 rounded-2xl p-4 space-y-2 active:bg-white/[0.05] transition-all"
    >
      <div className="flex justify-between items-center">
        <div className="flex items-center gap-2">
          <span
            className={`text-[9px] font-black px-1.5 py-0.5 rounded ${getProtoColor(log.proto)} bg-black/40 border border-white/5`}
          >
            {log.proto}
          </span>
          {log.subtype && (
            <span className="text-[8px] font-bold text-gray-500 uppercase">
              {log.subtype}
            </span>
          )}
        </div>
        <span className="text-[8px] font-medium text-gray-600 font-mono">
          {formatUptime(log.time)}
        </span>
      </div>
      <div className="grid grid-cols-2 gap-x-4 gap-y-1">
        <div>
          <span className="text-[7px] font-black text-gray-600 uppercase block">
            Source
          </span>
          <span className="text-[9px] font-bold text-gray-400 truncate block">
            {log.src}
          </span>
        </div>
        <div>
          <span className="text-[7px] font-black text-gray-600 uppercase block">
            Destination
          </span>
          <span className="text-[9px] font-bold text-gray-400 truncate block">
            {log.dst}
          </span>
        </div>
      </div>
      <div className="text-[9px] text-gray-500 truncate italic">{log.info}</div>
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

const DeviceTable = React.memo(
  ({
    devices,
    allDevices,
    status,
    onSelect,
    searchTerm,
    onSearch,
    filterType,
    onFilter,
  }: {
    devices: DeviceInfo[];
    allDevices: DeviceInfo[];
    status: any;
    onSelect: (d: DeviceInfo) => void;
    searchTerm: string;
    onSearch: (v: string) => void;
    filterType: string;
    onFilter: (v: string) => void;
  }) => {
    const columns: Column<DeviceInfo>[] = [
      {
        key: "isAP",
        header: "T",
        width: "40px",
        align: "center",
        sortable: true,
        sortValue: (r) => (r.isAP ? 1 : 0),
        render: (dev) => (
          <span
            className={`font-black tracking-tighter ${dev.isAP ? "text-green" : "text-accent"}`}
          >
            {dev.isAP ? "AP" : "ST"}
          </span>
        ),
      },
      {
        key: "mac",
        header: "MAC / VENDOR",
        width: "160px",
        sortable: true,
        filterable: true,
        filterPlaceholder: "Filter MAC/Ven",
        sortValue: (r) => getMacVendor(r.mac) || r.mac,
        filterValue: (r) => `${r.mac} ${getMacVendor(r.mac) || ""}`,
        render: (dev) => (
          <div className="truncate">
            <div className="group-hover:text-white transition-colors">
              {dev.mac}
            </div>
            {getMacVendor(dev.mac) && (
              <span className="text-[7.5px] font-bold text-gray-500 bg-white/[0.02] px-1 py-0.2 rounded border border-white/5 uppercase tracking-tighter mt-0.5 inline-block truncate max-w-full">
                {getMacVendor(dev.mac)}
              </span>
            )}
          </div>
        ),
      },
      {
        key: "ssid",
        header: "SSID / INFO",
        sortable: true,
        filterable: true,
        filterPlaceholder: "Filter SSID",
        sortValue: (r) => r.ssid || "",
        filterValue: (r) => r.ssid || "",
        render: (dev) => (
          <span className="text-gray-500 truncate group-hover:text-gray-400">
            {dev.isAP
              ? dev.ssid || "<Hidden SSID>"
              : dev.ssid
                ? `Probe: ${dev.ssid}`
                : "Scanning..."}
          </span>
        ),
      },
      {
        key: "channel",
        header: "CH",
        width: "50px",
        align: "center",
        sortable: true,
        filterable: true,
        filterPlaceholder: "CH",
        sortValue: (r) => r.channel || 0,
        filterValue: (r) => String(r.channel || ""),
        render: (dev) => (
          <span className="text-gray-400 font-bold">{dev.channel || "?"}</span>
        ),
      },
      {
        key: "security",
        header: "SEC",
        width: "70px",
        align: "center",
        filterable: true,
        filterPlaceholder: "SEC",
        filterValue: (r) => r.security || (r.isAP ? "Open" : "-"),
        render: (dev) => (
          <span className="text-[8px] text-gray-500 font-medium">
            {dev.security
              ? dev.security.split(" ")[0]
              : dev.isAP
                ? "Open"
                : "-"}
          </span>
        ),
      },
      {
        key: "wifiGen",
        header: "GN",
        width: "50px",
        align: "center",
        filterable: true,
        filterPlaceholder: "GN",
        filterValue: (r) => r.wifiGen || "",
        render: (dev) => (
          <span className="text-accent/80 font-black">
            {dev.wifiGen ? dev.wifiGen.replace("WiFi ", "") : "-"}
          </span>
        ),
      },
      {
        key: "stations",
        header: "STAs",
        width: "50px",
        align: "center",
        sortable: true,
        sortValue: (r) => {
           if (!r.isAP) return -1;
           if (r.clients !== undefined && r.clients >= 0) return r.clients;
           // Fallback approximation
           return allDevices.filter(d => !d.isAP && (d.bssid === r.mac || (d.channel === r.channel && !d.bssid))).length;
        },
        render: (dev) => {
           if (!dev.isAP) return <span className="text-gray-600">-</span>;
           
           let count = dev.clients;
           // If backend didn't provide a count, approximate it based on our store data
           if (count === undefined || count < 0) {
              count = allDevices.filter(d => !d.isAP && (d.bssid === dev.mac || (d.channel === dev.channel && !d.bssid))).length;
           }

           return (
             <span className={`font-black ${count && count > 0 ? 'text-green' : 'text-gray-500'}`}>
               {count}
             </span>
           );
        },
      },
      {
        key: "packetCount",
        header: "PKTS",
        width: "60px",
        align: "right",
        sortable: true,
        sortValue: (r) => r.packetCount || 0,
        render: (dev) => (
          <span className="font-bold text-gray-600 group-hover:text-gray-400">
            {dev.packetCount || 1}
          </span>
        ),
      },
      {
        key: "rssi",
        header: "RSSI",
        width: "60px",
        align: "right",
        sortable: true,
        sortValue: (r) => r.rssi,
        render: (dev) => (
          <span className={`font-black ${getRssiColor(dev.rssi)}`}>
            {dev.rssi}
          </span>
        ),
      },
      {
        key: "lastSeen",
        header: "SEEN",
        width: "70px",
        align: "right",
        sortable: true,
        sortValue: (r) => r.lastSeen || 0,
        render: (dev) => {
          const sec = dev.lastSeen
            ? Math.floor((status?.uptime || 0) - dev.lastSeen)
            : 0;
          return (
            <span className="text-accent/60 font-medium">
              {sec < 1 ? "now" : `${sec}s`}
            </span>
          );
        },
      },
      {
        key: "actions",
        header: "ACTIONS",
        width: "90px",
        align: "center",
        render: (dev) => (
          <div className="flex justify-center gap-1 opacity-40 group-hover:opacity-100 transition-opacity">
            <button
              title="Quick Ping"
              onClick={(e) => {
                e.stopPropagation();
                apiPing(dev.mac);
              }}
              className="p-1.5 rounded-lg bg-accent/10 border border-accent/20 text-accent hover:bg-accent/30"
            >
              <Target size={12} />
            </button>
            <button
              title="Deauth Attack"
              onClick={(e) => {
                e.stopPropagation();
                apiStressControl(
                  true,
                  "deauth",
                  dev.mac,
                  "FF:FF:FF:FF:FF:FF",
                  dev.channel || 1,
                  10,
                );
              }}
              className="p-1.5 rounded-lg bg-red/10 border border-red/20 text-red hover:bg-red/30"
            >
              <Skull size={12} />
            </button>
          </div>
        ),
      },
    ];

    const globalFilter = React.useCallback(
      (dev: DeviceInfo) => {
        if (filterType !== "ALL") {
          if (filterType === "AP" && !dev.isAP) return false;
          if (filterType === "STATION" && dev.isAP) return false;
        }
        if (searchTerm) {
          const hay =
            `${dev.mac} ${dev.ssid || ""} ${getMacVendor(dev.mac) || ""}`.toLowerCase();
          if (!hay.includes(searchTerm.toLowerCase())) return false;
        }
        return true;
      },
      [filterType, searchTerm],
    );

    const headerLeft = (
      <span className="text-[10px] font-black text-accent tracking-widest uppercase">
        DEVICES ({devices.length})
      </span>
    );

    const headerRight = (
      <>
        <div className="flex items-center gap-1.5 px-2 py-0.5 rounded bg-black/40 border border-white/5 shrink-0">
          <Search size={10} className="text-gray-600" />
          <input
            type="text"
            placeholder="Global Search..."
            value={searchTerm}
            onChange={(e) => onSearch(e.target.value)}
            className="bg-transparent border-none outline-none text-[9px] font-bold text-accent w-24 placeholder:text-gray-700"
          />
        </div>
        <select
          className="bg-black/80 text-[9px] font-black border border-white/10 rounded-lg px-2 py-1 outline-none text-accent focus:border-accent/40 shrink-0"
          value={filterType}
          onChange={(e) => onFilter(e.target.value)}
        >
          <option value="ALL">ALL TYPE</option>
          <option value="AP">ACCESS POINTS</option>
          <option value="STATION">STATIONS</option>
        </select>
      </>
    );

    return (
      <DataTable
        data={devices}
        columns={columns}
        defaultSortKey="rssi"
        defaultSortDir="desc"
        defaultPageSize={10}
        pageSizeOptions={[10, 25, 50, 100]}
        globalFilter={globalFilter}
        onRowClick={onSelect}
        headerLeft={headerLeft}
        headerRight={headerRight}
        minWidth="750px"
        containerHeight="600px"
        renderMobileCard={(dev) => (
          <DeviceCard
            key={dev.mac}
            dev={dev}
            status={status}
            allDevices={allDevices}
            onClick={() => onSelect(dev)}
          />
        )}
      />
    );
  },
);

const PacketStream = React.memo(
  ({
    logs,
    searchTerm, // Kept for backward compatibility/global search if needed
    filterProto,
    availableProtos,
    onInspect,
    onClear,
    onSearch,
    onFilter,
  }: any) => {
    const columns: Column<PacketLog>[] = [
      {
        key: "time",
        header: "TIME",
        width: "85px",
        sortable: true,
        sortValue: (r) => r.time,
        render: (log) => (
          <span className="text-gray-500 font-medium whitespace-nowrap">
            {formatUptime(log.time)}
          </span>
        ),
      },
      {
        key: "proto",
        header: "PROTO",
        width: "60px",
        sortable: true,
        filterable: true,
        filterPlaceholder: "PRT",
        sortValue: (r) => r.proto,
        filterValue: (r) => r.proto || "",
        render: (log) => (
          <span
            className={`font-black tracking-tighter ${getProtoColor(log.proto)}`}
          >
            {log.proto}
          </span>
        ),
      },
      {
        key: "subtype",
        header: "METH",
        width: "60px",
        sortable: true,
        filterable: true,
        filterPlaceholder: "MTH",
        sortValue: (r) => r.subtype || "",
        filterValue: (r) => r.subtype || "",
        render: (log) => (
          <span className="text-gray-400 font-bold tracking-tighter truncate">
            {log.subtype || "-"}
          </span>
        ),
      },
      {
        key: "src",
        header: "SOURCE",
        width: "140px",
        sortable: true,
        filterable: true,
        filterPlaceholder: "Filter SRC",
        sortValue: (r) => r.src,
        filterValue: (r) => `${r.src} ${r.srcMac || ""}`,
        render: (log) => renderAddr(log.src, log.srcMac),
      },
      {
        key: "srcPort",
        header: "S-PORT",
        width: "60px",
        align: "center",
        sortable: true,
        filterable: true,
        filterPlaceholder: "PT",
        sortValue: (r) => r.srcPort || 0,
        filterValue: (r) => String(r.srcPort || ""),
        render: (log) => (
          <span className="text-gray-400 font-bold">
            {log.srcPort && log.srcPort > 0 ? log.srcPort : "-"}
          </span>
        ),
      },
      {
        key: "dst",
        header: "DESTINATION",
        width: "140px",
        sortable: true,
        filterable: true,
        filterPlaceholder: "Filter DST",
        sortValue: (r) => r.dst,
        filterValue: (r) => `${r.dst} ${r.dstMac || ""}`,
        render: (log) => renderAddr(log.dst, log.dstMac),
      },
      {
        key: "dstPort",
        header: "D-PORT",
        width: "60px",
        align: "center",
        sortable: true,
        filterable: true,
        filterPlaceholder: "PT",
        sortValue: (r) => r.dstPort || 0,
        filterValue: (r) => String(r.dstPort || ""),
        render: (log) => (
          <span className="text-gray-400 font-bold">
            {log.dstPort && log.dstPort > 0 ? log.dstPort : "-"}
          </span>
        ),
      },
      {
        key: "channel",
        header: "CH",
        width: "40px",
        align: "center",
        sortable: true,
        filterable: true,
        filterPlaceholder: "CH",
        sortValue: (r) => r.channel || 0,
        filterValue: (r) => String(r.channel || ""),
        render: (log) => (
          <span className="text-gray-400 font-bold">{log.channel ?? "-"}</span>
        ),
      },
      {
        key: "len",
        header: "LEN",
        width: "55px",
        align: "right",
        sortable: true,
        sortValue: (r) => r.len,
        render: (log) => (
          <span className="text-gray-400 font-medium whitespace-nowrap">
            {log.len} B
          </span>
        ),
      },
      {
        key: "ttl",
        header: "TTL",
        width: "40px",
        align: "center",
        sortable: true,
        sortValue: (r) => r.ttl || 0,
        render: (log) => (
          <span className="text-gray-400 font-bold">
            {log.ttl && log.ttl > 0 ? log.ttl : "-"}
          </span>
        ),
      },
      {
        key: "rssi",
        header: "RSSI",
        width: "60px",
        align: "right",
        sortable: true,
        sortValue: (r) => r.rssi,
        render: (log) => (
          <span
            className={`font-black whitespace-nowrap ${getRssiColor(log.rssi)}`}
          >
            {log.rssi} dBm
          </span>
        ),
      },
      {
        key: "info",
        header: "INFO / PAYLOAD",
        filterable: true,
        filterPlaceholder: "Search Info...",
        filterValue: (r) => r.info || "",
        render: (log) => (
          <span
            className="text-gray-500 truncate group-hover:text-gray-300"
            title={log.info}
          >
            {log.info}
          </span>
        ),
      },
    ];

    const globalFilter = React.useCallback(
      (log: PacketLog) => {
        if (filterProto !== "ALL") {
          if (filterProto === "GROUP:SECURITY") {
            if (
              log.proto !== "DEAUTH" &&
              log.proto !== "DISASSOC" &&
              log.proto !== "WPA HS"
            )
              return false;
          } else if (filterProto === "GROUP:DNS") {
            if (log.proto !== "DNS" && log.proto !== "mDNS") return false;
          } else if (log.proto !== filterProto) return false;
        }

        if (searchTerm) {
          const srcMacVal =
            log.srcMac || (log.src.includes(":") ? log.src : "");
          const dstMacVal =
            log.dstMac || (log.dst.includes(":") ? log.dst : "");
          const hay =
            `${log.src} ${log.dst} ${log.info} ${log.proto} ${srcMacVal} ${dstMacVal} ${log.subtype || ""}`.toLowerCase();
          if (!hay.includes(searchTerm.toLowerCase())) return false;
        }
        return true;
      },
      [filterProto, searchTerm],
    );

    const headerLeft = (
      <span className="text-[10px] font-black text-accent tracking-widest uppercase hidden md:block">
        STREAM
      </span>
    );

    const headerRight = (
      <>
        <div className="flex items-center gap-1.5 px-2 py-0.5 rounded bg-black/40 border border-white/5 shrink-0 sm:hidden lg:flex">
          <Search size={10} className="text-gray-600" />
          <input
            type="text"
            placeholder="Global Filter..."
            value={searchTerm}
            onChange={(e) => onSearch(e.target.value)}
            className="bg-transparent border-none outline-none text-[9px] font-bold text-accent w-20 placeholder:text-gray-700"
          />
        </div>
        <select
          className="bg-black/80 text-[9px] font-black border border-white/10 rounded-lg px-2 py-1 outline-none text-accent focus:border-accent/40 min-w-[100px] shrink-0"
          value={filterProto}
          onChange={(e) => onFilter(e.target.value)}
        >
          <option value="ALL">ALL PROTOS</option>
          {availableProtos.hasSecurity && (
            <option value="GROUP:SECURITY">🛡 SECURITY</option>
          )}
          {availableProtos.hasDns && <option value="GROUP:DNS">🌐 DNS</option>}
          {availableProtos.list.map((p: any) => (
            <option key={p} value={p}>
              {p}
            </option>
          ))}
        </select>
        <button
          onClick={onClear}
          className="p-1.5 rounded-lg bg-red/5 border border-red/20 text-red hover:bg-red/10 shrink-0"
          title="Clear Stream"
        >
          <Trash2 size={12} />
        </button>
      </>
    );

    return (
      <div className="mx-4 mb-4">
        <DataTable
          data={logs}
          columns={columns}
          defaultSortKey="time"
          defaultSortDir="desc"
          defaultPageSize={50}
          pageSizeOptions={[50, 100, 250, 500]}
          globalFilter={globalFilter}
          onRowClick={(log) => onInspect(log, 0)} // Note: seq is not strictly needed for UI presentation if time is used
          headerLeft={headerLeft}
          headerRight={headerRight}
          minWidth="1100px"
          containerHeight="700px"
          footer={(filteredCount, totalCount) => (
            <>
              <span>
                Buffer: {filteredCount} / {totalCount} (Cap: 10,000)
              </span>
              <span className="text-accent/40 animate-pulse">SSE ACTIVE</span>
            </>
          )}
          renderMobileCard={(log, i) => (
            <PacketCard
              key={`${log.time}-${i}`}
              log={log}
              seq={logs.length - i}
              onInspect={onInspect}
            />
          )}
        />
      </div>
    );
  },
);

export const SnifferTab: React.FC<SnifferTabProps> = ({ onInspect }) => {
  const sniffer = useStore((state) => state.sniffer);
  const logs = useStore((state) => state.logs);
  const status = useStore((state) => state.status);
  const clearLogs = useStore((state) => state.clearLogs);

  const [filterProto, setFilterProto] = useState("ALL");
  const [searchTerm, setSearchTerm] = useState("");
  const [deviceSearch, setDeviceSearch] = useState("");
  const [deviceFilter, setDeviceFilter] = useState("ALL");
  const [selectedDevice, setSelectedDevice] = useState<DeviceInfo | null>(null);
  const [showFilters, setShowFilters] = useState(false);
  const [activeFilters, setActiveFilters] = useState<SnifferFilters>({
    whitelist: [],
    blacklist: [],
  });
  const [snifferLoading, setSnifferLoading] = useState(false);

  const isFilteringActive =
    activeFilters.whitelist.length > 0 || activeFilters.blacklist.length > 0;

  const refreshFilters = React.useCallback(() => {
    apiGetSnifferFilters().then((data) => {
      if (data) setActiveFilters(data);
    });
  }, []);

  useEffect(() => {
    refreshFilters();
  }, [showFilters, refreshFilters]);

  const setSniffer = useStore((state) => state.setSniffer);

  const toggleSniffer = async () => {
    const active = !sniffer?.active;
    // Optimistic update: only toggle active state, preserve existing devices and stats
    setSniffer({ active, concurrent: active } as any);
    try {
      await apiSnifferControl(active, 0, true);
    } catch {
      // Revert if failed (SSE will eventually fix it too)
      setSniffer({ active: !active, concurrent: !active } as any);
    }
  };

  const availableProtos = React.useMemo(() => {
    const protos = new Set<string>();
    logs.forEach((l) => protos.add(l.proto));
    const list = Array.from(protos).sort();
    const hasSecurity = logs.some(
      (l) =>
        l.proto === "DEAUTH" || l.proto === "DISASSOC" || l.proto === "WPA HS",
    );
    const hasDns = logs.some((l) => l.proto === "DNS" || l.proto === "mDNS");
    return { list, hasSecurity, hasDns };
  }, [logs]);

  if (!sniffer) {
    return (
      <div className="flex flex-col items-center justify-center p-12 space-y-4 animate-pulse">
        <Wifi size={48} className="text-gray-700" />
        <p className="text-[10px] font-black text-gray-500 uppercase tracking-[0.2em]">
          Synchronizing Monitor State...
        </p>
      </div>
    );
  }

  return (
    <div className="space-y-4 animate-fade-up">
      {selectedDevice && (
        <DeviceModal
          device={selectedDevice}
          allDevices={sniffer.devices || []}
          onClose={() => setSelectedDevice(null)}
          uptime={status?.uptime || 0}
          onUpdateFilters={refreshFilters}
        />
      )}
      {showFilters && <FilterModal onClose={() => setShowFilters(false)} />}

      <section className="section-card">
        <div className="flex items-center gap-2 mb-4">
          <div className="w-1.5 h-3.5 bg-accent rounded-full shadow-[0_0_10px_rgba(99,179,237,0.8)]" />
          <h2 className="text-[11px] font-extrabold tracking-widest text-accent uppercase italic flex items-center gap-2">
            <Monitor size={12} /> WIFI MONITOR
          </h2>
          {sniffer?.active && (
            <span className="ml-auto flex items-center gap-1.5 text-[9px] font-black text-green animate-pulse">
              ● LIVE
            </span>
          )}
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
                ? "bg-red text-bg shadow-[0_8px_24px_rgba(252,129,129,0.3)]"
                : "bg-accent text-bg shadow-[0_8px_24px_rgba(99,179,237,0.3)] hover:brightness-110"
            }`}
          >
            {sniffer?.active ? "⏹ STOP SNIFFER" : "⚡ START SNIFFER"}
          </button>

          <button
            onClick={() => setShowFilters(true)}
            className={`px-6 rounded-2xl border transition-all flex flex-col items-center justify-center relative ${
              isFilteringActive
                ? "bg-green/10 border-green/30 text-green shadow-[0_0_15px_rgba(72,187,120,0.2)]"
                : "bg-white/5 border-white/10 text-accent hover:bg-white/10"
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
              {isFilteringActive ? "ACTIVE" : "FILTERS"}
            </span>
          </button>
        </div>
      </section>

      <div className="animate-in fade-in slide-in-from-bottom-4 duration-700 space-y-4">
        {isFilteringActive && (
          <div className="mx-4 p-2.5 rounded-xl bg-green/5 border border-green/20 flex items-center justify-between">
            <div className="flex items-center gap-2">
              <Shield size={12} className="text-green" />
              <span className="text-[9px] font-black text-green/70 uppercase tracking-widest">
                MAC FILTERING ACTIVE: {activeFilters.whitelist.length} Whitelist
                / {activeFilters.blacklist.length} Blacklist
              </span>
            </div>
            <button
              onClick={() => setShowFilters(true)}
              className="text-[8px] font-black text-accent uppercase hover:underline"
            >
              Edit Filters
            </button>
          </div>
        )}

        <div className="grid grid-cols-1 gap-4 mx-4">
          <DeviceTable
            devices={sniffer.devices || []}
            allDevices={sniffer.devices || []}
            status={status}
            onSelect={setSelectedDevice}
            searchTerm={deviceSearch}
            onSearch={setDeviceSearch}
            filterType={deviceFilter}
            onFilter={setDeviceFilter}
          />

          </div>

        <PacketStream
          logs={logs}
          searchTerm={searchTerm}
          filterProto={filterProto}
          availableProtos={availableProtos}
          onInspect={onInspect}
          onClear={clearLogs}
          onSearch={setSearchTerm}
          onFilter={setFilterProto}
        />

        <div className="grid grid-cols-1 gap-4 mx-4 mb-4">
<div className="bg-[#050709] border border-white/10 rounded-2xl overflow-hidden shadow-2xl flex flex-col min-h-[400px]">
            <div className="bg-white/[0.03] px-4 py-3 border-b border-white/10 flex items-center justify-between">
              <span className="text-[10px] font-black text-accent tracking-widest uppercase">
                SPECTRUM & PROTOCOL ANALYSIS
              </span>
              <div className="flex items-center gap-1.5 text-[8px] font-black text-gray-500 uppercase">
                <Activity size={10} className="text-accent" />
                Live Insight
              </div>
            </div>
            <div className="flex-1 overflow-y-auto p-5 space-y-6 scrollbar-thin scrollbar-thumb-white/10">
              {/* Total Packets Overview */}
              <div className="flex items-center justify-between p-3 rounded-xl bg-accent/5 border border-accent/20">
                <div className="flex items-center gap-2 text-accent">
                  <Activity size={14} className="animate-pulse" />
                  <span className="text-[10px] font-black uppercase tracking-widest">
                    Total Packets Captured
                  </span>
                </div>
                <span className="text-sm font-black text-white">
                  {sniffer.packets.toLocaleString()}
                </span>
              </div>

              <div className="space-y-2">
                <span className="text-[9px] font-black text-gray-400 uppercase tracking-widest block">
                  Channel Congestion (1-13)
                </span>
                <div className="flex gap-1">
                  {Array.from({ length: 13 }, (_, i) => {
                    const ch = i + 1;
                    const apCount = (sniffer.devices || []).filter(
                      (d) => d.isAP && d.channel === ch,
                    ).length;

                    let bg = "bg-white/5 text-gray-700";
                    if (apCount > 3)
                      bg = "bg-red/20 text-red border border-red/30";
                    else if (apCount > 0)
                      bg = "bg-accent/20 text-accent border border-accent/30";

                    return (
                      <div
                        key={ch}
                        title={`CH ${ch}: ${apCount} APs`}
                        className={`flex-1 flex flex-col items-center justify-center py-1.5 rounded-lg transition-all ${bg}`}
                      >
                        <span className="text-[7px] font-black opacity-60">
                          CH{ch}
                        </span>
                        <span className="text-[10px] font-black leading-none mt-0.5">
                          {apCount}
                        </span>
                      </div>
                    );
                  })}
                </div>
              </div>

              <div className="space-y-4 border-t border-white/5 pt-5">
                <div className="flex justify-between items-end">
                  <span className="text-[9px] font-black text-gray-400 uppercase tracking-widest">
                    Protocol Distribution
                  </span>
                  <span className="text-[8px] font-bold text-gray-600 uppercase">
                    Deep Packet Stats
                  </span>
                </div>
                <div className="space-y-3">
                  {[
                    {
                      label: "Management",
                      value: (sniffer.beacons || 0) + (sniffer.probes || 0),
                      total: sniffer.packets,
                      color: "bg-accent",
                      icon: <Shield size={10} />,
                    },
                    {
                      label: "Data Traffic",
                      value: (sniffer.tcp || 0) + (sniffer.udp || 0),
                      total: sniffer.packets,
                      color: "bg-green",
                      icon: <Activity size={10} />,
                    },
                    {
                      label: "Security/Threats",
                      value: sniffer.deauths || 0,
                      total: sniffer.packets,
                      color: "bg-red",
                      icon: <Skull size={10} />,
                    },
                    {
                      label: "Infrastructure",
                      value:
                        (sniffer.dns || 0) +
                        (sniffer.arp || 0) +
                        (sniffer.dhcp || 0),
                      total: sniffer.packets,
                      color: "bg-[#f6e05e]",
                      icon: <Database size={10} />,
                    },
                  ].map((group, idx) => {
                    const percent = group.total
                      ? Math.round((group.value / group.total) * 100)
                      : 0;
                    return (
                      <div key={idx} className="space-y-1">
                        <div className="flex justify-between items-center text-[9px] font-bold">
                          <div className="flex items-center gap-1.5 text-gray-400">
                            {group.icon}
                            {group.label}
                          </div>
                          <span className="text-gray-500">
                            {group.value} ({percent}%)
                          </span>
                        </div>
                        <div className="h-1.5 w-full bg-black/40 rounded-full overflow-hidden border border-white/5">
                          <div
                            className={`h-full ${group.color} transition-all duration-700 ease-out rounded-full`}
                            style={{ width: `${percent}%` }}
                          />
                        </div>
                      </div>
                    );
                  })}
                </div>
              </div>

              <div className="grid grid-cols-2 gap-2 pt-2">
                {[
                  { l: "EAPOL", v: sniffer.eapol, c: "text-yellow-400" },
                  { l: "mDNS", v: sniffer.mdns, c: "text-accent" },
                  { l: "MQTT", v: sniffer.mqtt, c: "text-purple-400" },
                  { l: "QUIC", v: sniffer.quic, c: "text-blue-300" },
                ].map((s, i) => (
                  <div
                    key={i}
                    className="flex items-center justify-between px-3 py-2 bg-white/[0.02] border border-white/5 rounded-xl"
                  >
                    <span className="text-[8px] font-black text-gray-600 uppercase tracking-tighter">
                      {s.l}
                    </span>
                    <span className={`text-[10px] font-black ${s.c}`}>
                      {s.v || 0}
                    </span>
                  </div>
                ))}
              </div>
            </div>
          </div>
        
        </div>

      </div>
    </div>
  );
};
