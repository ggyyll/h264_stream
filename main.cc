#include <algorithm>
#include <assert.h>
#include <boost/circular_buffer.hpp>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>
#include "scoped_exit.hpp"
#include "h264.hpp"
#include "read_bits.hpp"
#include "h265_sps.hpp"

using CircularBytes = boost::circular_buffer<uint8_t>;
using Bytes = std::vector<uint8_t>;

static void pop_3_item(Bytes* buffer)
{
    assert(buffer->size() >= 3);
    buffer->pop_back();
    buffer->pop_back();
    buffer->pop_back();
}

static bool start_code_3_bytes(const CircularBytes& buf)
{
    if (buf.size() < 3)
    {
        return false;
    }
    return (buf[0] == 0x0 && buf[1] == 0x0 && buf[2] == 0x01);
}

static bool start_code_4_bytes(const CircularBytes& buf, uint8_t ch)
{
    if (buf.size() < 3)
    {
        return false;
    }
    return (buf[0] == 0x0 && buf[1] == 0x0 && buf[2] == 0x0 && ch == 0x01);
}
static bool ebsp_code(const CircularBytes& buf, uint8_t ch)
{
    if (buf.size() < 3)
    {
        return false;
    }
    bool prefix = false;
    bool prevent = false;
    if (buf[0] == 0x0 && buf[1] == 0x0 && buf[2] == 0x03)
    {
        prefix = true;
    }
    if (ch == 0x00 || ch == 0x01 || ch == 0x02 || ch == 0x03)
    {
        prevent = true;
    }
    return prefix && prevent;
}

struct NaluHeader
{
    int forbidden_bit;
    int nal_reference_bit;
    int nal_unit_type;
};

void parse_264_nalu_header(NaluHeader* h, uint8_t ch)
{
    h->forbidden_bit = ch & 0x80;
    h->nal_reference_bit = ch & 0x60;
    h->nal_reference_bit >>= 5;
    h->nal_unit_type = ch & 0x1f;
}

void parse_265_nalu_header(NaluHeader* h, const Bytes& buff)
{
    (void)h;
    ReadBit r(buff.data(), buff.size());
    h->forbidden_bit = r.read_bit();
    uint32_t type = r.read_n_bits(6);
    h->nal_unit_type = type;
    uint32_t layer_id = r.read_n_bits(6);
    uint32_t temporal_id_plus1 = r.read_n_bits(3);
    printf("type %u layer_id  %u temporal_id_plus1 %u\n", type, layer_id, temporal_id_plus1);
}

void log_nalu_header(const NaluHeader& h)
{
    printf("forbidden %d reference %d type %d\n",
           h.forbidden_bit,
           h.nal_reference_bit,
           h.nal_unit_type);
}

void rbsp_to_sodb(const Bytes& rbsp, Bytes* sodb)
{
    size_t last_byte_pos = rbsp.size();
    int bit_offset = 0;
    while (true)
    {
        if (rbsp[last_byte_pos] & (0x01 << bit_offset))
        {
            break;
        }
        bit_offset++;
        if (bit_offset == 8)
        {
            if (last_byte_pos == 1)
            {
                printf("failed zero data\n");
                return;
            }
            last_byte_pos--;
            bit_offset = 0;
        }
    }
    sodb->insert(sodb->end(), rbsp.begin(), rbsp.begin() + last_byte_pos);
}

void ebsp_to_rbsp(const Bytes& ebsp, Bytes* rbsp)
{
    CircularBytes bytes(3);
    for (const auto& ch : ebsp)
    {
        if (ebsp_code(bytes, ch))
        {
            bytes.clear();
            rbsp->pop_back();
        }
        bytes.push_back(ch);
        rbsp->push_back(ch);
    }
}

bool find_first_start_code(FILE* fp)
{
    CircularBytes bytes(3);
    while (true)
    {
        if (start_code_3_bytes(bytes))
        {
            break;
        }
        else
        {
            if (feof(fp))
            {
                return false;
            }
            uint8_t ch = getc(fp);
            if (start_code_4_bytes(bytes, ch))
            {
                break;
            }
            bytes.push_back(ch);
        }
    }
    return true;
}

void show_bytes(const Bytes& bytes, const std::string& msg)
{
    printf("---------------------%s---------------------\n", msg.data());
    for (const auto& byte : bytes)
    {
        printf("%x ", byte);
    }
    printf("\n");
}

