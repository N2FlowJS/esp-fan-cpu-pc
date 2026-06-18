export const formatUptime = (sec: number = 0) => {
  const h = Math.floor(sec / 3600);
  const m = Math.floor((sec % 3600) / 60);
  const s = sec % 60;
  return h > 0 ? `${h}h ${m}m ${s}s` : m > 0 ? `${m}m ${s}s` : `${s}s`;
};

export const formatBytes = (bytes: number = 0) => {
  if (bytes === 0) return '0 B';
  const k = 1024;
  const sizes = ['B', 'KB', 'MB'];
  const i = Math.floor(Math.log(bytes) / Math.log(k));
  return parseFloat((bytes / Math.pow(k, i)).toFixed(1)) + ' ' + sizes[i];
};

export const getRssiColor = (rssi: number) => {
  if (rssi >= -60) return 'text-green';
  if (rssi >= -80) return 'text-warm';
  return 'text-red';
};

export const getProtoColor = (proto: string) => {
  const p = proto.toUpperCase();
  if (p.startsWith('ARP')) return 'text-[#f6e05e]';
  if (p.includes('EAPOL') || p.includes('WPA')) return 'text-[#d6bcfa]';
  if (p === 'DNS' || p === 'MDNS') return 'text-[#76e4f7]';
  if (p.startsWith('DHCP')) return 'text-[#fc8181]';
  if (p === 'ICMP') return 'text-warm';
  if (p === 'HTTP') return 'text-warm';
  if (p === 'TCP') return 'text-accent';
  if (p === 'UDP') return 'text-[#a3bffa]';
  return 'text-gray-400';
};
