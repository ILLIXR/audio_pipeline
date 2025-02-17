#include "sound.hpp"
#include "audio.hpp"

#include <cstdio>
#include <portaudio.h>
#include <realtime.hpp>

#define SAMPLE_RATE ((SAMPLERATE))
#define AUDIO_FORMAT   paInt16
typedef unsigned short          sample_t;
#define SILENCE       ((sample_t)0x00)

using namespace ILLIXR::audio;

/*
 * illixr_rt_cb
 *
 * ILLIXR realtime audio callback method
 *
 * When the realtime audio driver requires more buffered data, this callback
 * method is called. This is where ILLIXR feeds more audio data to the realtime audio engine.
 */
static int illixr_rt_cb(const void *input_buffer, void *output_buffer,
                        unsigned long frames_per_buffer,
                        const PaStreamCallbackTimeInfo* time_info,
                        PaStreamCallbackFlags status_flags,
                        void *user_data ) {
    (void) frames_per_buffer;
    (void) time_info;
    (void) status_flags;
    auto *out = (sample_t*)output_buffer;
    auto *audio_obj = (ab_audio *)user_data;
    int i;
    (void) input_buffer; /* Prevent unused variable warnings. */

    // As this is an interrupt context, lots of data processing is ill-advised.
    // In future iterations, all processing will be handled in a separate thread with proper
    // synchronization primatives.
    audio_obj->process_block();

    for (i = 0; i < BLOCK_SIZE; i++) {
        *out++ = audio_obj->most_recent_block_L[i];
        *out++ = audio_obj->most_recent_block_R[i];
    }

    return 0;
}

/*******************************************************************/

void* print_err(const PaError err) {
    Pa_Terminate();
    fprintf( stderr, "An error occured while using the portaudio stream\n" );
    fprintf( stderr, "Error number: %d\n", err );
    fprintf( stderr, "Error message: %s\n", Pa_GetErrorText( err ) );
    return (void *)err;

}

/*
 * illixr_rt_init
 *
 * Initializes and launches the realtime audio thread, communicating with PortAudio
 * This method should be launched as a separate pthread.
 *
 * Arguments:
 * audioObj - A pointer to an ABAudio object from which audio will be streamed to the speakers.
 *
 * Returns:
 * PortAudio error codes cast to void * type
 *
 * Side Effects:
 * This function is blocking and will not return until the audio source is exhausted.
 * Launch this in an independent thread!
 */
void *illixr_rt_init(void *audio_obj) {
    PaStreamParameters  output_parameters;
    PaStream*           stream;
    PaError             err;
    PaTime              stream_opened;

    printf("Initializing audio hardware...\n");

    err = Pa_Initialize();
    if( err != paNoError )
        return print_err(err);

    output_parameters.device = Pa_GetDefaultOutputDevice(); /* Default output device. */
    if (output_parameters.device == paNoDevice) {
        fprintf(stderr,"Error: No default output device.\n");
        return print_err(err);
    }
    output_parameters.channelCount = 2;                     /* Stereo output. */
    output_parameters.sampleFormat = AUDIO_FORMAT;
    output_parameters.suggestedLatency = Pa_GetDeviceInfo( output_parameters.device )->defaultLowOutputLatency;
    output_parameters.hostApiSpecificStreamInfo = NULL;
    err = Pa_OpenStream( &stream,
                         NULL,      /* No input. */
                         &output_parameters,
                         SAMPLE_RATE,
                         BLOCK_SIZE,       /* Frames per buffer. */
                         paClipOff, /* We won't output out of range samples so don't bother clipping them. */
                         illixr_rt_cb,
                         audio_obj);
    if( err != paNoError )
        return print_err(err);

    stream_opened = Pa_GetStreamTime( stream ); /* Time in seconds when stream was opened (approx). */

    printf("Launching stream!\n");
    err = Pa_StartStream( stream );
    if( err != paNoError )
        return print_err(err);

    // Spin until the audio marks itself as being complete
    while( ((ab_audio *)audio_obj)->num_blocks_left > 0) {
        Pa_Sleep(25);
    }

    printf("Stopping stream\n");

    err = Pa_CloseStream( stream );
    if( err != paNoError )
        return print_err(err);

    Pa_Terminate();
    return (void *)err;
}
