/* ************************************************************************* *\
                  INTEL CORPORATION PROPRIETARY INFORMATION
     This software is supplied under the terms of a license agreement or
     nondisclosure agreement with Intel Corporation and may not be copied
     or disclosed except in accordance with the terms of that agreement.
          Copyright (C) 2012 Intel Corporation. All Rights Reserved.
\* ************************************************************************* */

#pragma once

#include "CL\cl.h"
#include "Particle_common.h"

cl_platform_id GetIntelOCLPlatform();
cl_platform_id GetATIOCLPlatform();

//slq 14.7.11
cl_platform_id GetNVOCLPlatform();


bool IsCPUDevicePresented(cl_platform_id );
bool IsGPUDevicePresented(cl_platform_id );

class iNBody
{
public:
    iNBody();
    ~iNBody();

    int Setup(cl_device_type deviceType, bool SDKProviderATI);
    void Cleanup();

    bool Execute( int sz, int wg_sz, int optimization, PARTICLES& particle_data, bool bManual = false, int nColorSplitMode =0 );
    size_t GetMaxWorkSize() { return m_szBodies; };
    float GetMinMass() { return m_fMinMass; };
    float GetMaxMass() { return m_fMaxMass; };
    float GetSplitting(){return m_fSplitting; };
    void SetSplitting(float f){m_fSplitting = f; };
    void SetValidation(bool b){m_bValidate= b; };


private:
    bool ExecuteNBodyKernel(const size_t szLocalWork,bool bManual = false  );
    bool Execute_NBody_C_Serial();
    bool Execute_NBody_C_Parallel();
    bool Execute_NBody_SSE_Parallel();
    bool Execute_NBody_Validate(
                                      const cl_float* pfInputVelocitiesX, const cl_float* pfInputVelocitiesY, const cl_float* pfInputVelocitiesZ,
                                      const cl_float* pfInputPositionsX,  const cl_float* pfInputPositionsY,  const cl_float* pfInputPositionsZ,
                                      const cl_float* pfOutputVelocitiesX, const cl_float* pfOutputVelocitiesY, const cl_float* pfOutputVelocitiesZ,
                                      const cl_float* pfOutputPositionsX,  const cl_float* pfOutputPositionsY,  const cl_float* pfOutputPositionsZ
                                      ) const;

    //    m_pfMass - input array for bodies' masses
    cl_float* m_pfMass;

    //    m_pfInputPositions - input arrays for bodies' positions x, y and z
    cl_float* m_pfInputPositionsX;
    cl_float* m_pfInputPositionsY;
    cl_float* m_pfInputPositionsZ;

    //    m_pfInputVelocities - input arrays for bodies' velocities in directions x, y and z
    cl_float* m_pfInputVelocitiesX;
    cl_float* m_pfInputVelocitiesY;
    cl_float* m_pfInputVelocitiesZ;

    //    m_pfOutputPositions - output arrays for bodies' positions x, y and z
    cl_float* m_pfOutputPositionsX;
    cl_float* m_pfOutputPositionsY;
    cl_float* m_pfOutputPositionsZ;

    //    m_pfOutputVelocities - output arrays for bodies' velocities in directions x, y and z
    cl_float* m_pfOutputVelocitiesX;
    cl_float* m_pfOutputVelocitiesY;
    cl_float* m_pfOutputVelocitiesZ;

    cl_float m_fSplitting;
    bool m_bValidate;

    //    m_szBodies - number of overall bodies in input, by default is 256
    size_t m_szBodies;
    //    m_fMinMass, m_fMaxMass - minimal and maximal values of float in input for mass, by default 100, must be non-negative (otherwise will be fabsED)
    cl_float m_fMinMass;
    cl_float m_fMaxMass;
    //    m_fMinPosition, m_fMaxPosition - minimal and maximal values of float in input for position, by default 100
    cl_float m_fMinPosition;
    cl_float m_fMaxPosition;
    //    m_fMinVelocity, m_fMaxVelocity - minimal and maximal values of float in input for velocity, by default 100
    cl_float m_fMinVelocity;
    cl_float m_fMaxVelocity;

    //    m_fEpsilonDistance - epsilon distance added to all distances, by default is 0.00001, must be non-negative (otherwise will be fabsED)
    cl_float m_fEpsilonDistance;

    //    m_fTimeDelta - time delta used for output velocities and positions evaluations, by default is 100, must be non-negative (otherwise will be fabsED)
    cl_float m_fTimeDelta;

    //    global and local sizes of global and local worksizes, by default all equal to 1
    size_t m_szGlobalWork;
    size_t m_szLocalWork;

    cl_context context;
    cl_command_queue cmd_queue;
    cl_command_queue cmd_queue_second;

    cl_program program;
    cl_kernel kernel;

    //    cl_mem objects used as parameters for kernels
    cl_mem m_pfMassBuffer;

    cl_mem m_pfInputPositionsBufferX;
    cl_mem m_pfInputPositionsBufferY;
    cl_mem m_pfInputPositionsBufferZ;

    cl_mem m_pfInputVelocitiesBufferX;
    cl_mem m_pfInputVelocitiesBufferY;
    cl_mem m_pfInputVelocitiesBufferZ;

    cl_mem m_pfOutputPositionsBufferX;
    cl_mem m_pfOutputPositionsBufferY;
    cl_mem m_pfOutputPositionsBufferZ;

    cl_mem m_pfOutputVelocitiesBufferX;
    cl_mem m_pfOutputVelocitiesBufferY;
    cl_mem m_pfOutputVelocitiesBufferZ;

    cl_mem pfInputPositionsBufferX ;
    cl_mem pfInputPositionsBufferY ;
    cl_mem pfInputPositionsBufferZ ;
    cl_mem pfInputVelocitiesBufferX;
    cl_mem pfInputVelocitiesBufferY;
    cl_mem pfInputVelocitiesBufferZ;
    cl_mem pfOutputPositionsBufferX;
    cl_mem pfOutputPositionsBufferY;
    cl_mem pfOutputPositionsBufferZ;
    cl_mem pfOutputVelocitiesBufferX;
    cl_mem pfOutputVelocitiesBufferY;
    cl_mem pfOutputVelocitiesBufferZ;

    cl_device_type m_deviceType;

    bool m_bFlipflop;
};
