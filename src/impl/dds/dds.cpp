#include "dds.hpp"

#include <fstream>
#include <algorithm>

using namespace devue::plugins;

///////////////////////////////////////////////////////////////////////////////
// INTERNAL

#define FOUR_CC(str) (uint32_t)(((str)[3] << 24U) | ((str)[2] << 16U) | ((str)[1] << 8U) | (str)[0])

struct pixel_format {
    uint32_t size;
    uint32_t flags;
    uint32_t four_cc;
    uint32_t rgb_bit_count;
    uint32_t r_bit_mask;
    uint32_t g_bit_mask;
    uint32_t b_bit_mask;
    uint32_t a_bit_mask;
};

struct basic_header {
    uint32_t     size;
    uint32_t     flags;
    uint32_t     height;
    uint32_t     width;
    uint32_t     pitch_linear_size;
    uint32_t     depth;
    uint32_t     mipmap_count;
    uint32_t     reserved_1[11];
    pixel_format pixel_format;
    uint32_t     caps_1;
    uint32_t     caps_2;
    uint32_t     caps_3;
    uint32_t     caps_4;
    uint32_t     reserved_2;
};

struct dxt10_header {
    uint32_t surface_pixel_format;
    uint32_t resource_type;
    uint32_t misc_flags_1;
    uint32_t size;
    uint32_t misc_flags_2;
};

// basic_header flags
enum {
    DDSD_CAPS        = 0x1,
    DDSD_HEIGHT      = 0x2,
    DDSD_WIDTH       = 0x4,
    DDSD_PITCH       = 0x8,
    DDSD_PIXELFORMAT = 0x1000,
    DDSD_MIPMAPCOUNT = 0x20000,
    DDSD_LINEARSIZE  = 0x80000,
    DDSD_DEPTH       = 0x800000
};

// basic_header caps_1
enum {
    DDSCAPS_COMPLEX = 0x8,
    DDSCAPS_MIPMAP  = 0x400000,
    DDSCAPS_TEXTURE = 0x1000
};

// pixel_format flags
enum {
    DDPF_ALPHAPIXELS = 0x1,
    DDPF_ALPHA       = 0x2,
    DDPF_FOURCC      = 0x4,
    DDPF_RGB         = 0x40,
    DDPF_YUV         = 0x200,
    DDPF_LUMINANCE   = 0x20000
};

///////////////////////////////////////////////////////////////////////////////
// PUBLIC

bool dds::parse(const std::string& file, bool flip) {
    std::vector<uint8_t> file_bytes;
    std::ifstream        file_stream(file, std::ios::binary);

    if (!file_stream.is_open()) {
        m_error_msg = "Failed to open file.";
        return false;
    }

    file_stream.seekg(0, std::ios::end);
    std::streamsize file_size = file_stream.tellg();
    file_stream.seekg(0, std::ios::beg);

    file_bytes.resize(file_size);

    file_stream.read(reinterpret_cast<char*>(file_bytes.data()), file_size);
    file_stream.close();

    return parse(file_bytes.data(), file_bytes.size(), flip);
}

bool dds::parse(const void* src, const size_t size, bool flip) {
    uint8_t* source = (uint8_t*)src;

    // Minimum file size
    if (size < 128) {
        return false;
    }

    // Read magic
    uint32_t magic = 0U;
    memcpy(&magic, source, sizeof(magic));
    source += sizeof(magic);

    // Check if magic is "DDS "
    if (magic != 0x20534444) {
        m_error_msg = "Not a DDS file.";
        return false;
    }

    basic_header header{};
    dxt10_header header_dxt10{};

    memcpy(&header, source, sizeof(basic_header));
    source += sizeof(basic_header);

    if (header.size != 124) {
        return false;
    }

    if (!(header.flags & DDSD_CAPS)) {
        return false;
    }

    if (!(header.flags & DDSD_WIDTH) || !(header.flags & DDSD_HEIGHT)) {
        return false;
    }

    if (!(header.flags & DDSD_PIXELFORMAT)) {
        return false;
    }

    if (!(header.caps_1 & DDSCAPS_TEXTURE)) {
        return false;
    }

    if ((header.pixel_format.flags & DDPF_FOURCC) && (header.pixel_format.four_cc == FOUR_CC("DX10"))) {
        memcpy(&header_dxt10, source, sizeof(dxt10_header));
        source += sizeof(dxt10_header);
    }

    width  = std::max(1U, header.width);
    height = std::max(1U, header.height);
    depth  = std::max(1U, header.depth);

    pixels.resize((size_t)width * height * depth * components);

    bool success = false;

    // Compressed
    if (header.pixel_format.flags & DDPF_FOURCC) {
        switch (header.pixel_format.four_cc) {
            case FOUR_CC("DXT5"): success = parse_dxt5(source); break;

            default: break;
        }
    }
    // Not compressed
    else {

    }

    if (success && flip)
        flip_vertically();

    return success;
}

const std::string& dds::get_error() {
    return m_error_msg;
}

