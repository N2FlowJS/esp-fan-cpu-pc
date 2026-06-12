import { useState, useEffect, useCallback } from 'react';
import { SnifferStats, PacketLog } from '../types';

export const useSniffer = () => {
  const [sniffer, setSniffer] = useState<SnifferStats | null>(null);
  const [logs, setLogs] = useState<PacketLog[]>([]);

  const fetchSniffer = useCallback(async () => {
    if (!sniffer?.active) return;
    try {
      const res = await fetch('/api/sniffer/status');
      const data = await res.json();
      setSniffer(prev => ({ ...prev, ...data }));
      
      if (data.logs && data.logs.length > 0) {
        setLogs(prevLogs => {
          const newLogs = data.logs.filter((nl: PacketLog) => 
            !prevLogs.some(pl => pl.time === nl.time && pl.info === nl.info)
          );
          return [...newLogs, ...prevLogs].slice(0, 500);
        });
      }
    } catch {}
  }, [sniffer?.active]);

  useEffect(() => {
    const int = setInterval(fetchSniffer, 1000);
    return () => clearInterval(int);
  }, [fetchSniffer]);

  const toggleSniffer = async () => {
    const active = !sniffer?.active;
    try {
      const res = await fetch('/api/sniffer/control', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ active, channel: 0, concurrent: true })
      });
      const data = await res.json();
      if (data.ok) {
        setSniffer(prev => ({ ...(prev || {} as SnifferStats), active }));
      }
    } catch {}
  };

  const clearLogs = () => setLogs([]);

  return { sniffer, logs, toggleSniffer, clearLogs };
};
