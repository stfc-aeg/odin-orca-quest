#include "MjpegStreamer.h"
#include <iostream>

MjpegStreamer::MjpegStreamer(boost::asio::io_service& io_service, unsigned int width, unsigned int height, int delay=50)
    : io_service_(io_service), acceptor_(io_service_), width_(width), height_(height), is_running_(false), delay_(delay) {}

MjpegStreamer::~MjpegStreamer() {
    stop();
}

void MjpegStreamer::start(unsigned short port) {
    if (!is_running_) {
        boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::tcp::v4(), port);
        acceptor_.open(endpoint.protocol());
        acceptor_.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
        acceptor_.bind(endpoint);
        acceptor_.listen();
        do_accept();
        is_running_ = true;
    }
}

void MjpegStreamer::stop() {
    if (is_running_) {
        acceptor_.close();
        is_running_ = false;
    }
}

void MjpegStreamer::push(const uint16_t* frame_data, size_t frame_size) {
    cv::Mat frame_16bit(height_, width_, CV_16UC1, const_cast<uint16_t*>(frame_data));
    cv::Mat frame_8bit;
    frame_16bit.convertTo(frame_8bit, CV_8U, 1.0 / 256.0);
    std::vector<unsigned char> encoded_frame;
    cv::imencode(".jpg", frame_8bit, encoded_frame);
    std::lock_guard<std::mutex> lock(frame_mutex_);
    frame_buffer_.push(std::move(encoded_frame));
    std::cout << frame_buffer_.size() << " frames in queue" << std::endl;
}

void MjpegStreamer::do_accept() {
    auto socket = std::make_shared<boost::asio::ip::tcp::socket>(io_service_);
    acceptor_.async_accept(*socket, [this, socket](const boost::system::error_code& ec) {
        if (!ec) {
            async_write_response_headers(socket);
        }
        do_accept();
    });
}

void MjpegStreamer::async_write_response_headers(const std::shared_ptr<boost::asio::ip::tcp::socket>& socket) {
    static std::string response_headers =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: multipart/x-mixed-replace; boundary=--boundarydonotcross\r\n"
        "Cache-Control: no-cache\r\n"
        "Connection: close\r\n\r\n";

    boost::asio::async_write(*socket, boost::asio::buffer(response_headers),
        [this, socket](const boost::system::error_code& ec, std::size_t) {
            if (!ec) {
                async_write_frame(socket);
            }
        });
}

void MjpegStreamer::async_write_frame(const std::shared_ptr<boost::asio::ip::tcp::socket>& socket) {
    std::vector<unsigned char> encoded_frame;
    {
        std::lock_guard<std::mutex> lock(frame_mutex_);
        if (!frame_buffer_.empty()) {
            encoded_frame = std::move(frame_buffer_.front());
            while(!frame_buffer_.empty())
            {
                frame_buffer_.pop();
            }
        }
    }

    if (!encoded_frame.empty()) {
        std::ostringstream frame_stream;
        frame_stream << "--boundarydonotcross\r\n"
                     << "Content-Type: image/jpeg\r\n"
                     << "Content-Length: " << encoded_frame.size() << "\r\n\r\n";
        std::string frame_header = frame_stream.str();

        std::vector<boost::asio::const_buffer> buffers;
        buffers.push_back(boost::asio::buffer(frame_header));
        buffers.push_back(boost::asio::buffer(encoded_frame));

        boost::asio::async_write(*socket, buffers,
            [this, socket](const boost::system::error_code& ec, std::size_t) {
                if (!ec) {
                    // Add a short delay before sending the next frame
                    std::this_thread::sleep_for(std::chrono::milliseconds(delay_));
                    async_write_frame(socket);
                }
            });
    } else {
        std::cout << "No frame to send" << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(delay_));
        async_write_frame(socket);
    }
}