#pragma once
#include "audio.hpp"

#include "illixr/data_format/pose.hpp"
#include "illixr/phonebook.hpp"
#include "illixr/relative_clock.hpp"
#include "illixr/switchboard.hpp"
#include "illixr/threadloop.hpp"


namespace ILLIXR {

class audio_xcoding : public threadloop {
public:
    audio_xcoding(phonebook *pb_, bool encoding);

    void _p_thread_setup() override;

    skip_option _p_should_skip() override;

    void _p_one_iteration() override;

private:
    const std::shared_ptr<switchboard>          switchboard_;
    const std::shared_ptr<relative_clock>       clock_;
    switchboard::reader<data_format::pose_type> pose_;
    ILLIXR::audio::ab_audio                     xcoder_;
    time_point                                  last_time_;
    static constexpr duration                   audio_period_{
            freq_to_period(static_cast<double>(SAMPLERATE) / static_cast<double>(BLOCK_SIZE))};
    bool                                        encoding_;
};

class audio_pipeline : public plugin {
public:
    [[maybe_unused]] audio_pipeline(const std::string &name_, phonebook *pb_)
            : plugin{name_, pb_}, audio_encoding{pb_, true}, audio_decoding{pb_, false} {}

    void start() override;

    void stop() override;

private:
    audio_xcoding audio_encoding;
    audio_xcoding audio_decoding;
};
}
