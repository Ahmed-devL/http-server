#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cerrno>

int main(){

    int server_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(server_fd == -1){
        std::cerr << "Failed to create socket: " << strerror(errno) << "\n";
        return 1;
    }

    int opt = 1;
    if(setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1){
        std::cerr << "Failed to set socket options:" << strerror(errno) << "\n";
        close(server_fd);
        return 1;
    }

    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(8080);

    if(bind(server_fd, (struct sockaddr *)&address, sizeof(address)) == -1){
        std::cerr << "Failed to bind socket: " << strerror(errno) << "\n";
        close(server_fd);
        return 1;
    }

    if(listen(server_fd, 3) == -1){
        std::cerr << "Failed to listen on socket: " << strerror(errno) << "\n";
        close(server_fd);
        return 1;
    }

    std::cout << "Server is listening on port 8080...\n";

    struct sockaddr_in client_address;
    socklen_t client_address_len = sizeof(client_address);
    int client_fd = accept(server_fd, (struct sockaddr *)&client_address, &client_address_len);
    if(client_fd == -1){
        std::cerr << "Failed to accept connection: " << strerror(errno) << "\n";
        close(server_fd);
        return 1;
    }

    std::cout << "Client connected: " << inet_ntoa(client_address.sin_addr) << ":" << ntohs(client_address.sin_port) << "\n";

    char buffer[4096];
    memset(buffer, 0, sizeof(buffer));
   ssize_t bytes_received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
if (bytes_received == -1) {
    std::cerr << "recv() failed: " << strerror(errno) << "\n";
    close(client_fd);
    close(server_fd);
    return 1;
}
if (bytes_received == 0) {
    std::cout << "Client disconnected before sending data.\n";
    close(client_fd);
    close(server_fd);
    return 1;
}
    std::cout << "\n--- Request received ---\n" << buffer << "\n";

    const char* response =
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: text/html\r\n"
    "Content-Length: 46\r\n"
    "Connection: close\r\n"
    "\r\n"
    "<html><body><h1>Hello World!</h1></body></html>";

int bytes_sent = send(client_fd, response, strlen(response), 0);

if (bytes_sent == -1) {
    std::cerr << "send() failed: " << strerror(errno) << "\n";
}

    close(client_fd);
    close(server_fd);
    return 0;




}