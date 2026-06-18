/**
 * api.ts – Centralized fetch wrapper for ESP32 REST API
 * Automatically attaches the session token from the store to every request.
 * Uses load balancing to distribute requests across multiple endpoints.
 */

import { useStore } from '../store/useStore';
import { fetchWithLoadBalancing } from './loadBalancer';

/** Base URL – empty string so it resolves relative to the current host (works both on-device and with Vite proxy) */
const BASE = '';

/** Build auth headers for authenticated endpoints */
function authHeaders(): HeadersInit {
  const token = useStore.getState().token;
  return {
    'Content-Type': 'application/json',
    'X-Token': token,
  };
}

/** Internal fetch wrapper that uses load balancing for /api calls */
const apiFetch = async (path: string, options?: RequestInit) => {
  // Check if we're in development mode (Vite dev server)
  const isDev = import.meta.env.DEV;
  
  // Use load balancer for /api calls, or regular fetch for relative paths in production
  if (isDev && path.startsWith('/api')) {
    return fetchWithLoadBalancing(path, options);
  }
  
  return fetch(`${BASE}${path}`, options);
};

/** GET /api/info – unauthenticated, returns system info containing ESP MAC address */
export async function apiGetInfo(): Promise<{ mac: string }> {
  const res = await apiFetch(`/api/info`);
  if (!res.ok) {
    throw new Error('Failed to fetch system info');
  }
  return res.json();
}

/** POST /api/login – returns token on success, throws on failure */
export async function apiLogin(password: string): Promise<string> {
  const res = await apiFetch(`/api/login`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ password }),
  });
  const data = await res.json();
  if (!res.ok || !data.ok) {
    throw new Error(data.error ?? 'Invalid password');
  }
  return data.token as string;
}

/** POST /api/led */
export const setLedConfig = async (mode: string, color?: string, brightness?: number, pin?: number) => {
  const res = await apiFetch(`/api/led`, {
    method: 'POST',
    headers: authHeaders(),
    body: JSON.stringify({ mode, color, brightness, pin }),
  });
  if (res.status === 401) { handleUnauthorized(); return null; }
  return res.json();
};

/** GET /api/status */
export async function apiGetStatus() {
  const res = await apiFetch(`/api/status`, { headers: authHeaders() });
  if (res.status === 401) { handleUnauthorized(); return null; }
  return res.json();
}

/** POST /api/speed */
export async function apiSetSpeed(speed: number) {
  const res = await apiFetch(`/api/speed`, {
    method: 'POST',
    headers: authHeaders(),
    body: JSON.stringify({ speed }),
  });
  if (res.status === 401) { handleUnauthorized(); return null; }
  return res.json();
}

/** POST /api/step */
export async function apiStep(step: number) {
  const res = await apiFetch(`/api/step`, {
    method: 'POST',
    headers: authHeaders(),
    body: JSON.stringify({ step }),
  });
  if (res.status === 401) { handleUnauthorized(); return null; }
  return res.json();
}

/** POST /api/mode */
export async function apiSetMode(mode: 'auto' | 'manual') {
  const res = await apiFetch(`/api/mode`, {
    method: 'POST',
    headers: authHeaders(),
    body: JSON.stringify({ mode }),
  });
  if (res.status === 401) { handleUnauthorized(); return null; }
  return res.json();
}

/** GET /api/sniffer/status */
export async function apiGetSnifferStatus() {
  const res = await apiFetch(`/api/sniffer/status`, { headers: authHeaders() });
  if (res.status === 401) { handleUnauthorized(); return null; }
  return res.json();
}

/** GET /api/sniffer/packets - fetch and clear pending packet logs (returns array) */
export async function apiGetSnifferPackets() {
  const res = await apiFetch(`/api/sniffer/packets`, { headers: authHeaders() });
  if (res.status === 401) { handleUnauthorized(); return null; }
  return res.json();
}

/** POST /api/sniffer/control */
export async function apiSnifferControl(active: boolean, channel?: number, concurrent?: boolean) {
  const res = await apiFetch(`/api/sniffer/control`, {
    method: 'POST',
    headers: authHeaders(),
    body: JSON.stringify({ active, channel: channel ?? 0, concurrent: concurrent ?? false }),
  });
  if (res.status === 401) { handleUnauthorized(); return null; }
  return res.json();
}

