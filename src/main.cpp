#include "illixr/switchboard.hpp"

#include "audio.hpp"
#include "realtime.hpp"

#include <iostream>
#include <pthread.h>

int main(int argc, char const *argv[])
{
    using namespace ILLIXR::audio;

    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " <number of size " << BLOCK_SIZE << " blocks to process> ";
        std::cout << "<optional: encode/decode>\n";
        std::cout << "Note: If you want to hear the output sound, limit the process sample blocks so that the output is not longer than input!\n";
        return 1;
    }

    const int numBlocks = atoi(argv[1]);
    ab_audio::process_type procType(ab_audio::process_type::FULL);
    if (argc > 2){
        if (!strcmp(argv[2], "encode"))
            procType = ab_audio::process_type::ENCODE;
        else
            procType = ab_audio::process_type::DECODE;
    }

    ab_audio audio("output.wav", procType);
    audio.load_source(std::make_shared<ILLIXR::switchboard>(nullptr));
    audio.num_blocks_left = numBlocks;

    // Launch realtime audio thread for audio processing
    if (procType == ab_audio::process_type::FULL) {
        pthread_t rt_audio_thread;
        pthread_create(&rt_audio_thread, nullptr, illixr_rt_init, (void *)&audio);
        pthread_join(rt_audio_thread, nullptr);
    }
    else {
        for (int i = 0; i < numBlocks; ++i) {
            audio.process_block();
        }
    }

    return 0;
}
