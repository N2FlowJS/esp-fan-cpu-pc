export interface SystemStatus {
  rpm: number;
  temp: number;
  speed: number;
  mode: 'manual' | 'auto';
  healthAlert?: boolean;
  hostTempActive?: boolean;
  chip?: string;
  chipRev?: number;
  version?: string;
  cpu?: number;
  uptime?: number;
  freeHeap?: number;
  totalHeap?: number;
  totalPsram?: number;
  freePsram?: number;
  flash?: number;
  spiffsTotal?: number;
  spiffsUsed?: number;
  staIP?: string;
  apIP?: string;
  staSSID?: string;
  staPass?: string;
  staEnabled?: boolean;
  rssi?: number;
  sdk?: string;
  ledMode?: string;
  ledColor?: string;
  ledBrightness?: number;
  ledPin?: number;
}

export interface SnifferStats {
  active: boolean;
  concurrent: boolean;
  channel: number;
  packets: number;
  beacons: number;
  probes: number;
  deauths: number;
  data: number;
  arp: number;
  eapol: number;
  dns: number;
  dhcp: number;
  icmp: number;
  tcp: number;
  udp: number;
  other?: number;
  mdns?: number;
  llmnr?: number;
  nbns?: number;
  ssdp?: number;
  quic?: number;
  mqtt?: number;
  jammingAlert: boolean;
  devices: DeviceInfo[];
  logs: PacketLog[];
}

export interface DeviceInfo {
  mac: string;
  rssi: number;
  isAP: boolean;
  ssid?: string;
  channel?: number;
  security?: string;
  packets?: number;
  packetCount?: number;
  lastSeen?: number;
  vendor?: string;
  wifiGen?: string;
  clients?: number;
  utilization?: number;
  bssid?: string; // Confirmed associated AP MAC address
}

export interface SnifferDevice {
  mac: string;
  ssid?: string;
  rssi: number;
  isAP: boolean;
  channel: number;
  security?: string;
  wifiGen?: string;
  clients?: number;
  utilization?: number;
  vendor?: string;
}

export interface PacketLog {
  time: number;
  proto: string;
  subtype?: string;
  src: string;
  dst: string;
  rssi: number;
  info: string;
  len: number;
  channel?: number;
  srcMac?: string;
  dstMac?: string;
  rawHex?: string;
  ttl?: number;
  srcPort?: number;
  dstPort?: number;
}

export interface SnifferFilters {
  whitelist: string[];
  blacklist: string[];
}

export interface StressStatus {
  active: boolean;
}

export interface PingResult {
  type: 'result' | 'end';
  success?: boolean;
  time?: number;
  ttl?: number;
  bytes?: number;
  seq?: number;
  ip?: string;
  target?: string;
  transmitted?: number;
  received?: number;
  time_ms?: number;
}
