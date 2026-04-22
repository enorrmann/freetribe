#ifndef PARAMETERS_H
#define PARAMETERS_H

/**
 * @brief   Enumeration of module parameters.
 *
 * Index of each external parameter of module.
 */
typedef enum {
    PARAM_TRANSMISSION_START,
    PARAM_TRANSMISSION_END,
    PARAM_SAMPLE_LOAD,
    PARAM_SAMPLE_COUNT_UPDATE,
    PARAM_TRANSFER_MEMORY,
    PARAM_SET_CPU_RECEIVE_BUFFER_ADDRESS,
    PARAM_SET_CPU_CALLBACK_FUNCTION_ADDRESS,
    PARAM_COUNT /// Should remain last to return number of parameters.
} e_param;

#endif // PARAMETERS_H