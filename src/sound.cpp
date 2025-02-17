#include "sound.hpp"

#ifdef ILLIXR_INTEGRATION
#include "illixr/error_util.hpp"
#endif /// ILLIXR_INTEGRATION

#include <cassert>
#include <cstddef>


ILLIXR::audio::sound::sound(std::string src_filename, unsigned int n_order, bool b3D)
        : src_file_{src_filename, std::fstream::in}
        , b_format_{std::make_shared<CBFormat>()}
        , amp_{1.0} {
    (void) b3D;
    /// NOTE: This is currently only accepts mono channel 16-bit depth WAV file
    /// TODO: Change brutal read from wav file
    constexpr std::size_t SRC_FILE_SIZE {44U};
    std::byte temp[SRC_FILE_SIZE];
    src_file_.read(reinterpret_cast<char*>(temp), sizeof(temp));

    /// BFormat file initialization
    if (!b_format_->Configure(n_order, true, BLOCK_SIZE)) {
        config_abort("BFormat");
    }
    b_format_->Refresh();

    /// Encoder initialization
    if (!b_encoder_.Configure(n_order, true, SAMPLERATE)) {
        config_abort("b_encoder_");
    }
    b_encoder_.Refresh();

    src_pos_.fAzimuth   = 0.0f;
    src_pos_.fElevation = 0.0f;
    src_pos_.fDistance  = 0.0f;

    b_encoder_.SetPosition(src_pos_);
    b_encoder_.Refresh();

    /// Clear errno, as this constructor is setting the flag (with value 2)
    /// A temporary fix.
    errno = 0;
}


void ILLIXR::audio::sound::set_src_pos(const PolarPoint& pos) {
    src_pos_.fAzimuth   = pos.fAzimuth;
    src_pos_.fElevation = pos.fElevation;
    src_pos_.fDistance  = pos.fDistance;

    b_encoder_.SetPosition(src_pos_);
    b_encoder_.Refresh();
}


[[maybe_unused]] void ILLIXR::audio::sound::set_src_amp(float amp_scale) {
    amp_ = amp_scale;
}


/// TODO: Change brutal read from wav file
std::weak_ptr<CBFormat> ILLIXR::audio::sound::read_in_b_format() {
    float sampleTemp[BLOCK_SIZE];
    src_file_.read((char*)sampleTemp, BLOCK_SIZE * sizeof(short));

    /// Normalize samples to -1 to 1 float, with amplitude scale
    constexpr float SAMPLE_DIV {(2 << 14) - 1}; /// 32767.0f == 2^14 - 1
    for (std::size_t i = 0U; i < BLOCK_SIZE; ++i) {
        sample_[i] = amp_ * (sampleTemp[i] / SAMPLE_DIV);
    }

    b_encoder_.Process(sample_, BLOCK_SIZE, b_format_.get());
    return b_format_;
}

void ILLIXR::audio::sound::config_abort(const std::string_view& comp_name) const
{
    static constexpr std::string_view cfg_fail_msg{"[sound] Failed to configure "};
#ifdef ILLIXR_INTEGRATION
    ILLIXR::abort(std::string{cfg_fail_msg} + std::string{comp_name});
#else
    std::cerr << cfg_fail_msg << comp_name << std::endl;
    std::abort();
#endif /// ILLIXR_INTEGRATION
}
