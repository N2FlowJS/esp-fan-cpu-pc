import customtkinter as ctk
import threading
import time
import os
import sqlite3
import json
import psutil
import requests
import sseclient
from tkinter import filedialog, messagebox, ttk
import datetime

ctk.set_appearance_mode("Dark")
ctk.set_default_color_theme("blue")

class SnifferApp(ctk.CTk):
    def __init__(self):
        super().__init__()
        self.title("ESP32 WiFi Sniffer Control Center (API Mode)")
        self.geometry("1100x750")
        self.minsize(900, 600)
        
        self.esp_ip = ""
        self.esp_pass = ""
        self.session_token = ""
        self.is_connected = False
        self.is_db_mode = False
        
        self.event_thread = None
        self.device_thread = None
        self.temp_thread = None
        self.stop_threads = False
        
        self.init_db()
        self.setup_ui()

    def init_db(self):
        self.db_conn = sqlite3.connect("sniffer.db", check_same_thread=False)
        self.db_cursor = self.db_conn.cursor()
        # ... (same tables as before)
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
        self.sidebar_frame.grid_rowconfigure(14, weight=1)

        self.logo_label = ctk.CTkLabel(self.sidebar_frame, text="ESP32 SNIFFER", font=ctk.CTkFont(size=20, weight="bold"))
        self.logo_label.grid(row=0, column=0, padx=20, pady=(20, 10))

        self.ip_label = ctk.CTkLabel(self.sidebar_frame, text="ESP32 IP Address:")
        self.ip_label.grid(row=1, column=0, padx=20, pady=(10, 0), sticky="w")
        self.ip_entry = ctk.CTkEntry(self.sidebar_frame, placeholder_text="e.g. 192.168.4.1")
        self.ip_entry.insert(0, "192.168.4.1")
        self.ip_entry.grid(row=2, column=0, padx=20, pady=(5, 10), sticky="ew")

        self.pass_label = ctk.CTkLabel(self.sidebar_frame, text="Access Password:")
        self.pass_label.grid(row=3, column=0, padx=20, pady=(0, 0), sticky="w")
        self.pass_entry = ctk.CTkEntry(self.sidebar_frame, placeholder_text="Enter password", show="*")
        self.pass_entry.grid(row=4, column=0, padx=20, pady=(5, 10), sticky="ew")

        self.connect_btn = ctk.CTkButton(self.sidebar_frame, text="Connect (API)", command=self.toggle_connection, fg_color="green", hover_color="darkgreen")
        self.connect_btn.grid(row=5, column=0, padx=20, pady=20, sticky="ew")

        # Sniffer Controls
        self.sniffer_start_btn = ctk.CTkButton(self.sidebar_frame, text="Enable Sniffer", command=lambda: self.api_cmd("sniffer/control", {"active": True}), state="disabled")
        self.sniffer_start_btn.grid(row=8, column=0, padx=20, pady=5, sticky="ew")
        
        self.sniffer_stop_btn = ctk.CTkButton(self.sidebar_frame, text="Disable Sniffer", command=lambda: self.api_cmd("sniffer/control", {"active": False}), state="disabled", fg_color="red", hover_color="darkred")
        self.sniffer_stop_btn.grid(row=9, column=0, padx=20, pady=5, sticky="ew")

        self.db_start_btn = ctk.CTkButton(self.sidebar_frame, text="Start DB Sync", command=self.start_db_sync, state="disabled", fg_color="#d97706", hover_color="#b45309")
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

    def log(self, message):
        self.log_textbox.insert(ctk.END, message + "\n")
        self.log_textbox.see(ctk.END)

    def toggle_connection(self):
        if self.is_connected:
            self.disconnect()
        else:
            self.connect()

    def connect(self):
        self.esp_ip = self.ip_entry.get().strip()
        self.esp_pass = self.pass_entry.get().strip()
        
        if not self.esp_ip:
            messagebox.showerror("Error", "Please enter ESP32 IP address.")
            return

        try:
            # 1. Login to get token
            self.log(f"--- Attempting API Login to {self.esp_ip} ---")
            resp = requests.post(f"http://{self.esp_ip}/api/login", 
                                 json={"password": self.esp_pass}, 
                                 timeout=5)
            data = resp.json()
            if not resp.ok or not data.get("ok"):
                messagebox.showerror("Auth Error", data.get("error", "Login failed"))
                return
            
            self.session_token = data.get("token")
            self.is_connected = True
            self.stop_threads = False
            
            self.connect_btn.configure(text="Disconnect", fg_color="red", hover_color="darkred")
            self.status_label.configure(text=f"Status: Connected to {self.esp_ip} (API)")
            self.ip_entry.configure(state="disabled")
            self.pass_entry.configure(state="disabled")
            
            # Enable buttons
            self.sniffer_start_btn.configure(state="normal")
            self.sniffer_stop_btn.configure(state="normal")
            self.db_start_btn.configure(state="normal")
            
            # Start SSE threads
            self.event_thread = threading.Thread(target=self.sse_loop, args=("events",), daemon=True)
            self.event_thread.start()
            self.device_thread = threading.Thread(target=self.sse_loop, args=("events/devices",), daemon=True)
            self.device_thread.start()
            
            # Start Host Temp Sync
            self.temp_thread = threading.Thread(target=self.host_temp_loop, daemon=True)
            self.temp_thread.start()
            
            self.log(f"--- API Session Initialized (Token: {self.session_token[:4]}...) ---")
        except Exception as e:
            messagebox.showerror("Connection Error", str(e))

    def disconnect(self):
        self.stop_threads = True
        self.is_connected = False
        self.session_token = ""
        
        self.connect_btn.configure(text="Connect (API)", fg_color="green", hover_color="darkgreen")
        self.status_label.configure(text="Status: Disconnected")
        self.ip_entry.configure(state="normal")
        self.pass_entry.configure(state="normal")
        
        self.sniffer_start_btn.configure(state="disabled")
        self.sniffer_stop_btn.configure(state="disabled")
        self.db_start_btn.configure(state="disabled")
        self.db_stop_btn.configure(state="disabled")
        
        self.log("--- Session Ended ---")

    def api_cmd(self, endpoint, data=None):
        if not self.is_connected: return
        try:
            url = f"http://{self.esp_ip}/api/{endpoint}"
            headers = {"X-Token": self.session_token}
            if data:
                requests.post(url, json=data, headers=headers, timeout=2)
            else:
                requests.get(url, headers=headers, timeout=2)
        except Exception as e:
            self.log(f"[API Error] {e}")

    def start_db_sync(self):
        self.is_db_mode = True
        self.db_start_btn.configure(state="disabled")
        self.db_stop_btn.configure(state="normal")
        self.status_label.configure(text=f"Status: DB Sync Active -> sniffer.db", text_color="#d97706")
        self.log("--- DB Sync Enabled (Receiving via SSE) ---")

    def stop_db_sync(self):
        self.is_db_mode = False
        self.db_start_btn.configure(state="normal")
        self.db_stop_btn.configure(state="disabled")
        self.status_label.configure(text=f"Status: Connected to {self.esp_ip}", text_color="white")
        self.log("--- DB Sync Disabled ---")

    def sse_loop(self, endpoint):
        """Listen for Server-Sent Events from ESP32."""
        while not self.stop_threads and self.is_connected:
            try:
                url = f"http://{self.esp_ip}/api/{endpoint}?token={self.session_token}"
                headers = {'Accept': 'text/event-stream'}
                
                response = requests.get(url, headers=headers, stream=True, timeout=30)
                client = sseclient.SSEClient(response)
                
                for event in client.events():
                    if self.stop_threads: break
                    
                    event_type = event.event
                    if not event.data: continue
                    
                    try:
                        data = json.loads(event.data)
                        if event_type == "logs" and self.is_db_mode:
                            # Handle batch logs
                            for d in data:
                                self.save_packet(d)
                        elif event_type == "devices" and self.is_db_mode:
                            # Handle batch devices
                            for d in data:
                                self.save_device(d)
                        elif event_type == "sniffer":
                            # Basic stats update (optional log)
                            pass
                        elif event_type == "status":
                            # System status update
                            pass
                        
                        # General log for console (limited)
                        if event_type == "logs":
                            first = data[0] if data else {}
                            self.after(0, self.log, f"[SSE] {len(data)} packets (First: {first.get('proto')} {first.get('src')}->{first.get('dst')})")
                        elif event_type == "devices":
                            self.after(0, self.log, f"[SSE] {len(data)} device updates")
                            
                    except Exception as e:
                        print(f"SSE Parse Error: {e}")
                        
            except Exception as e:
                if not self.stop_threads:
                    self.after(0, self.log, f"[SSE Reconnect] {endpoint}: {e}")
                    time.sleep(3)

    def save_packet(self, d):
        try:
            self.db_cursor.execute('''
                INSERT INTO packets (proto, subtype, src, dst, rssi, len, info, channel, srcMac, dstMac, ttl, srcPort, dstPort)
                VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
            ''', (
                d.get("proto"), d.get("subtype"), d.get("src"), d.get("dst"), 
                d.get("rssi"), d.get("len"), d.get("info"), d.get("channel"), 
                d.get("srcMac"), d.get("dstMac"), d.get("ttl"), d.get("srcPort"), d.get("dstPort")
            ))
            self.db_conn.commit()
        except Exception as e:
            print(f"DB Packet Error: {e}")

    def save_device(self, d):
        try:
            self.db_cursor.execute('''
                INSERT OR REPLACE INTO devices (mac, last_seen, ssid, rssi, is_ap, channel, security, wifi_gen, vendor, packet_count)
                VALUES (?, datetime('now', ?), ?, ?, ?, ?, ?, ?, ?, ?)
            ''', (
                d.get("mac"), f"-{d.get('lastSeen', 0)} seconds", d.get("ssid"), d.get("rssi"), 
                d.get("isAP"), d.get("channel"), d.get("security"), d.get("wifiGen"), d.get("vendor"), d.get("packetCount")
            ))
            self.db_conn.commit()
        except Exception as e:
            print(f"DB Device Error: {e}")

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

    def host_temp_loop(self):
        while not self.stop_threads and self.is_connected:
            try:
                max_temp = -1.0
                # Basic temp logic (simplified)
                try:
                    sensors = psutil.sensors_temperatures()
                    if sensors:
                        all_temps = [entry.current for sublist in sensors.values() for entry in sublist]
                        if all_temps: max_temp = max(all_temps)
                except: pass
                
                if max_temp > 0:
                    self.api_cmd("speed", {"speed": -1, "hostTemp": max_temp}) # Example using speed endpoint for temp sync
            except: pass
            time.sleep(5)

if __name__ == "__main__":
    app = SnifferApp()
    app.mainloop()

if __name__ == "__main__":
    app = SnifferApp()
    app.mainloop()
