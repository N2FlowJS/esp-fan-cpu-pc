import { create } from 'zustand';
import { persist, createJSONStorage } from 'zustand/middleware';
import { SystemStatus, SnifferStats, PacketLog, StressStatus, PingResult, DeviceInfo } from '../types';

interface AppState {
  status: SystemStatus | null;
  sniffer: SnifferStats | null;
  stress: StressStatus | null;
  pingResults: PingResult[];
  logs: PacketLog[];
  isOnline: boolean;
  isAuthenticated: boolean;
  token: string;

  // Actions
  setStatus: (status: SystemStatus) => void;
  setSniffer: (sniffer: SnifferStats) => void;
  setStress: (stress: StressStatus) => void;
  addPingResult: (res: PingResult) => void;
  clearPingResults: () => void;
  addLogs: (newLogs: PacketLog[]) => void;
  addDevices: (newDevices: DeviceInfo[]) => void;
  setIsOnline: (online: boolean) => void;
  clearLogs: () => void;
  setAuthenticated: (auth: boolean) => void;
  setToken: (token: string) => void;
  clearAllState: () => void;
}

const initialState = {
  status: null,
  sniffer: {
    active: false,
    concurrent: false,
    channel: 0,
    packets: 0,
    beacons: 0,
    probes: 0,
    deauths: 0,
    data: 0,
    arp: 0,
    eapol: 0,
    dns: 0,
    dhcp: 0,
    icmp: 0,
    tcp: 0,
    udp: 0,
    jammingAlert: false,
    devices: [],
    logs: []
  },
  stress: { active: false },
  pingResults: [],
  logs: [],
  isOnline: false,
  isAuthenticated: false,
  token: '',
};

export const useStore = create<AppState>()(
  persist(
    (set) => ({
      ...initialState,
      isAuthenticated: localStorage.getItem('is_auth') === 'true',
      token: localStorage.getItem('session_token') ?? '',

      clearAllState: () => set(initialState),

      setStatus: (status) => set({ status }),

      setSniffer: (sniffer) => set((state) => {
        const updatedSniffer = state.sniffer ? { ...state.sniffer, ...sniffer } : sniffer as SnifferStats;
        
        // If sniffer data contains devices, merge them with existing ones
        if (sniffer.devices && sniffer.devices.length > 0) {
          const deviceMap = new Map();
          state.sniffer?.devices.forEach(d => deviceMap.set(d.mac, d));
          sniffer.devices.forEach(d => {
            const existing = deviceMap.get(d.mac);
            deviceMap.set(d.mac, existing ? { ...existing, ...d } : d);
          });
          updatedSniffer.devices = Array.from(deviceMap.values());
        }
        
        return { sniffer: updatedSniffer };
      }),

      setStress: (stress) => set({ stress }),

      addPingResult: (res) => set((state) => ({
        pingResults: [...state.pingResults, res].slice(-50)
      })),

      clearPingResults: () => set({ pingResults: [] }),

      addLogs: (newLogs) => set((state) => {
        // No hard limit for logs, but we keep it reasonable for memory (e.g., 10k)
        const maxLogs = 10000;
        const combined = [...newLogs, ...state.logs];
        
        // Auto-association logic: scan new logs for Data/WPA frames to link STAs to APs
        let devicesChanged = false;
        const deviceMap = new Map();
        
        if (state.sniffer && newLogs.length > 0) {
            state.sniffer.devices.forEach(d => deviceMap.set(d.mac, { ...d }));
            
            newLogs.forEach(log => {
                if (log.proto === 'DATA' || log.proto === 'QoS DATA' || log.proto === 'WPA HS' || log.proto === 'DEAUTH') {
                    // Try to determine which MAC is AP and which is STA.
                    // This relies on the backend sending accurate srcMac/dstMac and us tracking APs.
                    const srcMac = log.srcMac || log.src;
                    const dstMac = log.dstMac || log.dst;
                    
                    if (!srcMac || !dstMac || !srcMac.includes(':') || !dstMac.includes(':')) return;

                    const srcDev = deviceMap.get(srcMac);
                    const dstDev = deviceMap.get(dstMac);

                    if (srcDev?.isAP && dstDev && !dstDev.isAP && dstDev.bssid !== srcMac) {
                        dstDev.bssid = srcMac;
                        devicesChanged = true;
                    } else if (dstDev?.isAP && srcDev && !srcDev.isAP && srcDev.bssid !== dstMac) {
                        srcDev.bssid = dstMac;
                        devicesChanged = true;
                    }
                }
            });
        }

        const newState: Partial<AppState> = { logs: combined.slice(0, maxLogs) };
        
        if (devicesChanged && state.sniffer) {
            newState.sniffer = {
                ...state.sniffer,
                devices: Array.from(deviceMap.values())
            };
        }

        return newState;
      }),

      addDevices: (newDevices) => set((state) => {
        if (!newDevices || newDevices.length === 0) return state;
        
        const deviceMap = new Map();
        // Seed map with existing devices
        state.sniffer?.devices.forEach(d => deviceMap.set(d.mac, d));
        
        // Merge new updates
        let changed = false;
        newDevices.forEach(d => {
          const existing = deviceMap.get(d.mac);
          if (!existing) {
            deviceMap.set(d.mac, d);
            changed = true;
          } else {
            // Optimization: Only trigger update if data actually changed significantly.
            // RSSI changes are constant, so we use a small threshold (2dBm) to reduce noise.
            const rssiChanged = Math.abs(existing.rssi - d.rssi) > 2;
            const metaChanged = existing.packetCount !== d.packetCount || 
                                existing.ssid !== d.ssid || 
                                existing.channel !== d.channel;
            
            if (rssiChanged || metaChanged) {
              deviceMap.set(d.mac, { ...existing, ...d });
              changed = true;
            }
          }
        });
        
        if (!changed) return state;
        
        return {
          sniffer: state.sniffer ? {
            ...state.sniffer,
            devices: Array.from(deviceMap.values())
          } : null
        };
      }),

      setIsOnline: (online) => set({ isOnline: online }),

      setToken: (token) => {
        localStorage.setItem('session_token', token);
        set({ token });
      },

      setAuthenticated: (auth) => {
        localStorage.setItem('is_auth', auth ? 'true' : 'false');
        if (!auth) {
          localStorage.removeItem('session_token');
          set({ isAuthenticated: false, token: '' });
        } else {
          set({ isAuthenticated: true });
        }
      },

      clearLogs: () => set({ logs: [] }),
    }),
    {
      name: 'esp32-sniffer-storage',
      storage: createJSONStorage(() => localStorage),
      // Only persist specific fields to avoid saving transient states like isOnline
      // We exclude logs because they can grow quite large and exceed localStorage limits.
      partialize: (state) => ({ 
        sniffer: state.sniffer,
        token: state.token,
        isAuthenticated: state.isAuthenticated
      }),
    }
  )
);
