import customtkinter as ctk
import serial
import serial.tools.list_ports
import threading
import time
import os
import sqlite3
import json
from tkinter import filedialog, messagebox, ttk
import datetime

ctk.set_appearance_mode("Dark")
ctk.set_default_color_theme("blue")

class SnifferApp(ctk.CTk):
    def __init__(self):
        super().__init__()
        self.title("ESP32 WiFi Sniffer Control Center")
        self.geometry("1100x700")
        self.minsize(900, 600)
        
        self.serial_port = None
        self.is_connected = False
        self.is_pcap_mode = False
        self.is_db_mode = False
        self.pcap_file = None
        self.read_thread = None
        self.stop_thread = False
        
        self.init_db()
        self.setup_ui()
        self.refresh_ports()

    def init_db(self):
        self.db_conn = sqlite3.connect("sniffer.db", check_same_thread=False)
        self.db_cursor = self.db_conn.cursor()
        self.db_cursor.execute("""
            CREATE TABLE IF NOT EXISTS packets (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
                proto TEXT,
                subtype TEXT,
                src TEXT,
                dst TEXT,
                rssi INTEGER,
                len INTEGER,
                info TEXT,
                channel INTEGER,
                srcMac TEXT,
                dstMac TEXT,
                ttl INTEGER,
                srcPort INTEGER,
                dstPort INTEGER
            )
        """)
        self.db_cursor.execute("""
            CREATE TABLE IF NOT EXISTS devices (
                mac TEXT PRIMARY KEY,
                first_seen DATETIME DEFAULT CURRENT_TIMESTAMP,
                last_seen DATETIME,
                ssid TEXT,
                rssi INTEGER,
                is_ap BOOLEAN,
                channel INTEGER,
                security TEXT,
                wifi_gen TEXT,
                vendor TEXT,
                packet_count INTEGER
            )
        """)
        self.db_conn.commit()

    def setup_ui(self):
        self.grid_columnconfigure(1, weight=1)
        self.grid_rowconfigure(0, weight=1)

        # --- Sidebar ---
        self.sidebar_frame = ctk.CTkFrame(self, width=200, corner_radius=0)
        self.sidebar_frame.grid(row=0, column=0, sticky="nsew")
        self.sidebar_frame.grid_rowconfigure(13, weight=1)

        self.logo_label = ctk.CTkLabel(self.sidebar_frame, text="ESP32 SNIFFER", font=ctk.CTkFont(size=20, weight="bold"))
        self.logo_label.grid(row=0, column=0, padx=20, pady=(20, 10))

        self.port_label = ctk.CTkLabel(self.sidebar_frame, text="Select COM Port:")
        self.port_label.grid(row=1, column=0, padx=20, pady=(10, 0), sticky="w")
        
        self.port_combo = ctk.CTkComboBox(self.sidebar_frame, values=["No Ports Found"])
        self.port_combo.grid(row=2, column=0, padx=20, pady=(5, 10), sticky="ew")

        self.refresh_btn = ctk.CTkButton(self.sidebar_frame, text="Refresh Ports", command=self.refresh_ports)
        self.refresh_btn.grid(row=3, column=0, padx=20, pady=5, sticky="ew")

        self.connect_btn = ctk.CTkButton(self.sidebar_frame, text="Connect", command=self.toggle_connection, fg_color="green", hover_color="darkgreen")
        self.connect_btn.grid(row=4, column=0, padx=20, pady=20, sticky="ew")

        # Sniffer Controls
        self.sniffer_start_btn = ctk.CTkButton(self.sidebar_frame, text="Start Sniffer (Text)", command=lambda: self.send_cmd("sniffer_start"), state="disabled")
        self.sniffer_start_btn.grid(row=7, column=0, padx=20, pady=5, sticky="ew")
        
        self.sniffer_stop_btn = ctk.CTkButton(self.sidebar_frame, text="Stop Sniffer", command=lambda: self.send_cmd("sniffer_stop"), state="disabled", fg_color="red", hover_color="darkred")
        self.sniffer_stop_btn.grid(row=8, column=0, padx=20, pady=5, sticky="ew")

        self.pcap_start_btn = ctk.CTkButton(self.sidebar_frame, text="Start PCAP Capture", command=self.start_pcap, state="disabled", fg_color="purple", hover_color="darkviolet")
        self.pcap_start_btn.grid(row=9, column=0, padx=20, pady=5, sticky="ew")
        
        self.pcap_stop_btn = ctk.CTkButton(self.sidebar_frame, text="Stop PCAP", command=self.stop_pcap, state="disabled")
        self.pcap_stop_btn.grid(row=10, column=0, padx=20, pady=5, sticky="ew")

        self.db_start_btn = ctk.CTkButton(self.sidebar_frame, text="Start DB Sync (JSON)", command=self.start_db_sync, state="disabled", fg_color="#d97706", hover_color="#b45309")
        self.db_start_btn.grid(row=11, column=0, padx=20, pady=5, sticky="ew")
        
        self.db_stop_btn = ctk.CTkButton(self.sidebar_frame, text="Stop DB Sync", command=self.stop_db_sync, state="disabled")
        self.db_stop_btn.grid(row=12, column=0, padx=20, pady=(5, 20), sticky="ew")

        # --- Main View (Tabs) ---
        self.tabview = ctk.CTkTabview(self)
        self.tabview.grid(row=0, column=1, padx=10, pady=(0, 10), sticky="nsew")
        self.tabview.add("Live Console")
        self.tabview.add("Database Viewer")

        # Live Console Tab
        self.tabview.tab("Live Console").grid_rowconfigure(1, weight=1)
        self.tabview.tab("Live Console").grid_columnconfigure(0, weight=1)

        self.status_label = ctk.CTkLabel(self.tabview.tab("Live Console"), text="Status: Disconnected", font=ctk.CTkFont(weight="bold"))
        self.status_label.grid(row=0, column=0, padx=10, pady=5, sticky="w")

        self.log_textbox = ctk.CTkTextbox(self.tabview.tab("Live Console"), font=ctk.CTkFont(family="Consolas", size=12))
        self.log_textbox.grid(row=1, column=0, padx=10, pady=5, sticky="nsew")

        self.cmd_entry = ctk.CTkEntry(self.tabview.tab("Live Console"), placeholder_text="Send raw command to ESP32...")
        self.cmd_entry.grid(row=2, column=0, padx=10, pady=(0, 10), sticky="ew")
        self.cmd_entry.bind("<Return>", lambda event: self.send_raw_cmd())

        # Database Viewer Tab
        self.tabview.tab("Database Viewer").grid_rowconfigure(1, weight=1)
        self.tabview.tab("Database Viewer").grid_columnconfigure(0, weight=1)

        self.db_controls = ctk.CTkFrame(self.tabview.tab("Database Viewer"))
        self.db_controls.grid(row=0, column=0, padx=10, pady=5, sticky="ew")
        
        self.db_refresh_btn = ctk.CTkButton(self.db_controls, text="Refresh Data", command=self.load_db_data)
        self.db_refresh_btn.pack(side="left", padx=5, pady=5)
        
        self.db_filter_entry = ctk.CTkEntry(self.db_controls, placeholder_text="Filter by Protocol (e.g. DNS, HTTP)")
        self.db_filter_entry.pack(side="left", padx=5, pady=5, expand=True, fill="x")
        self.db_filter_entry.bind("<Return>", lambda event: self.load_db_data())

        style = ttk.Style()
        style.theme_use("default")
        style.configure("Treeview", background="#2b2b2b", foreground="white", fieldbackground="#2b2b2b", borderwidth=0)
        style.configure("Treeview.Heading", background="#1f1f1f", foreground="white", relief="flat")
        style.map("Treeview", background=[('selected', '#1f538d')])

        columns = ("id", "time", "proto", "src", "dst", "rssi", "len", "info")
        self.tree = ttk.Treeview(self.tabview.tab("Database Viewer"), columns=columns, show="headings", style="Treeview")
        self.tree.heading("id", text="ID")
        self.tree.heading("time", text="Time")
        self.tree.heading("proto", text="Proto")
        self.tree.heading("src", text="Source")
        self.tree.heading("dst", text="Destination")
        self.tree.heading("rssi", text="RSSI")
        self.tree.heading("len", text="Len")
        self.tree.heading("info", text="Info")

        self.tree.column("id", width=50, stretch=False)
        self.tree.column("time", width=150, stretch=False)
        self.tree.column("proto", width=60, stretch=False)
        self.tree.column("src", width=120, stretch=False)
        self.tree.column("dst", width=120, stretch=False)
        self.tree.column("rssi", width=50, stretch=False)
        self.tree.column("len", width=50, stretch=False)
        self.tree.column("info", width=300)

        self.tree.grid(row=1, column=0, padx=10, pady=5, sticky="nsew")

    def load_db_data(self):
        for item in self.tree.get_children():
            self.tree.delete(item)
            
        query = "SELECT id, timestamp, proto, src, dst, rssi, len, info FROM packets"
        filter_text = self.db_filter_entry.get().strip()
        params = ()
        if filter_text:
            query += " WHERE proto LIKE ? OR info LIKE ?"
            params = (f"%{filter_text}%", f"%{filter_text}%")
            
        query += " ORDER BY id DESC LIMIT 500"
        
        try:
            self.db_cursor.execute(query, params)
            rows = self.db_cursor.fetchall()
            for row in rows:
                self.tree.insert("", ctk.END, values=row)
        except Exception as e:
            messagebox.showerror("DB Error", str(e))


    def refresh_ports(self):
        ports = serial.tools.list_ports.comports()
        port_list = [port.device for port in ports]
        if port_list:
            self.port_combo.configure(values=port_list)
            self.port_combo.set(port_list[0])
        else:
            self.port_combo.configure(values=["No Ports Found"])
            self.port_combo.set("No Ports Found")

    def log(self, message):
        self.log_textbox.insert(ctk.END, message + "\n")
        self.log_textbox.see(ctk.END)

    def toggle_connection(self):
        if self.is_connected:
            self.disconnect()
        else:
            self.connect()

    def connect(self):
        port = self.port_combo.get()
        if port == "No Ports Found" or not port:
            messagebox.showerror("Error", "No valid COM port selected.")
            return

        try:
            self.serial_port = serial.Serial(port, 115200, timeout=0.1)
            self.is_connected = True
            self.connect_btn.configure(text="Disconnect", fg_color="red", hover_color="darkred")
            self.status_label.configure(text=f"Status: Connected to {port}")
            self.port_combo.configure(state="disabled")
            self.refresh_btn.configure(state="disabled")
            
            # Enable buttons
            self.sniffer_start_btn.configure(state="normal")
            self.sniffer_stop_btn.configure(state="normal")
            self.pcap_start_btn.configure(state="normal")
            
            self.stop_thread = False
            self.read_thread = threading.Thread(target=self.serial_read_loop, daemon=True)
            self.read_thread.start()
            
            self.log(f"--- Connected to {port} at 115200 baud ---")
        except Exception as e:
            messagebox.showerror("Connection Error", str(e))

    def disconnect(self):
        self.stop_pcap()
        self.stop_thread = True
        time.sleep(0.2)
        if self.serial_port and self.serial_port.is_open:
            self.serial_port.close()
            
        self.is_connected = False
        self.connect_btn.configure(text="Connect", fg_color="green", hover_color="darkgreen")
        self.status_label.configure(text="Status: Disconnected")
        self.port_combo.configure(state="normal")
        self.refresh_btn.configure(state="normal")
        
        # Disable buttons
        self.sniffer_start_btn.configure(state="disabled")
        self.sniffer_stop_btn.configure(state="disabled")
        self.pcap_start_btn.configure(state="disabled")
        
        self.log("--- Disconnected ---")

    def send_cmd(self, cmd):
        if self.serial_port and self.serial_port.is_open:
            self.serial_port.write((cmd + "\n").encode('utf-8'))
            self.log(f"> Sent: {cmd}")

    def send_raw_cmd(self):
        cmd = self.cmd_entry.get()
        if cmd:
            self.send_cmd(cmd)
            self.cmd_entry.delete(0, ctk.END)

    def start_pcap(self):
        if not self.is_connected: return
        
        # Prompt for file to save
        timestamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
        filepath = filedialog.asksaveasfilename(
            defaultextension=".pcap",
            initialfile=f"capture_{timestamp}.pcap",
            title="Save PCAP File",
            filetypes=[("PCAP Files", "*.pcap"), ("All Files", "*.*")]
        )
        if not filepath:
            return

        try:
            self.pcap_file = open(filepath, 'wb')
            self.is_pcap_mode = True
            
            self.send_cmd("pcap_start")
            self.log(f"--- STARTED PCAP CAPTURE TO: {os.path.basename(filepath)} ---")
            
            self.pcap_start_btn.configure(state="disabled")
            self.pcap_stop_btn.configure(state="normal")
            self.status_label.configure(text=f"Status: PCAP Capture Active -> {os.path.basename(filepath)}", text_color="purple")
        except Exception as e:
            messagebox.showerror("File Error", str(e))

    def start_db_sync(self):
        if not self.is_connected: return
        self.is_db_mode = True
        self.send_cmd("db_sync_start")
        self.log("--- STARTED DB SYNC (JSON MODE) ---")
        self.db_start_btn.configure(state="disabled")
        self.db_stop_btn.configure(state="normal")
        self.status_label.configure(text=f"Status: DB Sync Active -> sniffer.db", text_color="#d97706")

    def stop_db_sync(self):
        if self.is_db_mode:
            self.send_cmd("db_sync_stop")
            self.is_db_mode = False
            self.log("--- STOPPED DB SYNC ---")
            self.db_start_btn.configure(state="normal")
            self.db_stop_btn.configure(state="disabled")
            self.status_label.configure(text=f"Status: Connected to {self.port_combo.get()}", text_color="white")

    def serial_read_loop(self):
        while not self.stop_thread and self.serial_port and self.serial_port.is_open:
            try:
                if self.is_pcap_mode:
                    if self.serial_port.in_waiting > 0:
                        data = self.serial_port.read(self.serial_port.in_waiting)
                        if self.pcap_file:
                            self.pcap_file.write(data)
                            self.pcap_file.flush()
                else:
                    if self.serial_port.in_waiting > 0:
                        line = self.serial_port.readline().decode('utf-8', errors='ignore').strip()
                        if line:
                            if self.is_db_mode and line.startswith('{') and line.endswith('}'):
                                try:
                                    doc = json.loads(line)
                                    if doc.get("type") == "packet":
                                        d = doc.get("data", {})
                                        self.db_cursor.execute('''
                                            INSERT INTO packets (proto, subtype, src, dst, rssi, len, info, channel, srcMac, dstMac, ttl, srcPort, dstPort)
                                            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
                                        ''', (
                                            d.get("proto"), d.get("subtype"), d.get("src"), d.get("dst"), 
                                            d.get("rssi"), d.get("len"), d.get("info"), d.get("channel"), 
                                            d.get("srcMac"), d.get("dstMac"), d.get("ttl"), d.get("srcPort"), d.get("dstPort")
                                        ))
                                        self.db_conn.commit()
                                        self.after(0, self.log, f"[DB] Saved Packet: {d.get('proto')} {d.get('src')} -> {d.get('dst')}")
                                    elif doc.get("type") == "devices":
                                        for d in doc.get("data", []):
                                            self.db_cursor.execute('''
                                                INSERT OR REPLACE INTO devices (mac, last_seen, ssid, rssi, is_ap, channel, security, wifi_gen, vendor, packet_count)
                                                VALUES (?, datetime('now', ?), ?, ?, ?, ?, ?, ?, ?, ?)
                                            ''', (
                                                d.get("mac"), f"-{d.get('lastSeen', 0) / 1000.0} seconds", d.get("ssid"), d.get("rssi"), 
                                                d.get("isAP"), d.get("channel"), d.get("security"), d.get("wifiGen"), d.get("vendor"), d.get("packetCount")
                                            ))
                                        self.db_conn.commit()
                                        self.after(0, self.log, f"[DB] Synced {len(doc.get('data', []))} Devices")
                                    elif doc.get("type") == "sys":
                                        self.after(0, self.log, f"[SYS] {doc.get('msg')}")
                                except json.JSONDecodeError:
                                    self.after(0, self.log, line)
                                except Exception as e:
                                    self.after(0, self.log, f"[DB Error] {e}")
                            else:
                                self.after(0, self.log, line)
                time.sleep(0.01)
            except serial.SerialException:
                break
            except Exception as e:
                print(f"Read error: {e}")
                break

if __name__ == "__main__":
    app = SnifferApp()
    app.mainloop()
