
// this order is important
enum t_sample_quality {
    SAMPLE_QUALITY_HIGH,
    SAMPLE_QUALITY_MEDIUM,
    SAMPLE_QUALITY_LOW
};

typedef enum t_sample_quality SampleQuality;

typedef struct {

    int root_note;
    int low_note;
    int hi_note;
    SampleQuality quality; // 0 hi, 1 med, 2 low
    uint32_t start_position;
    uint32_t end_position;
    // etc


} t_sample;


typedef t_sample *Sample;
