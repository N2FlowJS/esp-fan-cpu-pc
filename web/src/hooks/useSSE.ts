import { useEffect, useRef } from 'react';
import { useStore } from '../store/useStore';
import { apiGetStatus } from '../utils/api';

export const useSSE = () => {
  const { setStatus, setSniffer, addLogs, setIsOnline, isAuthenticated } = useStore();
  const eventSourceRef = useRef<EventSource | null>(null);

  useEffect(() => {
    if (!isAuthenticated) {
      if (eventSourceRef.current) {
        eventSourceRef.current.close();
        eventSourceRef.current = null;
      }
      setIsOnline(false);
      return;
    }

    const connect = () => {
      if (eventSourceRef.current) eventSourceRef.current.close();

      const token = useStore.getState().token;
      if (!token) return;

      const es = new EventSource(`/api/events?token=${encodeURIComponent(token)}`);
      eventSourceRef.current = es;

      es.onopen = () => {
        setIsOnline(true);
        console.log('[SSE] Connected');
      };

      es.onerror = () => {
        setIsOnline(false);
        console.error('[SSE] Connection lost, retrying...');
        es.close();

        // Verify if session token has expired or is invalid
        apiGetStatus().catch((err) => {
          console.error('[SSE] Auth validation request failed:', err);
        });

        setTimeout(connect, 3000);
      };

      es.addEventListener('status', (e: MessageEvent) => {
        try {
          const data = JSON.parse(e.data);
          setStatus(data);
        } catch (err) {
          console.error('[SSE] Failed to parse status', err);
        }
      });

      es.addEventListener('sniffer', (e: MessageEvent) => {
        try {
          const data = JSON.parse(e.data);
          setSniffer(data);
        } catch (err) {
          console.error('[SSE] Failed to parse sniffer stats', err);
        }
      });

      es.addEventListener('logs', (e: MessageEvent) => {
        try {
          const data = JSON.parse(e.data);
          if (Array.isArray(data)) addLogs(data);
        } catch (err) {
          console.error('[SSE] Failed to parse logs', err);
        }
      });
    };

    connect();

    return () => {
      if (eventSourceRef.current) {
        eventSourceRef.current.close();
        eventSourceRef.current = null;
      }
    };
  }, [setStatus, setSniffer, addLogs, setIsOnline, isAuthenticated]);
};
