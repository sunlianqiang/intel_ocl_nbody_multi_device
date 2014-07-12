/* ************************************************************************* *\
                  INTEL CORPORATION PROPRIETARY INFORMATION
     This software is supplied under the terms of a license agreement or
     nondisclosure agreement with Intel Corporation and may not be copied
     or disclosed except in accordance with the terms of that agreement.
          Copyright (C) 2012 Intel Corporation. All Rights Reserved.
\* ************************************************************************* */

//    nBodyScalarKernel
//    Each item evaluates velocity and position after time_delta units of time for its szElementsPerItem bodies within the input
//    input_position_x - input array with current x axis position of the element
//    input_position_y - input array with current y axis position of the element
//    input_position_z - input array with current z axis position of the element
//    mass - mass of the element
//    input_velocity_x - input array with current x axis velocity of the element
//    input_velocity_y - input array with current y axis velocity of the element
//    input_velocity_z - input array with current z axis velocity of the element
//    output_position_x - output array with current x axis position of the element
//    output_position_y - output array with current y axis position of the element
//    output_position_z - output array with current z axis position of the element
//    output_velocity_x - output array with current x axis velocity of the element
//    output_velocity_y - output array with current y axis velocity of the element
//    output_velocity_z - output array with current z axis velocity of the element
//    softening_squared - represents epsilon noise added to the distance evaluation between each pair of elements

__kernel
void nBodyScalarKernel(
        const __global float *input_position_x, const __global float *input_position_y, const __global float *input_position_z,
        const __global float *mass,
        const __global float *input_velocity_x, const __global float *input_velocity_y, const __global float *input_velocity_z,
        __global float *output_position_x, __global float *output_position_y, __global float *output_position_z,
        __global float *output_velocity_x, __global float *output_velocity_y, __global float *output_velocity_z,
        float softening_squared, float time_delta, int offset /*for subbuffer*/, int body_count )
{
    const int index = get_global_id(0);

    const float position_x = (float)(input_position_x[index]);
    const float position_y = (float)(input_position_y[index]);
    const float position_z = (float)(input_position_z[index]);

    float acc_x = 0;
    float acc_y = 0;
    float acc_z = 0;
    for (int i = 0; i < body_count; i++)
    {
        float dx = input_position_x[i] - position_x;
        float dy = input_position_y[i] - position_y;
        float dz = input_position_z[i] - position_z;

        float distance_squared = dx * dx + dy * dy + dz * dz + softening_squared;
        float inverse_distance = rsqrt(distance_squared);
        float s = (mass[i] * inverse_distance) * (inverse_distance * inverse_distance);

        acc_x += dx * s;
        acc_y += dy * s;
        acc_z += dz * s;
    }
    const int index_offset = index-offset;//we used non-overlapped subbuffers to write to the same buffer simulteneously with 2 devices
    output_velocity_x[index_offset] = input_velocity_x[index] + acc_x * time_delta;
    output_velocity_y[index_offset] = input_velocity_y[index] + acc_y * time_delta;
    output_velocity_z[index_offset] = input_velocity_z[index] + acc_z * time_delta;

    output_position_x[index_offset] = input_position_x[index] + input_velocity_x[index] * time_delta + acc_x * time_delta * time_delta*0.5f;
    output_position_y[index_offset] = input_position_y[index] + input_velocity_y[index] * time_delta + acc_y * time_delta * time_delta*0.5f;
    output_position_z[index_offset] = input_position_z[index] + input_velocity_z[index] * time_delta + acc_z * time_delta * time_delta*0.5f;

}