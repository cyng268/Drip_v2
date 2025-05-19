import serial
import time
import sys

class CameraSerial:
    def __init__(self):
        self.serial_port = None
        self.initialized = False
    
    def initialize_serial(self, port=None, baud_rate=9600):
        """Initialize the serial connection to the camera"""
        # Use the specified port or try common ones
        if port:
            ports_to_try = [port]
        else:
            ports_to_try = ["/dev/ttyUSB0", "/dev/ttyACM0", "/dev/ttyS0"]
        
        for port in ports_to_try:
            try:
                self.serial_port = serial.Serial(port, baud_rate, timeout=1)
                print(f"Serial port opened: {port}")
                self.serial_port.reset_input_buffer()
                self.initialized = True
                # Send initial command like in C++ code
                self.send_hex_command("8101044700000000FF")
                return True
            except Exception as e:
                print(f"Failed to open port {port}: {str(e)}")
        
        print("Failed to open any serial port")
        return False
    
    def dec_to_hex(self, decimal_number):
        """Convert decimal to hex format similar to C++ function"""
        hex_string = format(decimal_number, '02x')
        result = ''.join(['0' + c for c in hex_string])
        # Pad result to be 8 characters
        padded_result = '0' * (8 - len(result)) + result
        return padded_result
    
    def send_hex_command(self, hex_string):
        """Send a hex command to the camera and return the response"""
        if not self.initialized:
            if not self.initialize_serial():
                return "Serial error", None
        
        # Convert hex string to bytes
        try:
            # Remove any spaces in the hex string
            hex_string = hex_string.replace(" ", "")
            
            # Convert hex to bytes
            cmd_bytes = bytes.fromhex(hex_string)
            
            # Send command
            print(f"Sending command: {hex_string}")
            self.serial_port.write(cmd_bytes)
            
            # Wait for response
            time.sleep(0.5)
            response = b""
            while self.serial_port.in_waiting:
                response += self.serial_port.read(self.serial_port.in_waiting)
            
            # Convert response to hex string for display
            response_hex = response.hex().upper()
            
            # Process the response - trim acknowledgment and completion codes
            processed_response = self.process_response(response_hex)
            
            return "Command sent successfully", processed_response
        
        except Exception as e:
            return f"Error sending command: {str(e)}", None
    
    def process_response(self, response_hex):
        """Process the camera response by removing acknowledgment and completion codes"""
        if not response_hex:
            return ""
        
        # Remove all instances of acknowledgment code (9041FF) and completion code (9051FF)
        processed = response_hex
        processed = processed.replace("9041FF", "")
        processed = processed.replace("9051FF", "")
        processed = processed.replace("9042FF", "")
        processed = processed.replace("9052FF", "")
        
        # Log the processing
        if processed != response_hex:
            print(f"Original response: {response_hex}")
            print(f"Processed response: {processed}")
        
        return processed

    def zoom(self, level):
        """Send zoom command with specified level"""
        hex_level = self.dec_to_hex(level)
        cmd_str = "81010447" + hex_level + "FF"
        return self.send_hex_command(cmd_str)
    
    def icr(self, enable=True):
        """Enable or disable ICR (Infrared Cut-off Filter)"""
        cmd_str = "8101040102FF" if enable else "8101040103FF"
        return self.send_hex_command(cmd_str)
    
    def ir_correction(self, enable=True):
        """Enable or disable IR Correction"""
        cmd_str = "8101041101FF" if enable else "8101041100FF"
        return self.send_hex_command(cmd_str)
    
    def close(self):
        """Close the serial connection"""
        if self.serial_port and self.serial_port.is_open:
            self.serial_port.close()
            print("Serial port closed")


def main():
    camera = CameraSerial()
    
    # Try to initialize the serial connection
    if not camera.initialize_serial():
        print("Failed to initialize serial connection. Exiting.")
        sys.exit(1)
    
    print("\nCamera Serial Communication Tool")
    print("================================")
    print("Available commands:")
    print("  1. Send custom hex command")
    print("  2. Zoom (level: 0-16384)")
    print("  3. Toggle ICR (Infrared Cut-off Filter)")
    print("  4. Toggle IR Correction")
    print("  5. Exit")
    
    try:
        while True:
            choice = input("\nEnter command choice (1-5): ")
            
            if choice == '1':
                hex_cmd = input("Enter hex command (e.g., 8101044700000000FF): ")
                status, response = camera.send_hex_command(hex_cmd)
                print(f"Status: {status}")
                if response:
                    print(f"Response: {response}")
            
            elif choice == '2':
                try:
                    level = int(input("Enter zoom level (0-16384): "))
                    if 0 <= level <= 16384:
                        status, response = camera.zoom(level)
                        print(f"Status: {status}")
                        if response:
                            print(f"Response: {response}")
                    else:
                        print("Level must be between 0 and 16384")
                except ValueError:
                    print("Invalid input. Please enter a number.")
            
            elif choice == '3':
                enable = input("Enable ICR? (y/n): ").lower() == 'y'
                status, response = camera.icr(enable)
                print(f"Status: {status}")
                if response:
                    print(f"Response: {response}")
            
            elif choice == '4':
                enable = input("Enable IR Correction? (y/n): ").lower() == 'y'
                status, response = camera.ir_correction(enable)
                print(f"Status: {status}")
                if response:
                    print(f"Response: {response}")
            
            elif choice == '5':
                print("Exiting...")
                break
            
            else:
                print("Invalid choice. Please enter a number between 1 and 5.")
    
    except KeyboardInterrupt:
        print("\nProgram interrupted.")
    
    finally:
        camera.close()

if __name__ == "__main__":
    main()
