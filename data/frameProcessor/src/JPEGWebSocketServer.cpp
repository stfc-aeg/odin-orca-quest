#include "JPEGWebSocketServer.h"
#include <iostream>
#include <sstream>
#include <string>
#include <rapidjson/document.h>

namespace JPEGUtils {
    cv::Mat applyColorEffect(const cv::Mat& frame, const std::string& color_effect) {
        cv::Mat color_frame;
        try {
            if (color_effect == "autumn") {
                cv::applyColorMap(frame, color_frame, cv::COLORMAP_AUTUMN);
            } else if (color_effect == "bone") {
                cv::applyColorMap(frame, color_frame, cv::COLORMAP_BONE);
            } else if (color_effect == "jet") {
                cv::applyColorMap(frame, color_frame, cv::COLORMAP_JET);
            } else if (color_effect == "winter") {
                cv::applyColorMap(frame, color_frame, cv::COLORMAP_WINTER);
            } else if (color_effect == "rainbow") {
                cv::applyColorMap(frame, color_frame, cv::COLORMAP_RAINBOW);
            } else if (color_effect == "ocean") {
                cv::applyColorMap(frame, color_frame, cv::COLORMAP_OCEAN);
            } else if (color_effect == "summer") {
                cv::applyColorMap(frame, color_frame, cv::COLORMAP_SUMMER);
            } else if (color_effect == "spring") {
                cv::applyColorMap(frame, color_frame, cv::COLORMAP_SPRING);
            } else if (color_effect == "cool") {
                cv::applyColorMap(frame, color_frame, cv::COLORMAP_COOL);
            } else if (color_effect == "hsv") {
                cv::applyColorMap(frame, color_frame, cv::COLORMAP_HSV);
            } else if (color_effect == "pink") {
                cv::applyColorMap(frame, color_frame, cv::COLORMAP_PINK);
            } else if (color_effect == "hot") {
                cv::applyColorMap(frame, color_frame, cv::COLORMAP_HOT);
            } else if (color_effect == "parula") {
                cv::applyColorMap(frame, color_frame, cv::COLORMAP_PARULA);
            } else if (color_effect == "magma") {
                cv::applyColorMap(frame, color_frame, cv::COLORMAP_MAGMA);
            } else if (color_effect == "inferno") {
                cv::applyColorMap(frame, color_frame, cv::COLORMAP_INFERNO);
            } else if (color_effect == "plasma") {
                cv::applyColorMap(frame, color_frame, cv::COLORMAP_PLASMA);
            } else if (color_effect == "viridis") {
                cv::applyColorMap(frame, color_frame, cv::COLORMAP_VIRIDIS);
            } else if (color_effect == "cividis") {
                cv::applyColorMap(frame, color_frame, cv::COLORMAP_CIVIDIS);
            } else if (color_effect == "twilight") {
                cv::applyColorMap(frame, color_frame, cv::COLORMAP_TWILIGHT);
            } else if (color_effect == "twilight_shifted") {
                cv::applyColorMap(frame, color_frame, cv::COLORMAP_TWILIGHT_SHIFTED);
            } else if (color_effect == "turbo") {
                cv::applyColorMap(frame, color_frame, cv::COLORMAP_TURBO);
            } else if (color_effect == "deepgreen") {
                cv::applyColorMap(frame, color_frame, cv::COLORMAP_DEEPGREEN);
            } else {
                color_frame = frame;
                std::cout << "Unknown color effect. Defaulting to grayscale." << std::endl;
            }
        } catch (const cv::Exception& e) {
            std::cerr << "OpenCV exception: " << e.what() << std::endl;
            // Handle the exception or return an error
        }
        return color_frame;
    }
}

JPEGWebSocketServer::JPEGWebSocketServer(net::io_context& ioc, tcp::endpoint endpoint, size_t buffer_size)
    : acceptor_(ioc, endpoint), buffer_size_(buffer_size) {
    startAccepting();
}

// Function to push new frames to the buffer
// If buffer is full, then removed frame and add new

void JPEGWebSocketServer::pushFrame(const uint16_t* frameData, size_t frameLength) {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    
    if (frame_buffer_.size() >= buffer_size_) {
        frame_buffer_.pop();
    }
    
    std::shared_ptr<std::vector<uint16_t>> copiedFrameData = std::make_shared<std::vector<uint16_t>>(frameData, frameData + frameLength);
    frame_buffer_.emplace(copiedFrameData);
    
    //std::cout << "Pushed frame to buffer. Buffer size: " << frame_buffer_.size() << std::endl;
}


// Get a frame from the buffer
// Convert to JPEG and apply effects

std::vector<uint8_t> JPEGWebSocketServer::getFrame(int width, int height, const std::string& color_effect) {
    std::shared_ptr<std::vector<uint16_t>> frame_data;
    {
        std::lock_guard<std::mutex> lock(buffer_mutex_);
        if (!frame_buffer_.empty()) {
            frame_data = std::move(frame_buffer_.front());
            frame_buffer_.pop();
            std::cout << "Retrieved frame from buffer. Buffer size: " << frame_buffer_.size() << std::endl;
        } else {
            std::cout << "Buffer is empty. Skipping frame retrieval." << std::endl;
            return std::vector<uint8_t>();
        }
    }

    return convertToMJPEG(frame_data, width, height, color_effect);
}

