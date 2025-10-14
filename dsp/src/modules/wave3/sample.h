#ifndef SAMPLE_H
#define SAMPLE_H



typedef struct {

    int root_note;
    int low_note;
    int hi_note;
    int quality; 
    int start_position;
    int loop_point;
    int end_position;
    int playback_rate;
    int global_offset; // added to start position for playback

} t_sample;


typedef t_sample *Sample;


#endif