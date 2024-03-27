import socket

HOST = '192.168.1.15'
PORT = 5000

with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as client_socket:
    client_socket.connect((HOST, PORT))
    
    message = "Olá!"
    client_socket.sendall(message.encode())
    
    data = client_socket.recv(1024)
    print(f"Resposta do servidor: {data.decode()}")