void JPEGWebSocketServer::startAccepting() {
    acceptor_.async_accept([this](beast::error_code ec, tcp::socket socket) {
        if (!ec) {
            std::make_shared<Session>(std::move(socket), *this)->start();
        }
        startAccepting();
    });
}

JPEGWebSocketServer::Session::Session(tcp::socket socket, JPEGWebSocketServer& server)
    : websocket_(std::move(socket)), server_(server) {}

void JPEGWebSocketServer::Session::start() {
    websocket_.set_option(websocket::stream_base::timeout::suggested(beast::role_type::server));
    websocket_.set_option(websocket::stream_base::decorator([](websocket::response_type& res) {
        res.set(http::field::server, std::string(BOOST_BEAST_VERSION_STRING) + " websocket-server-async");
    }));

    websocket_.async_accept([self = shared_from_this()](beast::error_code ec) {
        if (!ec) {
            self->readRequest();
        }
    });
}

// Handle any requests, forwarding them to the handler
void JPEGWebSocketServer::Session::readRequest() {
    websocket_.async_read(buffer_, [self = shared_from_this()](beast::error_code ec, std::size_t bytes_transferred) {
        self->onRequestRead(ec, bytes_transferred);
    });
}

// Handle image requests
// This function decodes the JSON
void JPEGWebSocketServer::Session::onRequestRead(beast::error_code ec, std::size_t bytes_transferred) {
    if (!ec) {
        std::string data = beast::buffers_to_string(buffer_.data());
        buffer_.consume(buffer_.size());

        rapidjson::Document doc;
        doc.Parse(data.c_str());

        if (!doc.HasParseError() && doc.IsObject()) {
            int width = doc.HasMember("width") && doc["width"].IsInt() ? doc["width"].GetInt() : 640;
            int height = doc.HasMember("height") && doc["height"].IsInt() ? doc["height"].GetInt() : 480;
            std::string color_effect = doc.HasMember("color_effect") && doc["color_effect"].IsString() ? doc["color_effect"].GetString() : "grayscale";

            std::cout << "Received JSON request: width=" << width << ", height=" << height << ", color_effect=" << color_effect << std::endl;

            try {
                std::vector<uint8_t> jpeg_frame = server_.getFrame(width, height, color_effect);
                if (!jpeg_frame.empty()) {
                    websocket_.binary(true);
                    websocket_.write(net::buffer(jpeg_frame));
                    std::cout << "Sent frame to client." << std::endl;
                } else {
                    std::cout << "No frame available to send." << std::endl;
                }
            } catch (const std::exception& e) {
                std::cerr << "Exception occurred while processing request: " << e.what() << std::endl;
                // Handle the exception or send an error response to the client
            }
        } else {
            std::cout << "Invalid JSON request." << std::endl;
        }

        readRequest();
    } else if (ec == websocket::error::closed) {
        std::cout << "WebSocket connection closed by the client." << std::endl;
    } else {
        std::cerr << "Read error: " << ec.message() << std::endl;
        websocket_.close(websocket::close_code::normal);
    }
}

std::vector<uint8_t> JPEGWebSocketServer::convertToMJPEG(const std::shared_ptr<std::vector<uint16_t>>& frame, int width, int height, const std::string& color_effect) {
    if (!frame || frame->empty()) {
        std::cerr << "Frame data is empty." << std::endl;
        return std::vector<uint8_t>();
    }

    cv::Mat mat;
    try {
        mat = cv::Mat(2304, 4096, CV_16UC1, frame->data());
        printf("Frame pointer: %p", frame->data());
    } catch (const cv::Exception& e) {
        std::cerr << "OpenCV exception: " << e.what() << std::endl;
        return std::vector<uint8_t>();
    }

    if (mat.empty()) {
        std::cerr << "OpenCV matrix is empty after creation. Check frame size and data integrity." << std::endl;
        return std::vector<uint8_t>();
    }



    cv::Mat resized_frame;
    cv::resize(mat, resized_frame, cv::Size(width, height), 0, 0, cv::INTER_LINEAR);

    cv::Mat resized_frame_8bit;
    resized_frame.convertTo(resized_frame_8bit, CV_8U, 1.0 / 256.0);

    cv::Mat color_frame = JPEGUtils::applyColorEffect(resized_frame_8bit, color_effect);

    if (color_frame.empty()) {
        std::cerr << "OpenCV matrix is empty after color effect application." << std::endl;
        return std::vector<uint8_t>();
    }

    std::vector<uint8_t> jpeg_frame;
    std::vector<int> params = {cv::IMWRITE_JPEG_QUALITY, 90};
    if (!cv::imencode(".jpg", color_frame, jpeg_frame, params)) {
        std::cerr << "Failed to encode the image to JPEG format." << std::endl;
    }

    return jpeg_frame;
}