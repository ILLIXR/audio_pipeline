#include "audio.hpp"

#ifdef ILLIXR_INTEGRATION
#include "illixr/error_util.hpp"
#include "illixr/switchboard.hpp"
#endif /// ILLIXR_INTEGRATION

#include <iostream>
#include <cassert>
#include <cstdlib>

#ifdef ILLIXR_INTEGRATION
#include <filesystem>
#endif /// ILLIXR_INTEGRATION


#ifdef ILLIXR_INTEGRATION
std::string get_path(const std::shared_ptr<ILLIXR::switchboard>& sb) {
    if(sb) {
        std::string path = std::string{AUDIO_SAMPLES} + "/samples";
        if (std::filesystem::is_directory(path))
            return path;
        const char *AUDIO_ROOT = sb->get_env_char("AUDIO_ROOT");
        if (!AUDIO_ROOT)
            throw std::runtime_error("Ausio samples not found, please define AUDIO_ROOT");

        return std::string{AUDIO_ROOT} + "/samples/";
    } else {
#else
        std::string get_path() {
#endif
        return "samples/";
#ifdef ILLIXR_INTEGRATION
    }
#endif
}

ILLIXR::audio::ab_audio::ab_audio(std::string output_file_path, process_type proc_type_in)
    : process_type_ {proc_type_in}
    , output_file_ {
          process_type_ == ILLIXR::audio::ab_audio::process_type::FULL
          ? std::make_optional<std::ofstream>(output_file_path, std::ios_base::out | std::ios_base::binary)
          : std::nullopt
      } {
    if (process_type_ == ILLIXR::audio::ab_audio::process_type::FULL) {
        generate_wav_header();
    }

    unsigned int tailLength {0U};

    /// Binauralizer as ambisonics decoder
    if (!decoder_.Configure(NORDER, true, SAMPLERATE, BLOCK_SIZE, tailLength)) {
        config_abort("decoder");
    }

    /// Processor to rotate
    if (!rotator_.Configure(NORDER, true, BLOCK_SIZE, tailLength)) {
        config_abort("rotator");
    }

    /// Processor to zoom
    if (!zoomer_.Configure(NORDER, true, tailLength)) {
        config_abort("zoomer");
    }

    buffer_ready = false;
    num_blocks_left = 0;
}

#ifdef ILLIXR_INTEGRATION
void ILLIXR::audio::ab_audio::load_source(const std::shared_ptr<ILLIXR::switchboard>& sb){
#else
void ILLIXR::audio::ab_audio::loadSource(){
#endif
    /// Temporarily clear errno here if set (until merged with #225)
    if (errno > 0) {
        errno = 0;
    }

    /// Add a bunch of sound sources
#ifdef ILLIXR_INTEGRATION
    const std::string samples_folder{get_path(sb)};
#else
    const std::string samples_folder{get_path()};
#endif
    if (process_type_ == ILLIXR::audio::ab_audio::process_type::FULL) {
        sound_srcs_.emplace_back(samples_folder + "lectureSample.wav", NORDER, true);
        sound_srcs_.back().set_src_pos({
            .fAzimuth   = -0.1f,
            .fElevation = 3.14f/2,
            .fDistance  = 1
        });

        sound_srcs_.emplace_back(samples_folder + "radioMusicSample.wav", NORDER, true);
        sound_srcs_.back().set_src_pos({
            .fAzimuth   = 1.0f,
            .fElevation = 0.0f,
            .fDistance  = 5
        });
    } else {
        for (unsigned int i = 0U; i < NUM_SRCS; i++) {

            /// This line is setting errno (whose value is 2)
            /// As a temporary fix, we set errno to 0 within Sound's constructor, and assert that
            /// it has not been set
            /// The path here is broken, we need to specify a relative path like we do in kimera
            assert(errno == 0);
            sound_srcs_.emplace_back(samples_folder + "lectureSample.wav", NORDER, true);
            assert(errno == 0);

            sound_srcs_.back().set_src_pos({
                .fAzimuth   = i * -0.1f,
                .fElevation = i * 3.14f / 2,
                .fDistance  = i * 1.0f
            });
        }
    }
}


void ILLIXR::audio::ab_audio::process_block() {
    float** resultSample = new float*[2];
    resultSample[0] = new float[BLOCK_SIZE];
    resultSample[1] = new float[BLOCK_SIZE];

    /// Temporary BFormat file to sum up ambisonics
    CBFormat sumBF;
    sumBF.Configure(NORDER, true, BLOCK_SIZE);

    if (process_type_ != ILLIXR::audio::ab_audio::process_type::DECODE) {
        read_and_encode(sumBF);
    }

    if (process_type_ != ILLIXR::audio::ab_audio::process_type::ENCODE) {
        /// Processing garbage data if just decoding
        rotate_and_zoom(sumBF);
        decoder_.Process(&sumBF, resultSample);
    }

    if (process_type_ == ILLIXR::audio::ab_audio::process_type::FULL) {
        write_file(resultSample);
        if (num_blocks_left > 0) {
            num_blocks_left--;
        }
    }

    delete[] resultSample[0];
    delete[] resultSample[1];
    delete[] resultSample;
}


/// Read from WAV files and encode into ambisonics format
void ILLIXR::audio::ab_audio::read_and_encode(CBFormat& sumBF) {
    for (unsigned int soundIdx = 0U; soundIdx < sound_srcs_.size(); ++soundIdx) {
        /// 'readInBFormat' now returns a weak_ptr, ensuring that we don't access
        /// or destruct a freed resource
        std::weak_ptr<CBFormat> tempBF_weak {sound_srcs_[soundIdx].read_in_b_format()};
        std::shared_ptr<CBFormat> tempBF{tempBF_weak.lock()};

        if (tempBF != nullptr) {
            if (soundIdx == 0U) {
                sumBF = *tempBF;
            } else {
                sumBF += *tempBF;
            }
        } else {
            static constexpr std::string_view read_fail_msg{
                "[ab_audio] Failed to read/encode. Sound has expired or been destroyed."
            };
#ifdef ILLIXR_INTEGRATION
            throw std::runtime_error(std::string{read_fail_msg});
#else
            std::cerr << read_fail_msg << std::endl;
            throw std::runtime_error(std::string{read_fail_msg});
#endif /// ILLIXR_INTEGRATION
        }
   }
}


/// Simple rotation
void ILLIXR::audio::ab_audio::update_rotation() {
    frame_++;
    Orientation head(0, 0, 1.0 * frame_ / 1500 * 3.14 * 2);
    rotator_.SetOrientation(head);
    rotator_.Refresh();
}


/// Simple zoom
void ILLIXR::audio::ab_audio::update_zoom() {
    frame_++;
    zoomer_.SetZoom(sinf(frame_/100));
    zoomer_.Refresh();
}


/// Process some rotation and zoom effects
void ILLIXR::audio::ab_audio::rotate_and_zoom(CBFormat& sumBF) {
    update_rotation();
    rotator_.Process(&sumBF, BLOCK_SIZE);
    update_zoom();
    zoomer_.Process(&sumBF, BLOCK_SIZE);
}


void ILLIXR::audio::ab_audio::write_file(float** resultSample) {
    /// Normalize(Clipping), then write into file
    for (std::size_t sampleIdx = 0U; sampleIdx < BLOCK_SIZE; ++sampleIdx) {
        resultSample[0][sampleIdx] = std::max(std::min(resultSample[0][sampleIdx], +1.0f), -1.0f);
        resultSample[1][sampleIdx] = std::max(std::min(resultSample[1][sampleIdx], +1.0f), -1.0f);
        int16_t tempSample0 = (int16_t)(resultSample[0][sampleIdx]/1.0 * 32767);
        int16_t tempSample1 = (int16_t)(resultSample[1][sampleIdx]/1.0 * 32767);
        output_file_->write((char*)&tempSample0,sizeof(short));
        output_file_->write((char*)&tempSample1,sizeof(short));

        /// Cache written block in object buffer until needed by realtime audio thread
        most_recent_block_L[sampleIdx] = tempSample0;
        most_recent_block_R[sampleIdx] = tempSample1;
    }
}


namespace ILLIXR::audio
{
    /// NOTE: WAV FILE SIZE is not correct
    typedef struct __attribute__ ((packed)) WAVHeader_t
    {
        unsigned int sGroupID = 0x46464952;
        unsigned int dwFileLength = 48000000;       /// A large enough random number
        unsigned int sRiffType = 0x45564157;
        unsigned int subchunkID = 0x20746d66;
        unsigned int subchunksize = 16;
        unsigned short audioFormat = 1;
        unsigned short NumChannels = 2;
        unsigned int SampleRate = 48000;
        unsigned int byteRate = 48000*2*2;
        unsigned short BlockAlign = 2*2;
        unsigned short BitsPerSample = 16;
        unsigned int dataChunkID = 0x61746164;
        unsigned int dataChunkSize = 48000000;      /// A large enough random number
    } WAVHeader;
}


void ILLIXR::audio::ab_audio::generate_wav_header() {
    /// Brute force wav header
    WAVHeader wavh;
    output_file_->write((char*)&wavh, sizeof(WAVHeader));
}


void ILLIXR::audio::ab_audio::config_abort(const std::string_view& comp_name) const
{
    static constexpr std::string_view cfg_fail_msg{"[ABAudio] Failed to configure "};
#ifndef ILLIXR_INTEGRATION
    std::cerr << cfg_fail_msg << compName << std::endl;
#endif /// ILLIXR_INTEGRATION
    throw std::runtime_error(std::string{cfg_fail_msg} + std::string{comp_name});
}
