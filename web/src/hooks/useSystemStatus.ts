import { useState, useEffect, useCallback } from 'react';
import { SystemStatus } from '../types';

export const useSystemStatus = () => {
  const [status, setStatus] = useState<SystemStatus | null>(null);
  const [isOnline, setIsOnline] = useState(false);

  const fetchStatus = useCallback(async () => {
    try {
      const res = await fetch('/api/status');
      const data = await res.json();
      setStatus(data);
      setIsOnline(true);
    } catch {
      setIsOnline(false);
    }
  }, []);

  useEffect(() => {
    const int = setInterval(fetchStatus, 1000);
    fetchStatus();
    return () => clearInterval(int);
  }, [fetchStatus]);

  const setMode = async (mode: 'manual' | 'auto') => {
    try {
      await fetch('/api/mode', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ mode })
      });
      setStatus(s => s ? ({ ...s, mode }) : null);
    } catch {}
  };

  const setSpeed = async (speed: number) => {
    try {
      await fetch('/api/speed', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ speed })
      });
      setStatus(s => s ? ({ ...s, speed }) : null);
    } catch {}
  };

  const stepSpeed = async (delta: number) => {
    try {
      const res = await fetch('/api/step', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ step: delta })
      });
      const data = await res.json();
      if (data.ok) setStatus(s => s ? ({ ...s, speed: data.speed }) : null);
    } catch {}
  };

  return { status, isOnline, setMode, setSpeed, stepSpeed };
};
