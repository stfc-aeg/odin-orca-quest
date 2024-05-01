#ifndef MJPEG_STREAMER_H
#define MJPEG_STREAMER_H

#include <cstdint>
#include <vector>
#include <queue>
#include <mutex>
#include <boost/asio.hpp>
#include <opencv2/opencv.hpp>

class MjpegStreamer : public std::enable_shared_from_this<MjpegStreamer> {
public:
    MjpegStreamer(boost::asio::io_service& io_service, unsigned int width, unsigned int height, int delay);
    ~MjpegStreamer();

    void start(unsigned short port);
    void stop();
    void push(const uint16_t* frame_data, size_t frame_size);

private:
    void do_accept();
    void async_write_response_headers(const std::shared_ptr<boost::asio::ip::tcp::socket>& socket);
    void async_write_frame(const std::shared_ptr<boost::asio::ip::tcp::socket>& socket);

    boost::asio::io_service& io_service_;
    boost::asio::ip::tcp::acceptor acceptor_;
    std::mutex frame_mutex_;
    std::queue<std::vector<unsigned char>> frame_buffer_;
    unsigned int width_;
    unsigned int height_;
    bool is_running_;

    int delay_;
};

#endif // MJPEG_STREAMER_H