/** GET /api/sniffer/filters */
export async function apiGetSnifferFilters() {
  const res = await apiFetch(`/api/sniffer/filters`, { headers: authHeaders() });
  if (res.status === 401) { handleUnauthorized(); return null; }
  return res.json();
}

/** POST /api/sniffer/filters */
export async function apiSaveSnifferFilters(whitelist: string[], blacklist: string[]) {
  const res = await apiFetch(`/api/sniffer/filters`, {
    method: 'POST',
    headers: authHeaders(),
    body: JSON.stringify({ whitelist, blacklist }),
  });
  if (res.status === 401) { handleUnauthorized(); return null; }
  return res.json();
}

/** POST /api/sniffer/owner */
export async function apiRegisterOwnerMac(mac: string) {
  const res = await apiFetch(`/api/sniffer/owner`, {
    method: 'POST',
    headers: authHeaders(),
    body: JSON.stringify({ mac }),
  });
  if (res.status === 401) { handleUnauthorized(); return null; }
  return res.json();
}

/** POST /api/stress/control */
export async function apiStressControl(active: boolean, type?: string, targetMac?: string, clientMac?: string, channel?: number, rate?: number) {
  const res = await apiFetch(`/api/stress/control`, {
    method: 'POST',
    headers: authHeaders(),
    body: JSON.stringify({ active, type, targetMac, clientMac, channel, rate }),
  });
  if (res.status === 401) { handleUnauthorized(); return null; }
  return res.json();
}

/** GET /api/stress/status */
export async function apiGetStressStatus() {
  const res = await apiFetch(`/api/stress/status`, { headers: authHeaders() });
  if (res.status === 401) { handleUnauthorized(); return null; }
  return res.json();
}

/** GET /api/wifi/scan */
export async function apiGetWiFiScan() {
  const res = await apiFetch(`/api/wifi/scan`, { headers: authHeaders() });
  if (res.status === 401) { handleUnauthorized(); return null; }
  return res.json();
}

/** POST /api/ping */
export async function apiPing(target: string, count: number = 4, stop: boolean = false) {
  const res = await apiFetch(`/api/ping`, {
    method: 'POST',
    headers: authHeaders(),
    body: JSON.stringify({ target, count, stop }),
  });
  if (res.status === 401) { handleUnauthorized(); return null; }
  return res.json();
}

/** POST /api/wifi */
export async function apiSaveWiFi(ssid: string, pass: string) {
  const res = await apiFetch(`/api/wifi`, {
    method: 'POST',
    headers: authHeaders(),
    body: JSON.stringify({ ssid, pass }),
  });
  if (res.status === 401) { handleUnauthorized(); return null; }
  return res.json();
}

/** GET /api/fancurve */
export async function apiGetFanCurve(): Promise<{ temp: number, speed: number }[]> {
  const res = await apiFetch(`/api/fancurve`, { headers: authHeaders() });
  if (res.status === 401) { handleUnauthorized(); return []; }
  return res.json();
}

/** POST /api/fancurve */
export async function apiSetFanCurve(curve: { temp: number, speed: number }[]) {
  const res = await apiFetch(`/api/fancurve`, {
    method: 'POST',
    headers: authHeaders(),
    body: JSON.stringify(curve),
  });
  if (res.status === 401) { handleUnauthorized(); return null; }
  return res.json();
}

/** POST /api/password */
export async function apiSavePassword(password: string) {
  const res = await apiFetch(`/api/password`, {
    method: 'POST',
    headers: authHeaders(),
    body: JSON.stringify({ password }),
  });
  if (res.status === 401) { handleUnauthorized(); return null; }
  return res.json();
}

/** POST /api/wifi/toggle */
export async function apiToggleWiFi(enabled: boolean) {
  const res = await apiFetch(`/api/wifi/toggle`, {
    method: 'POST',
    headers: authHeaders(),
    body: JSON.stringify({ enabled }),
  });
  if (res.status === 401) { handleUnauthorized(); return null; }
  return res.json();
}

/** Called when ESP32 returns 401 – token expired/invalid, force re-login */
function handleUnauthorized() {
  useStore.getState().setAuthenticated(false);
}
