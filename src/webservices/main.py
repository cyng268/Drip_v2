from flask import Flask, request, jsonify
import serial
import serial.tools.list_ports
import binascii
import logging

app = Flask(__name__)
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

serial_port = None
SERIAL_INITIALIZED = False
MAX_ZOOM_LEVEL = 16384
MIN_ZOOM_LEVEL = 0
MIN_ZOOM_MULTIPLIER = 1.0
MAX_ZOOM_MULTIPLIER = 30.0
current_zoom_level = 0

def dec_to_hex(decimal_number):
    """Convert decimal to hex format as in C++ code"""
    hex_string = f"{decimal_number:02x}"
    result = ""
    for c in hex_string:
        result += "0" + c
    
    # Pad result to be 8 characters
    padded_result = "0" * (8 - len(result)) + result
    return padded_result

def initialize_serial():
    """Initialize serial connection to camera"""
    global serial_port, SERIAL_INITIALIZED
    
    # Try to find available serial ports
    serial_ports = ["/dev/ttyUSB0", "/dev/ttyACM0", "/dev/ttyS0"]
    
    for port in serial_ports:
        try:
            ser = serial.Serial(port, 9600, timeout=1)
            logger.info(f"Serial port opened: {port}")
            
            # Send initial command
            init_cmd = "8101044700000000FF"
            buffer = binascii.unhexlify(init_cmd)
            ser.write(buffer)
            
            serial_port = ser
            SERIAL_INITIALIZED = True
            return True
        except (serial.SerialException, OSError) as e:
            logger.warning(f"Failed to open port {port}: {e}")
    
    logger.error("Failed to open any serial port")
    return False

def send_zoom_command(level):
    """Send zoom command to camera"""
    global serial_port, SERIAL_INITIALIZED
    
    if not SERIAL_INITIALIZED:
        if not initialize_serial():
            return {"status": "error", "message": "Serial error"}
    
    hex_level = dec_to_hex(level)
    cmd_str = f"81010447{hex_level}FF"
    
    logger.info(f"Sending zoom command: {cmd_str}")
    
    try:
        buffer = binascii.unhexlify(cmd_str)
        serial_port.write(buffer)
        return {"status": "success", "message": f"Zoom level set to {level}", "zoom_level": level}
    except Exception as e:
        logger.error(f"Error sending zoom command: {e}")
        return {"status": "error", "message": str(e)}

def send_icr_command(enable):
    """Send ICR command to camera"""
    global serial_port, SERIAL_INITIALIZED
    
    if not SERIAL_INITIALIZED:
        if not initialize_serial():
            return {"status": "error", "message": "Serial error"}
    
    # Command to enable ICR: 81 01 04 01 02 FF
    # Command to disable ICR: 81 01 04 01 03 FF
    cmd_str = "8101040102FF" if enable else "8101040103FF"
    
    logger.info(f"Sending ICR command: {cmd_str} (Enable: {enable})")
    
    try:
        buffer = binascii.unhexlify(cmd_str)
        serial_port.write(buffer)
        return {"status": "success", "message": f"ICR Mode: {'ON' if enable else 'OFF'}", "icr_enabled": enable}
    except Exception as e:
        logger.error(f"Error sending ICR command: {e}")
        return {"status": "error", "message": str(e)}

def zoom_multiplier_to_level(multiplier):
    """Convert zoom multiplier (1.0x to 30.0x) to level (0 to 16384)"""
    # Ensure multiplier is within valid range
    multiplier = max(MIN_ZOOM_MULTIPLIER, min(MAX_ZOOM_MULTIPLIER, multiplier))
    
    # Calculate level using the inverse of the multiplier formula
    # Original formula: multiplier = (level / MAX_ZOOM_LEVEL) * MAX_ZOOM_MULTIPLIER
    # Inverted formula: level = (multiplier / MAX_ZOOM_MULTIPLIER) * MAX_ZOOM_LEVEL
    level = int((multiplier / MAX_ZOOM_MULTIPLIER) * MAX_ZOOM_LEVEL)
    
    # Ensure level is within bounds
    level = max(MIN_ZOOM_LEVEL, min(MAX_ZOOM_LEVEL, level))
    
    return level

