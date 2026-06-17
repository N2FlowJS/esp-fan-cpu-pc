# For IDE linter compatibility
Import = globals().get("Import")
env = globals().get("env")

Import("env")
import os
import subprocess
import sys

def build_web_ui(source, target, env):
    project_dir = env.get("PROJECT_DIR")
    web_dir = os.path.join(project_dir, "web")
    
    print("==================================================")
    print("[WEB BUILD] Automatically building React Web UI...")
    print("==================================================")
    
    try:
        # Automatically install dependencies if missing
        if not os.path.exists(os.path.join(web_dir, "node_modules")):
            print("[WEB BUILD] node_modules not found. Running npm install first...")
            subprocess.run("npm install", shell=True, cwd=web_dir, check=True)
            
        print("[WEB BUILD] Running npm run build (filtered stdout)...")
        
        # Run and filter output to avoid Unicode errors on Windows (CP1252)
        process = subprocess.Popen(
            "npm run build",
            shell=True,
            cwd=web_dir,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            encoding="utf-8"
        )
        
        while True:
            line = process.stdout.readline()
            if not line:
                break
            # Replace common Unicode characters with safe ASCII characters
            clean_line = line.replace("\u2713", "[OK]").replace("\u2717", "[FAIL]")
            clean_line = clean_line.encode("ascii", "ignore").decode("ascii")
            print(f"[WEB BUILD] {clean_line}", end="")
            sys.stdout.flush()
            
        process.wait()
        if process.returncode != 0:
            raise RuntimeError(f"npm run build failed with exit code {process.returncode}")
            
        print("[WEB BUILD] Web UI build completed successfully!")
    except Exception as e:
        print(f"[WEB BUILD] Error building Web UI: {e}")
        raise RuntimeError("Failed to build Web UI")

# Automatically build Web UI and upload LittleFS before uploading firmware
def auto_build_and_upload_fs(source, target, env):
    print("==================================================")
    print("[AUTO-FS] Triggering LittleFS build and upload...")
    print("==================================================")
    port = env.get("UPLOAD_PORT")
    cmd = "pio run -t uploadfs"
    if port:
        cmd += f" --upload-port {port}"
    env.Execute(cmd)

# Register hooks
env.AddPreAction("buildfs", build_web_ui)
env.AddPreAction("uploadfs", build_web_ui)
env.AddPreAction("upload", auto_build_and_upload_fs)

def launch_monitor(source, target, env):
    print("==================================================")
    print("[POST-UPLOAD] Launching Serial Monitor...")
    print("==================================================")
    port = env.get("UPLOAD_PORT")
    cmd = "pio device monitor"
    if port:
        cmd += f" --port {port}"
    env.Execute(cmd)

env.AddPostAction("upload", launch_monitor)


