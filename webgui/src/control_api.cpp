#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <WinSock2.h>
#include <WS2tcpip.h>

#include "config_json.h"
#include "control_api.h"
#include "key_capture_win.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string_view>
#include <vector>

#pragma comment(lib, "ws2_32.lib")

namespace joycon::webgui {
namespace {

using SocketHandle = SOCKET;
using json = nlohmann::json;

struct HttpResponse {
    int status = 200;
    std::string contentType = "application/json; charset=utf-8";
    std::string body;
};

HttpResponse JsonError(int status, const std::string& message);

std::string ReadFileText(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

std::vector<char> ReadFileBytes(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    return std::vector<char>((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

std::string GetContentType(const std::filesystem::path& path) {
    const auto ext = path.extension().string();
    if (ext == ".html") return "text/html; charset=utf-8";
    if (ext == ".js") return "application/javascript; charset=utf-8";
    if (ext == ".css") return "text/css; charset=utf-8";
    if (ext == ".json") return "application/json; charset=utf-8";
    if (ext == ".avif") return "image/avif";
    return "application/octet-stream";
}

bool TryParseJsonBody(const std::string& body, HttpResponse& response, json& parsed) {
    try {
        parsed = body.empty() ? json::object() : json::parse(body);
        return true;
    } catch (const std::exception& ex) {
        response = JsonError(400, ex.what());
        return false;
    }
}

HttpResponse JsonError(int status, const std::string& message) {
    HttpResponse response;
    response.status = status;
    response.body = json{
        { "ok", false },
        { "error", message }
    }.dump();
    return response;
}

void SendResponse(SocketHandle socketHandle, const HttpResponse& response) {
    std::ostringstream header;
    header
        << "HTTP/1.1 " << response.status << " "
        << (response.status == 200 ? "OK" : "Error") << "\r\n"
        << "Content-Type: " << response.contentType << "\r\n"
        << "Content-Length: " << response.body.size() << "\r\n"
        << "Connection: close\r\n\r\n";

    const std::string headerText = header.str();
    send(socketHandle, headerText.data(), static_cast<int>(headerText.size()), 0);
    send(socketHandle, response.body.data(), static_cast<int>(response.body.size()), 0);
}

HttpResponse ServeFile(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path) || !std::filesystem::is_regular_file(path)) {
        return JsonError(404, "File not found.");
    }

    HttpResponse response;
    response.contentType = GetContentType(path);

    if (response.contentType.rfind("image/", 0) == 0 || response.contentType == "application/octet-stream") {
        const auto bytes = ReadFileBytes(path);
        response.body.assign(bytes.begin(), bytes.end());
    } else {
        response.body = ReadFileText(path);
    }
    return response;
}

std::string NormalizePath(std::string path) {
    const auto query = path.find('?');
    if (query != std::string::npos) {
        path = path.substr(0, query);
    }
    return path;
}

} // namespace

ControlApiServer::ControlApiServer(
    MapperRuntime& runtime,
    ConfigStore& configStore,
    std::filesystem::path staticRoot)
    : runtime_(runtime),
      configStore_(configStore),
      staticRoot_(std::move(staticRoot)) {}

ControlApiServer::~ControlApiServer() {
    Stop();
}

bool ControlApiServer::Start(uint16_t port, std::string& errorMessage) {
    WSADATA wsaData{};
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        errorMessage = "WSAStartup failed.";
        return false;
    }

    listenSocket_ = static_cast<uintptr_t>(socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
    if (listenSocket_ == static_cast<uintptr_t>(INVALID_SOCKET)) {
        errorMessage = "Failed to create socket.";
        return false;
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    inet_pton(AF_INET, "127.0.0.1", &address.sin_addr);

    const int reuse = 1;
    setsockopt(static_cast<SocketHandle>(listenSocket_), SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));

    if (bind(static_cast<SocketHandle>(listenSocket_), reinterpret_cast<const sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR) {
        errorMessage = "Failed to bind localhost port.";
        closesocket(static_cast<SocketHandle>(listenSocket_));
        listenSocket_ = 0;
        return false;
    }

    if (listen(static_cast<SocketHandle>(listenSocket_), SOMAXCONN) == SOCKET_ERROR) {
        errorMessage = "Failed to listen on localhost port.";
        closesocket(static_cast<SocketHandle>(listenSocket_));
        listenSocket_ = 0;
        return false;
    }

    port_ = port;
    running_.store(true);
    acceptThread_ = std::thread([this]() { AcceptLoop(); });
    errorMessage.clear();
    return true;
}

void ControlApiServer::Stop() {
    running_.store(false);

    if (listenSocket_ != 0) {
        closesocket(static_cast<SocketHandle>(listenSocket_));
        listenSocket_ = 0;
    }

    if (acceptThread_.joinable()) {
        acceptThread_.join();
    }

    WSACleanup();
}

uint16_t ControlApiServer::Port() const {
    return port_;
}

void ControlApiServer::AcceptLoop() {
    while (running_.load()) {
        const SocketHandle client = accept(static_cast<SocketHandle>(listenSocket_), nullptr, nullptr);
        if (client == INVALID_SOCKET) {
            if (!running_.load()) {
                return;
            }
            continue;
        }

        std::thread(&ControlApiServer::HandleClient, this, static_cast<uintptr_t>(client)).detach();
    }
}

void ControlApiServer::HandleClient(uintptr_t clientSocketValue) {
    const SocketHandle client = static_cast<SocketHandle>(clientSocketValue);

    std::string request;
    char buffer[4096];
    int received = 0;
    while ((received = recv(client, buffer, sizeof(buffer), 0)) > 0) {
        request.append(buffer, buffer + received);
        const auto headerEnd = request.find("\r\n\r\n");
        if (headerEnd == std::string::npos) {
            continue;
        }

        const auto contentLengthPos = request.find("Content-Length:");
        std::size_t expectedBodyLength = 0;
        if (contentLengthPos != std::string::npos) {
            const auto lineEnd = request.find("\r\n", contentLengthPos);
            expectedBodyLength = static_cast<std::size_t>(std::stoi(request.substr(contentLengthPos + 15, lineEnd - (contentLengthPos + 15))));
        }

        if (request.size() >= headerEnd + 4 + expectedBodyLength) {
            break;
        }
    }

    const auto requestLineEnd = request.find("\r\n");
    if (requestLineEnd == std::string::npos) {
        closesocket(client);
        return;
    }

    std::istringstream requestLine(request.substr(0, requestLineEnd));
    std::string method;
    std::string path;
    requestLine >> method >> path;
    path = NormalizePath(path);

    const auto bodyStart = request.find("\r\n\r\n");
    const std::string body = (bodyStart == std::string::npos) ? std::string() : request.substr(bodyStart + 4);

    HttpResponse response;

    if (method == "GET" && path == "/api/state") {
        response.body = RuntimeSnapshotToJson(runtime_.Snapshot()).dump();
    } else if (method == "GET" && path == "/api/config") {
        response.body = ConfigToJson(runtime_.CurrentConfig()).dump();
    } else if (method == "GET" && path == "/api/stats") {
        const auto snapshot = runtime_.Snapshot();
        response.body = json{
            { "ok", true },
            { "leftRateHz", snapshot.left.rateHz },
            { "rightRateHz", snapshot.right.rateHz },
            { "averageDispatchUs", snapshot.mouseStats.averageDispatchUs },
            { "maxDispatchUs", snapshot.mouseStats.maxDispatchUs }
        }.dump();
    } else if (method == "POST" && path == "/api/connect/left") {
        std::string error;
        const bool ok = runtime_.ConnectSide(JoyConSide::Left, error);
        response.body = ok ? json{ { "ok", true } }.dump() : json{ { "ok", false }, { "error", error } }.dump();
    } else if (method == "POST" && path == "/api/connect/right") {
        std::string error;
        const bool ok = runtime_.ConnectSide(JoyConSide::Right, error);
        response.body = ok ? json{ { "ok", true } }.dump() : json{ { "ok", false }, { "error", error } }.dump();
    } else if (method == "POST" && path == "/api/disconnect/left") {
        runtime_.DisconnectSide(JoyConSide::Left);
        response.body = json{ { "ok", true } }.dump();
    } else if (method == "POST" && path == "/api/disconnect/right") {
        runtime_.DisconnectSide(JoyConSide::Right);
        response.body = json{ { "ok", true } }.dump();
    } else if (method == "POST" && path == "/api/config/replace") {
        json requestBody;
        if (!TryParseJsonBody(body, response, requestBody)) {
            SendResponse(client, response);
            closesocket(client);
            return;
        }

        auto config = runtime_.CurrentConfig();
        UpdateConfigFromJson(requestBody, config);
        runtime_.ApplyConfig(config);
        response.body = json{ { "ok", true } }.dump();
    } else if (method == "POST" && path == "/api/config/save") {
        std::string error;
        const bool ok = configStore_.Save(runtime_.CurrentConfig(), error);
        response.body = ok ? json{ { "ok", true } }.dump() : json{ { "ok", false }, { "error", error } }.dump();
    } else if (method == "POST" && path == "/api/config/load") {
        auto config = runtime_.CurrentConfig();
        std::string error;
        const bool ok = configStore_.Load(config, error);
        if (ok) {
            runtime_.ApplyConfig(config);
            response.body = json{ { "ok", true } }.dump();
        } else {
            response.body = json{ { "ok", false }, { "error", error } }.dump();
        }
    } else if (method == "POST" && path == "/api/capture-key") {
        const CaptureKeyResult captured = CaptureKeyCustomToken(std::chrono::milliseconds(45000));
        if (captured.ok) {
            response.body = json{
                { "ok", true },
                { "token", captured.token },
                { "action", captured.action },
            }.dump();
        } else {
            response.body = json{ { "ok", false }, { "error", captured.error } }.dump();
        }
    } else {
        std::filesystem::path filePath = staticRoot_ / (path == "/" ? "index.html" : path.substr(1));
        response = ServeFile(filePath);
    }

    SendResponse(client, response);
    closesocket(client);
}

} // namespace joycon::webgui
