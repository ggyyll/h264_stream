#ifndef __H264_HPP__
#define __H264_HPP__

#include <assert.h>
#include <stdint.h>
#include "noncopyable.hpp"

class H264Parse
{
#define UNUSE(x) (void)(x)
private:
    unsigned int read_bit()
    {
        assert(current_bit_ <= length_ * 8);
        int index = current_bit_ / 8;
        int offset = current_bit_ % 8 + 1;

        current_bit_++;
        return (start_[index] >> (8 - offset)) & 0x01;
    }
    unsigned int read_n_bits(int n)
    {
        int r = 0;
        int i;
        for (i = 0; i < n; i++)
        {
            r |= (read_bit() << (n - i - 1));
        }
        return r;
    }

    unsigned int read_exponential_golomb_code()
    {
        int r = 0;
        int i = 0;

        while ((read_bit() == 0) && (i < 32))
        {
            i++;
        }

        r = read_n_bits(i);
        r += (1 << i) - 1;
        return r;
    }

    unsigned int read_se()
    {
        int r = read_exponential_golomb_code();
        if (r & 0x01)
        {
            r = (r + 1) / 2;
        }
        else
        {
            r = -(r / 2);
        }
        return r;
    }

public:
    H264Parse(const uint8_t *start, uint64_t len)
        : start_(start)
        , length_(len)
        , current_bit_(0) {};

public:
    void h264_width_height(int64_t *width, int64_t *height)
    {

        int frame_crop_left_offset = 0;
        int frame_crop_right_offset = 0;
        int frame_crop_top_offset = 0;
        int frame_crop_bottom_offset = 0;

        int profile_idc = read_n_bits(8);
        UNUSE(profile_idc);
        int constraint_set0_flag = read_bit();
        UNUSE(constraint_set0_flag);
        int constraint_set1_flag = read_bit();
        UNUSE(constraint_set1_flag);
        int constraint_set2_flag = read_bit();
        UNUSE(constraint_set2_flag);
        int constraint_set3_flag = read_bit();
        UNUSE(constraint_set3_flag);
        int constraint_set4_flag = read_bit();
        UNUSE(constraint_set4_flag);
        int constraint_set5_flag = read_bit();
        UNUSE(constraint_set5_flag);
        int reserved_zero_2bits = read_n_bits(2);
        UNUSE(reserved_zero_2bits);
        int level_idc = read_n_bits(8);
        UNUSE(level_idc);
        int seq_parameter_set_id = read_exponential_golomb_code();
        UNUSE(seq_parameter_set_id);

        if (profile_idc == 100 || profile_idc == 110 || profile_idc == 122 || profile_idc == 244
            || profile_idc == 44 || profile_idc == 83 || profile_idc == 86 || profile_idc == 118)
        {
            int chroma_format_idc = read_exponential_golomb_code();

            if (chroma_format_idc == 3)
            {
                int residual_colour_transform_flag = read_bit();
                UNUSE(residual_colour_transform_flag);
            }
            int bit_depth_luma_minus8 = read_exponential_golomb_code();
            UNUSE(bit_depth_luma_minus8);
            int bit_depth_chroma_minus8 = read_exponential_golomb_code();
            UNUSE(bit_depth_chroma_minus8);
            int qpprime_y_zero_transform_bypass_flag = read_bit();
            UNUSE(qpprime_y_zero_transform_bypass_flag);
            int seq_scaling_matrix_present_flag = read_bit();
            UNUSE(seq_scaling_matrix_present_flag);

            if (seq_scaling_matrix_present_flag)
            {
                int i = 0;
                for (i = 0; i < 8; i++)
                {
                    int seq_scaling_list_present_flag = read_bit();
                    if (seq_scaling_list_present_flag)
                    {
                        int sizeOfScalingList = (i < 6) ? 16 : 64;
                        int lastScale = 8;
                        int nextScale = 8;
                        int j = 0;
                        for (j = 0; j < sizeOfScalingList; j++)
                        {
                            if (nextScale != 0)
                            {
                                int delta_scale = read_se();
                                nextScale = (lastScale + delta_scale + 256) % 256;
                            }
                            lastScale = (nextScale == 0) ? lastScale : nextScale;
                        }
                    }
                }
            }
        }

        int log2_max_frame_num_minus4 = read_exponential_golomb_code();
        UNUSE(log2_max_frame_num_minus4);
        int pic_order_cnt_type = read_exponential_golomb_code();
        UNUSE(pic_order_cnt_type);
        if (pic_order_cnt_type == 0)
        {
            int log2_max_pic_order_cnt_lsb_minus4 = read_exponential_golomb_code();
            UNUSE(log2_max_pic_order_cnt_lsb_minus4);
        }
        else if (pic_order_cnt_type == 1)
        {
            int delta_pic_order_always_zero_flag = read_bit();
            UNUSE(delta_pic_order_always_zero_flag);
            int offset_for_non_ref_pic = read_se();
            UNUSE(offset_for_non_ref_pic);
            int offset_for_top_to_bottom_field = read_se();
            UNUSE(offset_for_top_to_bottom_field);
            int num_ref_frames_in_pic_order_cnt_cycle = read_exponential_golomb_code();
            for (int i = 0; i < num_ref_frames_in_pic_order_cnt_cycle; i++)
            {
                read_se();
                // sps->offset_for_ref_frame[ i ] = ReadSE();
            }
        }
        int max_num_ref_frames = read_exponential_golomb_code();
        UNUSE(max_num_ref_frames);
        int gaps_in_frame_num_value_allowed_flag = read_bit();
        UNUSE(gaps_in_frame_num_value_allowed_flag);
        int pic_width_in_mbs_minus1 = read_exponential_golomb_code();
        int pic_height_in_map_units_minus1 = read_exponential_golomb_code();
        int frame_mbs_only_flag = read_bit();
        if (!frame_mbs_only_flag)
        {
            int mb_adaptive_frame_field_flag = read_bit();
            UNUSE(mb_adaptive_frame_field_flag);
        }
        int direct_8x8_inference_flag = read_bit();
        UNUSE(direct_8x8_inference_flag);
        int frame_cropping_flag = read_bit();
        if (frame_cropping_flag)
        {
            frame_crop_left_offset = read_exponential_golomb_code();
            frame_crop_right_offset = read_exponential_golomb_code();
            frame_crop_top_offset = read_exponential_golomb_code();
            frame_crop_bottom_offset = read_exponential_golomb_code();
        }
        int vui_parameters_present_flag = read_bit();
        UNUSE(vui_parameters_present_flag);

        *width = ((pic_width_in_mbs_minus1 + 1) * 16) - frame_crop_bottom_offset * 2
                 - frame_crop_top_offset * 2;
        *height = ((2 - frame_mbs_only_flag) * (pic_height_in_map_units_minus1 + 1) * 16)
                  - (frame_crop_right_offset * 2) - (frame_crop_left_offset * 2);
    }
    NONCOPYABLE(H264Parse);

private:
    const uint8_t *start_;
    uint64_t length_;
    uint64_t current_bit_;
};

void h264_width_height(const uint8_t *begin, uint64_t len, int64_t *width, int64_t *height)
{
    H264Parse parse(begin, len);
    parse.h264_width_height(width, height);
}

#endif  // __H264_HPP__
