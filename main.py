#!/usr/bin/env python3
import socket

def send_fix_message(host='localhost', port=9000):
    SOH = '\x01'
    
    # FIX Logon Message
    logon_msg = f"8=FIX.4.2{SOH}35=A{SOH}10=000{SOH}"
    
    # FIX New Order Single Message
    order_msg = f"8=FIX.4.2{SOH}35=D{SOH}55=AAPL{SOH}54=1{SOH}44=150.50{SOH}38=100{SOH}10=000{SOH}"
    
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
        sock.connect((host, port))
        print(f"Connected to {host}:{port}")
        
        # Send logon
        sock.sendall(logon_msg.encode())
        print("Sent Logon message")
        
        # Send order
        sock.sendall(order_msg.encode())
        print("Sent Order message")
        
        # Keep connection open briefly
        import time
        time.sleep(1)
        
    except Exception as e:
        print(f"Error: {e}")
    finally:
        sock.close()
        print("Connection closed")

if __name__ == "__main__":
    send_fix_message()