#ifndef SIMULATED_IMAGE_GENERATOR_H
#define SIMULATED_IMAGE_GENERATOR_H

#include <vector>
#include <cstdint>
#include <string>

enum class ImageFormat { MONO, COLOR };

class SimulatedImageGenerator {
public:
    SimulatedImageGenerator(int width, int height, int bitDepth, ImageFormat format);
    
    void loadImagesFromFile(const std::string& filePath);
    void generateImage(int cameraNumber, uint64_t frameNumber);
    const std::vector<uint16_t>& getImageData() const;
    size_t getFrameCount() const;

private:
    static const uint8_t Font40_Table[];
    int width_;
    int height_;
    int bitDepth_;
    ImageFormat format_;
    std::vector<std::vector<uint16_t>> loaded_frames_;
    std::vector<uint16_t> current_frame_;
    size_t current_frame_index_;

    void drawPixel(int x, int y, uint16_t value);
    void drawChar(char c, int x, int y, uint16_t value, int scale);
    void drawText(const std::string& text, int x, int y, uint16_t value, int scale);
};

#endif // SIMULATED_IMAGE_GENERATOR_H