const OUI_MAP: { [key: string]: string } = {
  // Apple
  '00:1E:52': 'Apple', '00:26:08': 'Apple', '10:DD:B1': 'Apple', '18:EE:69': 'Apple', '1C:AB:A7': 'Apple',
  '34:FC:9F': 'Apple', '4C:ED:DE': 'Apple', '50:32:37': 'Apple', '54:26:96': 'Apple', '74:AC:5F': 'Apple',
  '7C:9E:BD': 'Apple', '80:EA:96': 'Apple', '8C:85:90': 'Apple', '90:B9:31': 'Apple', 'A4:CF:12': 'Apple',
  'A8:20:66': 'Apple', 'AC:BC:32': 'Apple', 'C8:2B:96': 'Apple', 'C8:3C:85': 'Apple', 'D8:3B:BF': 'Apple',
  'E8:9F:80': 'Apple', 'EC:AD:B8': 'Apple', 'F0:18:98': 'Apple', 'F4:0F:24': 'Apple', 'FC:FC:48': 'Apple',
  '00:03:93': 'Apple', '00:0A:27': 'Apple', '00:14:51': 'Apple', '00:16:CB': 'Apple', '00:19:E3': 'Apple',

  // Samsung
  '00:02:78': 'Samsung', '00:12:36': 'Samsung', '08:FC:88': 'Samsung', '40:B4:CD': 'Samsung', '4C:BC:9A': 'Samsung',
  '50:CC:F8': 'Samsung', '5C:0A:5B': 'Samsung', '60:6B:FF': 'Samsung', '68:05:71': 'Samsung', '74:45:CE': 'Samsung',
  '78:46:C4': 'Samsung', '84:25:DB': 'Samsung', '8C:77:12': 'Samsung', '90:18:7C': 'Samsung', '98:0C:A5': 'Samsung',
  '9C:02:98': 'Samsung', 'A0:82:1F': 'Samsung', 'A8:06:00': 'Samsung', 'B8:C6:8E': 'Samsung', 'C0:8C:60': 'Samsung',
  'C8:14:51': 'Samsung', 'CC:05:1B': 'Samsung', 'D0:C1:B1': 'Samsung', 'D4:87:D8': 'Samsung', 'D8:E5:6D': 'Samsung',
  'E0:99:71': 'Samsung', 'F4:09:D8': 'Samsung', 'F8:04:2E': 'Samsung', 'FC:A1:3E': 'Samsung', 'FC:75:16': 'Samsung',

  // Espressif (ESP8266 / ESP32)
  '18:FE:34': 'Espressif', '24:0A:C4': 'Espressif', '24:6F:28': 'Espressif', '24:B2:DE': 'Espressif', '30:AE:A4': 'Espressif',
  '3C:61:05': 'Espressif', '3C:71:BF': 'Espressif', '40:22:D8': 'Espressif', '40:91:51': 'Espressif', '44:17:93': 'Espressif',
  '48:27:E2': 'Espressif', '48:3F:DA': 'Espressif', '4C:11:AE': 'Espressif', '4C:75:25': 'Espressif', '50:02:91': 'Espressif',
  '54:5A:B6': 'Espressif', '54:EF:44': 'Espressif', '58:BF:25': 'Espressif', '5C:CF:7F': 'Espressif', '60:01:94': 'Espressif',
  '68:B6:B3': 'Espressif', '68:C6:3A': 'Espressif', '7C:87:CE': 'Espressif', '7C:DF:A1': 'Espressif',
  '80:7D:3A': 'Espressif', '84:CC:A8': 'Espressif', '84:F3:EB': 'Espressif', '90:38:0C': 'Espressif', '90:97:D5': 'Espressif',
  '94:B9:7E': 'Espressif', '98:F4:AB': 'Espressif', 'A0:B7:65': 'Espressif', 'A4:7B:9D': 'Espressif',
  'AC:0B:FB': 'Espressif', 'AC:67:B2': 'Espressif', 'B4:8A:0A': 'Espressif', 'B4:E6:2D': 'Espressif', 'BC:DD:C2': 'Espressif',
  'C0:49:EF': 'Espressif', 'C4:4F:33': 'Espressif', 'C4:DD:57': 'Espressif', 'C8:2E:18': 'Espressif', 'C8:C9:A3': 'Espressif',
  'CC:50:E3': 'Espressif', 'D4:D4:DA': 'Espressif', 'D8:A0:1D': 'Espressif', 'D8:BF:C0': 'Espressif', 'D8:F1:5B': 'Espressif',
  'DC:4F:22': 'Espressif', 'E0:5A:1B': 'Espressif', 'E0:E2:E6': 'Espressif', 'E8:68:E7': 'Espressif', 'EC:FA:BC': 'Espressif',
  'F0:08:D1': 'Espressif', 'F4:CF:A2': 'Espressif', '34:85:18': 'Espressif',

  // Google
  '00:1A:11': 'Google', '3C:5A:B4': 'Google', '54:60:09': 'Google', '70:5A:0F': 'Google', '80:D8:1A': 'Google',
  'A0:20:A6': 'Google', 'D8:50:E6': 'Google', 'F4:F5:D8': 'Google', 'F8:8F:CA': 'Google',

  // Xiaomi
  '18:04:ED': 'Xiaomi', '54:5A:A6': 'Xiaomi', '9C:9C:1E': 'Xiaomi', 'E0:E1:A9': 'Xiaomi', 'EC:35:86': 'Xiaomi',
  '00:9E:C8': 'Xiaomi', '28:6C:07': 'Xiaomi', '34:CE:00': 'Xiaomi', '50:64:2B': 'Xiaomi', '58:44:98': 'Xiaomi',
  '64:09:80': 'Xiaomi', '68:AB:1E': 'Xiaomi', '74:51:BA': 'Xiaomi', '8C:BE:BE': 'Xiaomi', 'A0:36:9F': 'Xiaomi',

  // Huawei
  'E4:E4:AB': 'Huawei', 'F4:12:FA': 'Huawei', '00:18:82': 'Huawei', '00:1E:10': 'Huawei', '00:25:68': 'Huawei',
  '00:46:4B': 'Huawei', '00:E0:FC': 'Huawei', '08:19:A6': 'Huawei', '10:1B:54': 'Huawei', '10:47:80': 'Huawei',
  '14:B9:68': 'Huawei', '20:08:ED': 'Huawei', '24:69:A5': 'Huawei', '28:3C:E4': 'Huawei', '2C:55:D3': 'Huawei',
  
  // Cisco
  '00:00:0C': 'Cisco', '00:01:42': 'Cisco', '00:1A:80': 'Cisco', '00:01:43': 'Cisco', '00:01:63': 'Cisco',
  '00:01:64': 'Cisco', '00:01:96': 'Cisco', '00:01:97': 'Cisco', '00:02:B9': 'Cisco', '00:02:FC': 'Cisco',
  
  // Intel
  '00:1E:67': 'Intel', '00:24:D7': 'Intel', '00:02:B3': 'Intel', '00:03:47': 'Intel', '00:04:23': 'Intel',
  '00:0C:F1': 'Intel', '00:0E:0C': 'Intel', '00:0F:F2': 'Intel', '00:11:75': 'Intel', '00:13:20': 'Intel',
  '00:13:CE': 'Intel', '00:15:00': 'Intel', '00:15:17': 'Intel', '00:16:6F': 'Intel', '00:16:EA': 'Intel',
  '00:18:DE': 'Intel', '00:19:D1': 'Intel', '00:1B:77': 'Intel', '00:1C:C0': 'Intel', '00:1C:C4': 'Intel',

  // Ubiquiti
  'FC:EC:DA': 'Ubiquiti', '00:15:6D': 'Ubiquiti', '00:27:22': 'Ubiquiti', '04:18:D6': 'Ubiquiti', '18:E8:29': 'Ubiquiti',
  '24:A4:3C': 'Ubiquiti', '44:D9:E7': 'Ubiquiti', '60:22:32': 'Ubiquiti', '68:D7:9A': 'Ubiquiti', '74:83:C2': 'Ubiquiti',
  '78:8A:20': 'Ubiquiti', '80:2A:A8': 'Ubiquiti', 'B4:FB:E4': 'Ubiquiti', 'D0:21:F9': 'Ubiquiti', 'E0:63:DA': 'Ubiquiti',
  
  // TP-Link
  '00:0A:EB': 'TP-Link', '00:1D:0F': 'TP-Link', '00:21:27': 'TP-Link', '00:23:CD': 'TP-Link', '00:25:86': 'TP-Link',
  '14:CC:20': 'TP-Link', '30:B5:C2': 'TP-Link', '50:3E:AA': 'TP-Link', '60:E3:27': 'TP-Link', '64:56:01': 'TP-Link',
  '70:4F:57': 'TP-Link', '7C:8B:CA': 'TP-Link', '90:F6:52': 'TP-Link', 'A0:F3:C1': 'TP-Link', 'B0:4E:26': 'TP-Link',
  'C0:06:C3': 'TP-Link', 'C4:E9:84': 'TP-Link', 'D8:0D:17': 'TP-Link', 'E8:4D:D0': 'TP-Link', 'F4:F2:6D': 'TP-Link',

  // Netgear
  '00:09:5B': 'Netgear', '00:0F:B5': 'Netgear', '00:14:6C': 'Netgear', '00:18:4D': 'Netgear', '00:1B:2F': 'Netgear',
  '00:1E:2A': 'Netgear', '00:22:3F': 'Netgear', '00:24:B2': 'Netgear', '00:26:F2': 'Netgear', '08:BD:43': 'Netgear',
  '10:0D:7F': 'Netgear', '14:59:C0': 'Netgear', '20:4E:7F': 'Netgear', '28:C6:8E': 'Netgear', '2C:30:33': 'Netgear',

  // ASUS
  '00:0C:6E': 'ASUS', '00:11:2F': 'ASUS', '00:13:D4': 'ASUS', '00:15:F2': 'ASUS', '00:17:31': 'ASUS',
  '00:18:F3': 'ASUS', '00:1A:92': 'ASUS', '00:1B:FC': 'ASUS', '00:1E:8C': 'ASUS', '00:22:15': 'ASUS',
  
  // Dell
  '00:14:22': 'Dell', '00:21:70': 'Dell', '00:06:5B': 'Dell', '00:08:74': 'Dell', '00:0B:DB': 'Dell',
  '00:0D:56': 'Dell', '00:0F:1F': 'Dell', '00:11:43': 'Dell', '00:12:3F': 'Dell', '00:13:72': 'Dell',
  
  // HP
  '00:08:02': 'HP', '00:0E:7F': 'HP', '00:0F:20': 'HP', '00:11:0A': 'HP', '00:12:79': 'HP',
  '00:13:21': 'HP', '00:14:38': 'HP', '00:15:60': 'HP', '00:16:35': 'HP', '00:17:A4': 'HP',

  // Sony
  '00:01:4A': 'Sony', '00:0A:D9': 'Sony', '00:0E:07': 'Sony', '00:13:15': 'Sony', '00:13:A9': 'Sony',
  '00:15:B7': 'Sony', '00:19:C5': 'Sony', '00:1A:8A': 'Sony', '00:1D:BA': 'Sony', '00:1E:B3': 'Sony',

  // Microsoft / Xbox
  '00:03:FF': 'Microsoft', '00:15:5D': 'Microsoft', '00:1D:D8': 'Microsoft', '00:22:48': 'Microsoft', '00:25:AE': 'Microsoft',
  '00:50:F2': 'Microsoft', '28:18:78': 'Microsoft', '30:59:B7': 'Microsoft', '50:1A:C5': 'Microsoft', '58:82:A8': 'Microsoft',
  '60:45:BD': 'Microsoft', '7C:1E:52': 'Microsoft', '98:5F:D3': 'Microsoft', 'B4:AE:2B': 'Microsoft', 'C8:3A:35': 'Microsoft',

  // Raspberry Pi
  'B8:27:EB': 'Raspberry Pi', 'DC:A6:32': 'Raspberry Pi', 'E4:5F:01': 'Raspberry Pi', '28:CD:C1': 'Raspberry Pi', 'D8:3A:DD': 'Raspberry Pi',

  // Virtualization
  '00:05:69': 'VMware', '00:0C:29': 'VMware', '00:1C:14': 'VMware', '00:50:56': 'VMware',
  '08:00:27': 'VirtualBox',
  '00:16:3E': 'Xen',
  
  // Other Notable
  '2C:F0:A2': 'Synology', '00:11:32': 'Synology',
  '00:10:FA': 'Apple', '00:25:90': 'Supermicro',
  '00:1A:2B': 'Airespace', '00:11:88': 'Brocade',
  '70:B3:D5': 'Linksys', '00:06:25': 'Linksys', '00:0C:41': 'Linksys', '00:0F:66': 'Linksys',
  '00:14:BF': 'Linksys', '00:16:B6': 'Linksys', '00:18:39': 'Linksys', '00:1A:70': 'Linksys',
};

export const getMacVendor = (mac: string): string => {
  if (!mac || mac.length < 8) return '';
  const cleanMac = mac.trim().toUpperCase().replace(/-/g, ':');
  const oui = cleanMac.slice(0, 8);
  return OUI_MAP[oui] || '';
};