bool dds::parse_dxt5(uint8_t* src) {
    uint32_t block_count_x = (width  + 3) / components;
    uint32_t block_count_y = (height + 3) / components;
    uint32_t block_width   = (width  < 4) ? width  : components;
    uint32_t block_height  = (height < 4) ? height : components;

    for (uint32_t i = 0; i < block_count_y; i++) {
        for (uint32_t j = 0; j < block_count_x; j++) {
            decompress_dxt5_block(j * 4, i * 4, width, src + j * 16);
        }

        src += block_count_x * 16;
    }

    return true;
}

void dds::decompress_dxt5_block(uint32_t x, uint32_t y, uint32_t width, const uint8_t* src) {
    uint8_t alpha_0 = *reinterpret_cast<const uint8_t*>(src);
    uint8_t alpha_1 = *reinterpret_cast<const uint8_t*>(src + 1);

    const uint8_t* bits         = src + 2;
    int32_t        alpha_code_1 = bits[2] | (bits[3] << 8) | (bits[4] << 16) | (bits[5] << 24);
    uint16_t       alpha_code_2 = bits[0] | (bits[1] << 8);

    uint16_t color_0 = *reinterpret_cast<const uint16_t*>(src + 8);
    uint16_t color_1 = *reinterpret_cast<const uint16_t*>(src + 10);

    int32_t temp;

            temp = (color_0 >> 11) * 255 + 16;
    uint8_t r0   = (uint8_t)((temp / 32 + temp) / 32);
            temp = ((color_0 & 0x07E0) >> 5) * 255 + 32;
    uint8_t g0   = (uint8_t)((temp / 64 + temp) / 64);
            temp = (color_0 & 0x001F) * 255 + 16;
    uint8_t b0   = (uint8_t)((temp / 32 + temp) / 32);

            temp = (color_1 >> 11) * 255 + 16;
    uint8_t r1   = (uint8_t)((temp / 32 + temp) / 32);
            temp = ((color_1 & 0x07E0) >> 5) * 255 + 32;
    uint8_t g1   = (uint8_t)((temp / 64 + temp) / 64);
            temp = (color_1 & 0x001F) * 255 + 16;
    uint8_t b1   = (uint8_t)((temp / 32 + temp) / 32);

    int32_t code = *reinterpret_cast<const int32_t*>(src + 12);

    for (int32_t j = 0; j < 4; j++) {
        for (int32_t i = 0; i < 4; i++) {
            int32_t alpha_code_index = 3 * (4 * j + i);
            int32_t alpha_code       = 0;

            if (alpha_code_index <= 12) {
                alpha_code = (alpha_code_2 >> alpha_code_index) & 0x07;
            }
            else if (alpha_code_index == 15) {
                alpha_code = (alpha_code_2 >> 15) | ((alpha_code_1 << 1) & 0x06);
            }
            else {
                alpha_code = (alpha_code_1 >> (alpha_code_index - 16)) & 0x07;
            }

            uint8_t final_alpha;
            if (alpha_code == 0) {
                final_alpha = alpha_0;
            }
            else if (alpha_code == 1) {
                final_alpha = alpha_1;
            }
            else {
                if (alpha_0 > alpha_1) {
                    final_alpha = ((8 - alpha_code) * alpha_0 + (alpha_code - 1) * alpha_1) / 7;
                }
                else {
                    if (alpha_code == 6)
                        final_alpha = 0;
                    else if (alpha_code == 7)
                        final_alpha = 255;
                    else
                        final_alpha = ((6 - alpha_code) * alpha_0 + (alpha_code - 1) * alpha_1) / 5;
                }
            }

            uint8_t color_code = (code >> 2 * (4 * j + i)) & 0x03;
            uint8_t r          = 0U;
            uint8_t g          = 0U;
            uint8_t b          = 0U;

            switch (color_code) {
                case 0:
                    r = r0; 
                    g = g0; 
                    b = b0; 
                    break;
                case 1:
                    r = r1; 
                    g = g1; 
                    b = b1;
                    break;
                case 2:
                    r = (2 * r0 + r1) / 3;
                    g = (2 * g0 + g1) / 3;
                    b = (2 * b0 + b1) / 3;
                    break;
                case 3:
                    r = (r0 + 2 * r1) / 3;
                    g = (g0 + 2 * g1) / 3;
                    b = (b0 + 2 * b1) / 3;
                    break;
            }

            if (x + j < width) {
                size_t index = (((size_t)y + j) * width + ((size_t)x + i)) * components;
                pixels[index + 0] = r;
                pixels[index + 1] = g;
                pixels[index + 2] = b;
                pixels[index + 3] = final_alpha;
            }
        }
    }
}

void dds::flip_vertically() {
    size_t row_size = width * sizeof(uint8_t) * components;

    for (uint32_t i = 0; i < height / 2; i++) {
        size_t row_index_1 = i * row_size;
        size_t row_index_2 = ((size_t)height - 1 - i) * row_size;

        std::swap_ranges(pixels.begin() + row_index_1, pixels.begin() + row_index_1 + row_size, pixels.begin() + row_index_2);
    }
}
