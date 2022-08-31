#include <stdio.h>
#include <stdlib.h>
#include "../include/vosk_api.h"
#include "../include/portaudio.h"

#include <string.h>

/* #define SAMPLE_RATE  (17932) // Test failure to open with this value. */
#define SAMPLE_RATE  (44100)
#define FRAMES_PER_BUFFER (1024)
#define NUM_CHANNELS    (1)
#define NUM_SECONDS     (100)
/* #define DITHER_FLAG     (paDitherOff)  */
#define DITHER_FLAG     (0) /**/

/* @todo Underflow and overflow is disabled until we fix priming of blocking write. */
#define CHECK_OVERFLOW  (0)
#define CHECK_UNDERFLOW  (0)


#define PA_SAMPLE_TYPE  paFloat32
typedef float SAMPLE;
#define SAMPLE_SILENCE  (0.0f)
#define PRINTF_S_FORMAT "%.8f"

typedef struct {
    int frameIndex;  /* Index into sample array. */
    int maxFrameIndex;
    SAMPLE *recordedSamples;
}
        paTestData;

/* This routine will be called by the PortAudio engine when audio is needed.
** It may be called at interrupt level on some machines so don't do anything
** that could mess up the system like calling malloc() or free().
*/
static int recordCallback(const void *inputBuffer, void *outputBuffer,
                          unsigned long framesPerBuffer,
                          const PaStreamCallbackTimeInfo *timeInfo,
                          PaStreamCallbackFlags statusFlags,
                          void *userData) {
    paTestData *data = (paTestData *) userData;
    const SAMPLE *rptr = (const SAMPLE *) inputBuffer;
    SAMPLE *wptr = &data->recordedSamples[data->frameIndex * NUM_CHANNELS];
    long framesToCalc;
    long i;
    int finished;
    unsigned long framesLeft = data->maxFrameIndex - data->frameIndex;

    (void) outputBuffer; /* Prevent unused variable warnings. */
    (void) timeInfo;
    (void) statusFlags;
    (void) userData;

    if (framesLeft < framesPerBuffer) {
        framesToCalc = framesLeft;
        finished = paComplete;
    } else {
        framesToCalc = framesPerBuffer;
        finished = paContinue;
    }

    if (inputBuffer == NULL) {
        for (i = 0; i < framesToCalc; i++) {
            *wptr++ = SAMPLE_SILENCE;  /* left */
            if (NUM_CHANNELS == 2) *wptr++ = SAMPLE_SILENCE;  /* right */
        }
    } else {
        for (i = 0; i < framesToCalc; i++) {
            *wptr++ = *rptr++;  /* left */
            if (NUM_CHANNELS == 2) *wptr++ = *rptr++;  /* right */
        }
    }
    data->frameIndex += framesToCalc;
    return finished;
}

/*******************************************************************/
int main(void);

int main(void) {
    PaStreamParameters inputParameters,
            outputParameters;
    PaStream *stream;
    PaError err = paNoError;
    paTestData data;
    int i;
    int totalFrames;
    int numSamples;
    int numBytes;
    SAMPLE max, val;
    double average;

    printf("patest_record.c\n");
    fflush(stdout);

    data.maxFrameIndex = totalFrames = NUM_SECONDS * SAMPLE_RATE; /* Record for a few seconds. */
    data.frameIndex = 0;
    numSamples = totalFrames * NUM_CHANNELS;
    numBytes = numSamples * sizeof(SAMPLE);
    data.recordedSamples = (SAMPLE *) malloc(numBytes); /* From now on, recordedSamples is initialised. */
    if (data.recordedSamples == NULL) {
        printf("Could not allocate record array.\n");
        goto done;
    }
    for (i = 0; i < numSamples; i++) data.recordedSamples[i] = 0;

    err = Pa_Initialize();
    if (err != paNoError) goto done;

    inputParameters.device = Pa_GetDefaultInputDevice(); /* default input device */
    if (inputParameters.device == paNoDevice) {
        fprintf(stderr, "Error: No default input device.\n");
        goto done;
    }
    inputParameters.channelCount = NUM_CHANNELS;
    inputParameters.sampleFormat = PA_SAMPLE_TYPE;
    inputParameters.suggestedLatency = Pa_GetDeviceInfo(inputParameters.device)->defaultLowInputLatency;
    inputParameters.hostApiSpecificStreamInfo = NULL;

    /* Record some audio. -------------------------------------------- */
    err = Pa_OpenStream(
            &stream,
            &inputParameters,
            NULL,                  /* &outputParameters, */
            SAMPLE_RATE,
            FRAMES_PER_BUFFER,
            paClipOff,      /* we won't output out of range samples so don't bother clipping them */
            recordCallback,
            &data);
    if (err != paNoError) goto done;

    err = Pa_StartStream(stream);
    if (err != paNoError) goto done;
    printf("\n=== Now recording!! Please speak into the microphone. ===\n");
    fflush(stdout);

    VoskModel *model = vosk_model_new("../model");
    VoskRecognizer *recognizer = vosk_recognizer_new(model, 16000.0F);

    int final;

    while ((err = Pa_IsStreamActive(stream)) == 1) {
//        Pa_Sleep(1000);
        printf("index = %d\n", data.frameIndex);

        final = vosk_recognizer_accept_waveform_f(recognizer,
                                                  &data.recordedSamples[data.frameIndex*NUM_CHANNELS],
                                                  sizeof(data.recordedSamples[data.frameIndex*NUM_CHANNELS]));
        if (final) {
            printf("%s\n", vosk_recognizer_result(recognizer));
        } else {
            printf("%s\n", vosk_recognizer_partial_result(recognizer));
        }
        printf("%s\n", vosk_recognizer_final_result(recognizer));
        fflush(stdout);

    }

    vosk_recognizer_free(recognizer);
    vosk_model_free(model);

    if (err < 0) goto done;

    err = Pa_CloseStream(stream);
    if (err != paNoError) goto done;

    done:
    Pa_Terminate();
    if (data.recordedSamples)       /* Sure it is NULL or valid. */
        free(data.recordedSamples);
    if (err != paNoError) {
        fprintf(stderr, "An error occurred while using the portaudio stream\n");
        fprintf(stderr, "Error number: %d\n", err);
        fprintf(stderr, "Error message: %s\n", Pa_GetErrorText(err));
        err = 1;          /* Always return 0 or 1, but no other return codes. */
    }
    return err;
}