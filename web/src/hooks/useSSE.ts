import { useEffect, useRef } from 'react';
import { useStore } from '../store/useStore';
import { apiGetStatus } from '../utils/api';
import { getEndpoints } from '../utils/loadBalancer';

export const useSSE = () => {
  const { setStatus, setSniffer, setStress, addLogs, setIsOnline, isAuthenticated, addPingResult, addDevices } = useStore();
  const eventSourceRef = useRef<EventSource | null>(null);
  const deviceEventSourceRef = useRef<EventSource | null>(null);
  const endpointIndexRef = useRef(0);

  // Throttling buffers
  const logBufferRef = useRef<any[]>([]);
  const deviceBufferRef = useRef<any[]>([]);
  const throttleIntervalRef = useRef<any>(null);

  useEffect(() => {
    if (!isAuthenticated) {
      if (eventSourceRef.current) {
        eventSourceRef.current.close();
        eventSourceRef.current = null;
      }
      if (deviceEventSourceRef.current) {
        deviceEventSourceRef.current.close();
        deviceEventSourceRef.current = null;
      }
      setIsOnline(false);
      return;
    }

    // Start throttling timer
    throttleIntervalRef.current = setInterval(() => {
      if (logBufferRef.current.length > 0) {
        addLogs([...logBufferRef.current]);
        logBufferRef.current = [];
      }
      if (deviceBufferRef.current.length > 0) {
        addDevices([...deviceBufferRef.current]);
        deviceBufferRef.current = [];
      }
    }, 500); // Commit updates every 500ms

    const connect = () => {
      if (eventSourceRef.current) eventSourceRef.current.close();
      if (deviceEventSourceRef.current) deviceEventSourceRef.current.close();

      const token = useStore.getState().token;
      if (!token) return;

      const endpoints = getEndpoints();
      const endpoint = endpoints[endpointIndexRef.current % endpoints.length];
      endpointIndexRef.current++;

      const es = new EventSource(`${endpoint}/api/events?token=${encodeURIComponent(token)}`);
      eventSourceRef.current = es;

      const des = new EventSource(`${endpoint}/api/events/devices?token=${encodeURIComponent(token)}`);
      deviceEventSourceRef.current = des;

      es.onopen = () => {
        setIsOnline(true);
        console.log(`[SSE] Connected to ${endpoint}`);
      };

      es.onerror = () => {
        setIsOnline(false);
        console.error(`[SSE] Connection lost at ${endpoint}, retrying...`);
        es.close();
        des.close();
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

      es.addEventListener('stress', (e: MessageEvent) => {
        try {
          const data = JSON.parse(e.data);
          setStress(data);
        } catch (err) {
          console.error('[SSE] Failed to parse stress status', err);
        }
      });

      es.addEventListener('ping', (e: MessageEvent) => {
        try {
          const data = JSON.parse(e.data);
          addPingResult(data);
        } catch (err) {
          console.error('[SSE] Failed to parse ping result', err);
        }
      });

      es.addEventListener('logs', (e: MessageEvent) => {
        try {
          const data = JSON.parse(e.data);
          if (Array.isArray(data)) {
            logBufferRef.current.push(...data);
            if (logBufferRef.current.length > 500) {
              logBufferRef.current = logBufferRef.current.slice(-500);
            }
          }
        } catch (err) {
          console.error('[SSE] Failed to parse logs', err);
        }
      });

      des.addEventListener('devices', (e: MessageEvent) => {
        try {
          const data = JSON.parse(e.data);
          if (Array.isArray(data)) {
            deviceBufferRef.current.push(...data);
            if (deviceBufferRef.current.length > 200) {
              deviceBufferRef.current = deviceBufferRef.current.slice(-200);
            }
          }
        } catch (err) {
          console.error('[SSE-Device] Failed to parse devices', err);
        }
      });
    };

    connect();

    return () => {
      if (eventSourceRef.current) {
        eventSourceRef.current.close();
        eventSourceRef.current = null;
      }
      if (deviceEventSourceRef.current) {
        deviceEventSourceRef.current.close();
        deviceEventSourceRef.current = null;
      }
      if (throttleIntervalRef.current) {
        clearInterval(throttleIntervalRef.current);
      }
    };
  }, [setStatus, setSniffer, setStress, addLogs, setIsOnline, isAuthenticated, addPingResult, addDevices]);
};
