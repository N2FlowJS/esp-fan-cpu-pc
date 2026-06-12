import { create } from 'zustand';
import { SystemStatus, SnifferStats, PacketLog } from '../types';

interface AppState {
  status: SystemStatus | null;
  sniffer: SnifferStats | null;
  logs: PacketLog[];
  isOnline: boolean;
  isAuthenticated: boolean;
  token: string;

  // Actions
  setStatus: (status: SystemStatus) => void;
  setSniffer: (sniffer: SnifferStats) => void;
  addLogs: (newLogs: PacketLog[]) => void;
  setIsOnline: (online: boolean) => void;
  clearLogs: () => void;
  setAuthenticated: (auth: boolean) => void;
  setToken: (token: string) => void;
}

export const useStore = create<AppState>((set) => ({
  status: null,
  sniffer: null,
  logs: [],
  isOnline: false,
  isAuthenticated: localStorage.getItem('is_auth') === 'true',
  token: localStorage.getItem('session_token') ?? '',

  setStatus: (status) => set({ status }),

  setSniffer: (sniffer) => set((state) => ({
    sniffer: { ...state.sniffer, ...sniffer },
  })),

  addLogs: (newLogs) => set((state) => {
    const uniqueNewLogs = newLogs.filter(nl =>
      !state.logs.some(pl => pl.time === nl.time && pl.info === nl.info)
    );
    if (uniqueNewLogs.length === 0) return state;
    return {
      logs: [...uniqueNewLogs, ...state.logs].slice(0, 500),
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
}));
