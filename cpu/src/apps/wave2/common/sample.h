#ifndef SAMPLE_H
#define SAMPLE_H


enum t_sample_parameter {
    SAMPLE_PLAYBACK_RATE,
    SAMPLE_START_POINT,
    SAMPLE_LOOP_POINT,
    SAMPLE_PARAM_QUALITY,
    SAMPLE_PARAM_COUNT
};


typedef struct {

    int root_note;
    int low_note;
    int hi_note;
    int quality; 
    int start_position;
    int loop_point;
    int end_position;
    int playback_rate;
    // etc


} t_sample;


typedef t_sample *Sample;


#endif