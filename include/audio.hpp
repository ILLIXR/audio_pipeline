#pragma once

#ifdef ILLIXR_INTEGRATION

#include "illixr/switchboard.hpp"

#endif

#include "sound.hpp"

#include <optional>
#include <pthread.h>
#include <string>
#include <string_view>
#include <vector>

namespace ILLIXR::audio {
class ab_audio {
public:
    // Process types
    enum class process_type {
        FULL,            // FULL for output wav file
        ENCODE,        // For profiling, do file reading and encoding without file output
        DECODE            // For profiling, do ambisonics decoding without file output
    };

    ab_audio(std::string output_file_path, process_type proc_type_in);

    // Process a block (1024) samples of sound
    void process_block();

    // Load sound source files (predefined)
#ifdef ILLIXR_INTEGRATION
    void load_source(const std::shared_ptr<ILLIXR::switchboard> &sb);
#else
    void load_source();
#endif

    // Buffer of most recent processed block for fast copying to audio buffer
    short most_recent_block_L[BLOCK_SIZE];
    short most_recent_block_R[BLOCK_SIZE];
    bool buffer_ready;

    // Number of blocks left to process before this stream is complete
    unsigned long num_blocks_left;

private:
    // Generate dummy WAV output file header
    void generate_wav_header();

    // Read in data from WAV files and encode into ambisonics
    void read_and_encode(CBFormat &sum_bf);

    // Apply rotation and zoom effects to the ambisonics sound field
    void rotate_and_zoom(CBFormat &sum_bf);

    // Write out a block of samples to the output file
    void write_file(float **result_sample);

    // Abort failed configuration
    void config_abort(const std::string_view &comp_name) const;

    void update_rotation();

    void update_zoom();

    process_type                 process_type_;
    std::vector<sound>           sound_srcs_;    //!< a list of sound sources in this audio
    std::optional<std::ofstream> output_file_;   //!< target output file
    CAmbisonicBinauralizer       decoder_;       //!< decoder associated with this audio
    CAmbisonicProcessor          rotator_;       //!< ambisonics rotator associated with this audio
    CAmbisonicZoomer             zoomer_;        //!< ambisonics zoomer associated with this audio
    int                          frame_ = 0;
};
}
