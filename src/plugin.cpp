#include "../include/plugin.hpp"

#include <future>
#include <thread>


using namespace ILLIXR;


audio_xcoding::audio_xcoding(phonebook *pb_, bool encoding)
        : threadloop{encoding ? "audio_encoding" : "audio_decoding", pb_},
          switchboard_{pb_->lookup_impl<switchboard>()}, clock_{pb_->lookup_impl<relative_clock>()},
          pose_{switchboard_->get_reader<data_format::pose::head_pose_type>("slow_pose")}, xcoder_{"", encoding
                                                                                                  ? ILLIXR::audio::ab_audio::process_type::ENCODE
                                                                                                  : ILLIXR::audio::ab_audio::process_type::DECODE},
          encoding_{encoding} {
    xcoder_.load_source(switchboard_);
}

void audio_xcoding::_p_thread_setup() {
    last_time_ = clock_->now();
}

threadloop::skip_option audio_xcoding::_p_should_skip() {
    last_time_ += audio_period_;
    std::this_thread::sleep_for(last_time_ - clock_->now());
    return skip_option::run;
}

void audio_xcoding::_p_one_iteration() {
    if (!encoding_) {
        [[maybe_unused]] auto most_recent_pose = pose_.get_ro_nullable();
    }
    xcoder_.process_block();
}


void audio_pipeline::start() {
    audio_encoding.start();
    audio_decoding.start();
    plugin::start();
}

void audio_pipeline::stop() {
    audio_encoding.stop();
    audio_decoding.stop();
    plugin::stop();
}


PLUGIN_MAIN(audio_pipeline)
