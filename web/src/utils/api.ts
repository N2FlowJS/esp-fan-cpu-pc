/**
 * api.ts – Centralized fetch wrapper for ESP32 REST API
 * Automatically attaches the session token from the store to every request.
 */

import { useStore } from '../store/useStore';

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

/** POST /api/login – returns token on success, throws on failure */
export async function apiLogin(password: string): Promise<string> {
  const res = await fetch(`${BASE}/api/login`, {
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

/** GET /api/status */
export async function apiGetStatus() {
  const res = await fetch(`${BASE}/api/status`, { headers: authHeaders() });
  if (res.status === 401) { handleUnauthorized(); return null; }
  return res.json();
}

/** POST /api/speed */
export async function apiSetSpeed(speed: number) {
  const res = await fetch(`${BASE}/api/speed`, {
    method: 'POST',
    headers: authHeaders(),
    body: JSON.stringify({ speed }),
  });
  if (res.status === 401) { handleUnauthorized(); return null; }
  return res.json();
}

/** POST /api/step */
export async function apiStep(step: number) {
  const res = await fetch(`${BASE}/api/step`, {
    method: 'POST',
    headers: authHeaders(),
    body: JSON.stringify({ step }),
  });
  if (res.status === 401) { handleUnauthorized(); return null; }
  return res.json();
}

/** POST /api/mode */
export async function apiSetMode(mode: 'auto' | 'manual') {
  const res = await fetch(`${BASE}/api/mode`, {
    method: 'POST',
    headers: authHeaders(),
    body: JSON.stringify({ mode }),
  });
  if (res.status === 401) { handleUnauthorized(); return null; }
  return res.json();
}

/** GET /api/sniffer/status */
export async function apiGetSnifferStatus() {
  const res = await fetch(`${BASE}/api/sniffer/status`, { headers: authHeaders() });
  if (res.status === 401) { handleUnauthorized(); return null; }
  return res.json();
}

/** POST /api/sniffer/control */
export async function apiSnifferControl(active: boolean, channel?: number, concurrent?: boolean) {
  const res = await fetch(`${BASE}/api/sniffer/control`, {
    method: 'POST',
    headers: authHeaders(),
    body: JSON.stringify({ active, channel: channel ?? 0, concurrent: concurrent ?? false }),
  });
  if (res.status === 401) { handleUnauthorized(); return null; }
  return res.json();
}

/** POST /api/wifi */
export async function apiSaveWiFi(ssid: string, pass: string) {
  const res = await fetch(`${BASE}/api/wifi`, {
    method: 'POST',
    headers: authHeaders(),
    body: JSON.stringify({ ssid, pass }),
  });
  if (res.status === 401) { handleUnauthorized(); return null; }
  return res.json();
}

/** Called when ESP32 returns 401 – token expired/invalid, force re-login */
function handleUnauthorized() {
  useStore.getState().setAuthenticated(false);
}
