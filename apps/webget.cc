#include "tcp_sponge_socket.hh"

#include <cstdlib>
#include <iostream>

using namespace std;

constexpr size_t kBufferSize = 1024;

void get_URL(const string &host, const string &path) {
    std::cerr << "Function called: get_URL(" << host << ", " << path << ").\n";

    CS144TCPSocket tcp_sock;
    Address addr(host, "http");
    tcp_sock.connect(addr);

    char req_str[kBufferSize];
    snprintf(req_str, sizeof(req_str),
             "GET %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n", path.c_str(), host.c_str());
    tcp_sock.write(req_str);
    tcp_sock.shutdown(SHUT_WR);

    std::string resp;
    while (!tcp_sock.eof()) {
        resp.append(tcp_sock.read());
    }
    tcp_sock.wait_until_closed();

    std::cout << resp;
}

int main(int argc, char *argv[]) {
    try {
        if (argc <= 0) {
            abort();  // For sticklers: don't try to access argv[0] if argc <= 0.
        }

        // The program takes two command-line arguments: the hostname and "path" part of the URL.
        // Print the usage message unless there are these two arguments (plus the program name
        // itself, so arg count = 3 in total).
        if (argc != 3) {
            cerr << "Usage: " << argv[0] << " HOST PATH\n";
            cerr << "\tExample: " << argv[0] << " stanford.edu /class/cs144\n";
            return EXIT_FAILURE;
        }

        // Get the command-line arguments.
        const string host = argv[1];
        const string path = argv[2];

        // Call the student-written function.
        get_URL(host, path);
    } catch (const exception &e) {
        cerr << e.what() << "\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
