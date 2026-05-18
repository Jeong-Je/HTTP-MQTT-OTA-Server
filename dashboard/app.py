from flask import Flask, render_template, request, redirect
import os

app = Flask(__name__)

BASE_DIR = "../"

ECU_LIST = {
    "1234": "MOTOR ECU",
    "5678": "STEERING ECU"
}

# =========================
# UTIL
# =========================

def get_latest_version(ecu):

    version_file = f"{BASE_DIR}/hex/{ecu}/version.list"

    if not os.path.exists(version_file):
        return "0.0"

    with open(version_file, "r") as f:
        lines = f.readlines()

    if not lines:
        return "0.0"

    return lines[-1].strip()

# =========================
# MAIN PAGE
# =========================

@app.route("/")
def index():

    ecus = []

    for addr, name in ECU_LIST.items():

        version = get_latest_version(addr)

        ecus.append({
            "address": addr,
            "name": name,
            "version": version
        })

    return render_template(
        "index.html",
        ecus=ecus
    )

# =========================
# UPLOAD PAGE
# =========================

@app.route("/upload")
def upload_page():

    return render_template(
        "upload.html",
        ecus=ECU_LIST
    )

# =========================
# UPLOAD ACTION
# =========================

@app.route("/upload", methods=["POST"])
def upload_firmware():

    ecu = request.form["ecu"]
    version = request.form["version"]

    hex_file = request.files["hex_file"]
    sig_file = request.files["sig_file"]

    hex_dir = f"{BASE_DIR}/hex/{ecu}"
    sig_dir = f"{BASE_DIR}/sig/{ecu}"

    os.makedirs(hex_dir, exist_ok=True)
    os.makedirs(sig_dir, exist_ok=True)

    hex_path = f"{hex_dir}/{version}.hex"
    sig_path = f"{sig_dir}/{version}.sig"

    # 파일 저장
    hex_file.save(hex_path)
    sig_file.save(sig_path)

    # version.list append
    version_list = f"{hex_dir}/version.list"

    with open(version_list, "a") as f:
        f.write(version + "\n")

    print("\n==============================")
    print("[FIRMWARE UPLOAD]")
    print(f"ECU     : {ecu}")
    print(f"VERSION : {version}")
    print(f"HEX     : {hex_path}")
    print(f"SIG     : {sig_path}")
    print("==============================\n")

    return redirect("/")

# =========================
# MAIN
# =========================

if __name__ == "__main__":

    app.run(
        host="0.0.0.0",
        port=5000,
        debug=True
    )