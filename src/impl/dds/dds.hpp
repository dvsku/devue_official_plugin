#pragma once

#include <vector>
#include <string>

namespace devue::plugins {
    class dds {
    public:
        uint32_t width      = 0U;
        uint32_t height     = 0U;
        uint32_t depth      = 0U;
        uint32_t components = 4U;

        std::vector<uint8_t> pixels;

    public:
        bool parse(const std::string& file, bool flip = false);
        bool parse(const void* src, const size_t size, bool flip = false);

        const std::string& get_error();

    private:
        std::string m_error_msg = "";

    private:
        bool parse_dxt5(uint8_t* src);

        void decompress_dxt5_block(uint32_t x, uint32_t y, uint32_t width, const uint8_t* src);

        void flip_vertically();
    };
}