def level_to_zoom_multiplier(level):
    """Convert level (0 to 16384) to zoom multiplier (1.0x to 30.0x)"""
    multiplier = (level / MAX_ZOOM_LEVEL) * MAX_ZOOM_MULTIPLIER
    # Ensure minimum zoom is 1.0x even at level 0
    multiplier = max(MIN_ZOOM_MULTIPLIER, multiplier)
    return round(multiplier, 1)

@app.route('/')
def index():
    return jsonify({"status": "ok", "message": "Camera Control API"})

@app.route('/icr/toggle')
def toggle_icr():
    """Toggle ICR mode using URL parameters"""
    # Get enable parameter from URL, default to None
    enable_param = request.args.get('enable', '').lower()
    
    if enable_param in ('true', '1', 'on', 'yes'):
        enable = True
    elif enable_param in ('false', '0', 'off', 'no'):
        enable = False
    else:
        return jsonify({"status": "error", "message": "Please specify 'enable' parameter as true or false"})
    
    result = send_icr_command(enable)
    return jsonify(result)

@app.route('/zoom')
def set_zoom():
    """Set zoom level by multiplier using URL parameters"""
    global current_zoom_level
    
    # Get multiplier parameter from URL
    multiplier_param = request.args.get('multiplier')
    
    if multiplier_param is None:
        return jsonify({"status": "error", "message": "Please specify 'multiplier' parameter"})
    
    try:
        multiplier = float(multiplier_param)
    except ValueError:
        return jsonify({"status": "error", "message": "Zoom multiplier must be a number"})
    
    if multiplier < MIN_ZOOM_MULTIPLIER or multiplier > MAX_ZOOM_MULTIPLIER:
        return jsonify({
            "status": "error", 
            "message": f"Zoom multiplier must be between {MIN_ZOOM_MULTIPLIER}x and {MAX_ZOOM_MULTIPLIER}x"
        })
    
    # Convert multiplier to level
    level = zoom_multiplier_to_level(multiplier)
    
    result = send_zoom_command(level)
    if result["status"] == "success":
        current_zoom_level = level
        
        # Use the exact multiplier the user requested in the response
        result["zoom_multiplier"] = multiplier
        result["message"] = f"Zoom set to {multiplier}x"
    
    return jsonify(result)

@app.route('/ir_correction')
def set_ir_correction():
    """Set IR correction mode using URL parameters"""
    enable_param = request.args.get('enable', '').lower()
    
    if enable_param in ('true', '1', 'on', 'yes'):
        enable = True
    elif enable_param in ('false', '0', 'off', 'no'):
        enable = False
    else:
        return jsonify({"status": "error", "message": "Please specify 'enable' parameter as true or false"})
    
    # Command to enable IR Correction: 81 01 04 11 01 FF
    # Command to disable IR Correction: 81 01 04 11 00 FF
    cmd_str = "8101041101FF" if enable else "8101041100FF"
    
    logger.info(f"Sending IR Correction command: {cmd_str} (Enable: {enable})")
    
    try:
        if not SERIAL_INITIALIZED:
            if not initialize_serial():
                return jsonify({"status": "error", "message": "Serial error"})
                
        buffer = binascii.unhexlify(cmd_str)
        serial_port.write(buffer)
        return jsonify({"status": "success", "message": f"IR Correction: {'ON' if enable else 'OFF'}", "ir_correction_enabled": enable})
    except Exception as e:
        logger.error(f"Error sending IR correction command: {e}")
        return jsonify({"status": "error", "message": str(e)})

if __name__ == '__main__':
    # Try to initialize serial at startup
    initialize_serial()
    # Run the API server on all network interfaces so it's accessible from other devices
    app.run(host='0.0.0.0', port=5000, debug=True)