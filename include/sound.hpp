#pragma once

#include <fstream>
#include <memory>
#include <spatialaudio/Ambisonics.h>
#include <string>

constexpr std::size_t SAMPLERATE {48000U};
constexpr std::size_t BLOCK_SIZE {512U};
constexpr std::size_t NORDER     {3U};
constexpr std::size_t NUM_SRCS   {16U};

#define NUM_CHANNELS (OrderToComponents(NORDER, true))

namespace ILLIXR::audio{
class sound{
public:
    sound(std::string src_filename, unsigned n_order, bool b3D);

    // set sound src position
    void set_src_pos(const PolarPoint& pos);

    // set sound amplitude scale
    [[maybe_unused]] void set_src_amp(float amp_scale);

    // read sound samples from mono 16bit WAV file and encode into ambisonics format
    std::weak_ptr<CBFormat> read_in_b_format();
private:
    // Abort failed configuration
    void config_abort(const std::string_view& comp_name) const;

    std::fstream               src_file_;            //!< corresponding sound src file
    float                      sample_[BLOCK_SIZE];  //!< sample buffer HARDCODE
    std::shared_ptr<CBFormat>  b_format_;            //!< ambisonics format sound buffer
    CAmbisonicEncoderDist      b_encoder_;           //!< ambisonics encoder, containing format info, position info, etc.
    PolarPoint                 src_pos_;             //!< ambisonics position
    float                      amp_;                 //!< amplitude scale to avoid clipping
};
}