void process_nal_payload(const Bytes& buff)
{
    if (buff.empty())
    {
        return;
    }
    NaluHeader h;
    parse_264_nalu_header(&h, buff[0]);
    log_nalu_header(h);
    if (h.nal_unit_type != 7 && h.nal_unit_type != 8)
    {
        return;
    }
    std::string s("sps ");
    if (h.nal_unit_type == 8)
    {
        s = "pps ";
    }
    Bytes ebsp = buff;
    Bytes rbsp;
    Bytes sodb;
    ebsp.erase(ebsp.begin());
    ebsp_to_rbsp(ebsp, &rbsp);
    if (rbsp.empty())
    {
        return;
    }
    rbsp_to_sodb(rbsp, &sodb);
    if (sodb.empty())
    {
        return;
    }
    show_bytes(ebsp, s + "ebsp");
    show_bytes(rbsp, s + "rbsp");
    show_bytes(sodb, s + "sodb");
    if (h.nal_unit_type == 7)
    {
        int64_t width = 0;
        int64_t height = 0;
        h264_width_height(ebsp.data(), ebsp.size(), &width, &height);
        printf("h264 stream %ldx%ld\n", width, height);
    }
}

void find_nal_payload(FILE* fp, Bytes* buffer)
{
    CircularBytes bytes(3);
    buffer->clear();
    while (feof(fp) == false)
    {
        if (start_code_3_bytes(bytes))
        {
            pop_3_item(buffer);
            break;
        }
        else
        {
            uint8_t ch = getc(fp);
            if (start_code_4_bytes(bytes, ch))
            {
                pop_3_item(buffer);
                break;
            }
            bytes.push_back(ch);
            buffer->push_back(ch);
        }
    }
}

void process_h265_nal_payload(const Bytes& buff)
{
    if (buff.size() < 2)
    {
        return;
    }
    Bytes ebsp = buff;
    h265_nal_t nal;
    memset(&nal, 0, sizeof nal);

    bs_t* b = bs_new(ebsp.data(), ebsp.size());
    // nal header
    nal.forbidden_zero_bit = bs_read_f(b, 1);
    nal.nal_unit_type = bs_read_u(b, 6);
    nal.nuh_layer_id = bs_read_u(b, 6);
    nal.nuh_temporal_id_plus1 = bs_read_u(b, 3);
    nal.parsed = NULL;
    nal.sizeof_parsed = 0;
    bs_free(b);

    int nal_size = ebsp.size();
    int rbsp_size = ebsp.size();
    uint8_t* rbsp_buf = (uint8_t*)malloc(rbsp_size);

    int rc = nal_to_rbsp(2, buff.data(), &nal_size, rbsp_buf, &rbsp_size);

    if (rc < 0)
    {
        free(rbsp_buf);
        return;
    }  // handle conversion error
    if (nal.nal_unit_type != 33)
    {
        return;
    }
    b = bs_new(rbsp_buf, rbsp_size);
    h265_sps_t sps;
    h265_read_sps_rbsp(&sps, b);
    printf("%ldx%ld\n", sps.width, sps.height);

}
void parse_h265_file(const std::string& filename)
{
    FILE* fp = fopen(filename.data(), "rb");
    if (!fp)
    {
        return;
    }
    auto file_close = make_scoped_exit([&fp]() { fclose(fp); });

    if (find_first_start_code(fp) == false)
    {
        printf("invalid h264 stream : not found first start code\n");
        return;
    }
    Bytes buffer;
    while (feof(fp) == false)
    {
        find_nal_payload(fp, &buffer);
        process_h265_nal_payload(buffer);
        buffer.clear();
    }
    printf("file eof\n");
}
void parse_h264_file(const std::string& filename)
{
    FILE* fp = fopen(filename.data(), "rb");
    if (!fp)
    {
        return;
    }
    auto file_close = make_scoped_exit([&fp]() { fclose(fp); });

    if (find_first_start_code(fp) == false)
    {
        printf("invalid h264 stream : not found first start code\n");
        return;
    }
    Bytes buffer;
    while (feof(fp) == false)
    {
        find_nal_payload(fp, &buffer);
        process_nal_payload(buffer);
        buffer.clear();
    }
    printf("file eof\n");
}

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        printf("%s input filename\n", argv[0]);
        exit(0);
    }
    // parse_h264_file(argv[1]);
    parse_h265_file(argv[1]);
    return 0;
}
