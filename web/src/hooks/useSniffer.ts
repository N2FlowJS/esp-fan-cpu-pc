import React from 'react';
import { useStore } from '../store/useStore';
import { apiGetSnifferPackets } from '../utils/api';

export const useSniffer = () => {
  const sniffer = useStore(state => state.sniffer);
  const logs = useStore(state => state.logs);
  const clearLogs = useStore(state => state.clearLogs);
  const addLogs = useStore(state => state.addLogs);

  const toggleSniffer = async (active: boolean) => {
    // The actual API call is already handled by apiSnifferControl in SnifferTab.tsx
    // The state is updated via the SSE stream in useSSE.ts
  };

  // Poll pending packets endpoint when sniffer is active
  React.useEffect(() => {
    let interval: any = null;
    const poll = async () => {
      try {
        const data = await apiGetSnifferPackets();
        if (Array.isArray(data) && data.length > 0) {
          addLogs(data);
        }
      } catch (err) {
        // ignore polling errors silently
      }
    };

    if (sniffer && sniffer.active) {
      poll();
      interval = setInterval(poll, 500);
    }

    return () => { if (interval) clearInterval(interval); };
  }, [sniffer, addLogs]);

  return { sniffer, logs, toggleSniffer, clearLogs };
};
