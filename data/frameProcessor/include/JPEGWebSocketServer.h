#ifndef MJPEG_WEBSOCKET_SERVER_H
#define MJPEG_WEBSOCKET_SERVER_H

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>
#include <boost/thread/thread.hpp>
#include <queue>
#include <mutex>
#include <memory>
#include <vector>
#include <opencv2/opencv.hpp>
#include <rapidjson/document.h>

namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;

namespace MJPEGUtils {
    cv::Mat applyColorEffect(const cv::Mat& frame, const std::string& color_effect);
}

class JPEGWebSocketServer {
public:
    JPEGWebSocketServer(net::io_context& ioc, tcp::endpoint endpoint, size_t buffer_size);
    void pushFrame(const uint16_t* frameData, size_t frameLength);
    std::vector<uint8_t> getFrame(int width, int height, const std::string& color_effect);
    std::vector<uint8_t> convertToMJPEG(const std::shared_ptr<std::vector<uint16_t>>& frame, int width, int height, const std::string& color_effect);

private:
    void startAccepting();

    tcp::acceptor acceptor_;
    size_t buffer_size_;
    std::queue<std::shared_ptr<std::vector<uint16_t>>> frame_buffer_;
    std::mutex buffer_mutex_;

    class Session : public std::enable_shared_from_this<Session> {
    public:
        Session(tcp::socket socket, JPEGWebSocketServer& server);
        void start();

    private:
        void readRequest();
        void onRequestRead(beast::error_code ec, std::size_t bytes_transferred);

        websocket::stream<beast::tcp_stream> websocket_;
        beast::flat_buffer buffer_;
        JPEGWebSocketServer& server_;
    };
};

#endif // MJPEG_WEBSOCKET_SERVER_H