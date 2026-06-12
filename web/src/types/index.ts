export interface SystemStatus {
  rpm: number;
  temp: number;
  speed: number;
  mode: 'manual' | 'auto';
  chip?: string;
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
  staSSID?: string;
  rssi?: number;
  sdk?: string;
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
}
