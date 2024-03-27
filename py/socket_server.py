import socket

HOST = '192.168.1.15'
PORT = 5000

with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as server_socket:
    server_socket.bind((HOST, PORT))
    server_socket.listen(5)
    print(f"Servidor TCP ouvindo em {HOST}:{PORT}...")

    while True:
        client_socket, client_address = server_socket.accept()
        print(f"Conectado por {client_address}")

        with client_socket:
            while True:
                data = client_socket.recv(1024)
                if not data:
                    print(f"Cliente {client_address} desconectado.")
                    break
                
                print(f"Dados recebidos: {data.decode()}")
                response = "Mensagem recebida"
                client_socket.sendall(response.encode())
