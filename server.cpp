#include <iostream>
#include <cstring>
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cerrno>

// ─────────────────────────────────────────────────────────────
// Parse the HTTP request line
// Input:  raw buffer from recv()
// Output: fills method and path by reference
// ─────────────────────────────────────────────────────────────
void parse_request(const std::string& request, std::string& method, std::string& path) {
    // get first line only (everything before first \r\n)
    std::string first_line = request.substr(0, request.find("\r\n"));

    // find first space — method ends here
    size_t first_space = first_line.find(' ');
    if (first_space == std::string::npos) {
        method = "UNKNOWN";
        path = "/";
        return;
    }

    // find second space — path ends here
    size_t second_space = first_line.find(' ', first_space + 1);
    if (second_space == std::string::npos) {
        method = "UNKNOWN";
        path = "/";
        return;
    }

    method = first_line.substr(0, first_space);
    path   = first_line.substr(first_space + 1, second_space - first_space - 1);
}

// ─────────────────────────────────────────────────────────────
// Build a complete HTTP response
// Input:  status code + message, content type, body
// Output: complete HTTP response string ready to send
// ─────────────────────────────────────────────────────────────
std::string build_response(const std::string& status,
                           const std::string& content_type,
                           const std::string& body) {
    std::string response;
    response += "HTTP/1.1 " + status + "\r\n";
    response += "Content-Type: " + content_type + "\r\n";
    response += "Content-Length: " + std::to_string(body.size()) + "\r\n";
    response += "Connection: close\r\n";
    response += "\r\n";   // ← mandatory blank line
    response += body;
    return response;
}

// ─────────────────────────────────────────────────────────────
// Router — decide what to send based on path
// Input:  method, path
// Output: complete HTTP response string
// ─────────────────────────────────────────────────────────────
std::string handle_request(const std::string& method, const std::string& path) {

    // we only handle GET for now
    if (method != "GET") {
        std::string body = "<html><body><h1>405 Method Not Allowed</h1></body></html>";
        return build_response("405 Method Not Allowed", "text/html", body);
    }

    // ── routes ───────────────────────────────────────────────
    if (path == "/") {
        std::string body =
            "<html>"
            "<head><title>Home</title></head>"
            "<body>"
            "<h1>Home Page</h1>"
            "<p>Welcome to my HTTP server written in C++.</p>"
            "<a href='/about'>About</a>"
            "</body>"
            "</html>";
        return build_response("200 OK", "text/html", body);
    }

    if (path == "/about") {
        std::string body =
            "<html>"
            "<head><title>About</title></head>"
            "<body>"
            "<h1>About</h1>"
            "<p>This server was built from scratch in C++ using POSIX sockets.</p>"
            "<a href='/'>Home</a>"
            "</body>"
            "</html>";
        return build_response("200 OK", "text/html", body);
    }

    // ── 404 — nothing matched ─────────────────────────────────
    std::string body =
        "<html>"
        "<head><title>404</title></head>"
        "<body>"
        "<h1>404 Not Found</h1>"
        "<p>The path <strong>" + path + "</strong> does not exist on this server.</p>"
        "<a href='/'>Go Home</a>"
        "</body>"
        "</html>";
    return build_response("404 Not Found", "text/html", body);
}

// ─────────────────────────────────────────────────────────────
// main
// ─────────────────────────────────────────────────────────────
int main() {

    // ── create socket ─────────────────────────────────────────
    int server_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_fd == -1) {
        std::cerr << "socket() failed: " << strerror(errno) << "\n";
        return 1;
    }

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        std::cerr << "setsockopt() failed: " << strerror(errno) << "\n";
        close(server_fd);
        return 1;
    }

    // ── bind ──────────────────────────────────────────────────
    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family      = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port        = htons(8080);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) == -1) {
        std::cerr << "bind() failed: " << strerror(errno) << "\n";
        close(server_fd);
        return 1;
    }

    // ── listen ────────────────────────────────────────────────
    if (listen(server_fd, 10) == -1) {
        std::cerr << "listen() failed: " << strerror(errno) << "\n";
        close(server_fd);
        return 1;
    }

    std::cout << "Server running on http://localhost:8080\n";
    std::cout << "Routes: /    /about    anything else = 404\n\n";

    // ── main loop — handle one request at a time ───────────────
    while (true) {

        struct sockaddr_in client_address;
        socklen_t client_len = sizeof(client_address);

        int client_fd = accept(server_fd, (struct sockaddr*)&client_address, &client_len);
        if (client_fd == -1) {
            std::cerr << "accept() failed: " << strerror(errno) << "\n";
            continue;  // don't exit — just wait for next connection
        }

        std::cout << "Connected: "
                  << inet_ntoa(client_address.sin_addr) << ":"
                  << ntohs(client_address.sin_port) << "\n";

        // ── read request ──────────────────────────────────────
        char buffer[4096];
        memset(buffer, 0, sizeof(buffer));
        ssize_t bytes_received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);

        if (bytes_received <= 0) {
            close(client_fd);
            continue;
        }

        std::string raw_request(buffer);

        // ── parse ─────────────────────────────────────────────
        std::string method, path;
        parse_request(raw_request, method, path);

        std::cout << method << " " << path << "\n";

        // ── route and respond ─────────────────────────────────
        std::string response = handle_request(method, path);
        send(client_fd, response.c_str(), response.size(), 0);

        // ── close this connection ─────────────────────────────
        close(client_fd);
    }

    close(server_fd);
    return 0;
}