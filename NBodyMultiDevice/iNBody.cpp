/* ************************************************************************* *\
                  INTEL CORPORATION PROPRIETARY INFORMATION
     This software is supplied under the terms of a license agreement or
     nondisclosure agreement with Intel Corporation and may not be copied
     or disclosed except in accordance with the terms of that agreement.
          Copyright (C) 2012 Intel Corporation. All Rights Reserved.
\* ************************************************************************* */

#include "stdafx.h"
#include "omp.h"
#include "iNBody.h"
#include "basic.hpp"
extern char  g_device_name[128];

//variables for load-balancing and averaging of the splitting ratio (CB - cyclic buffer to keep performance history of the prev frames)
cl_double g_NDRangeTime1 = 1;
cl_double g_NDRangeTime2 = 1;
cl_double g_NDRangeTimeRatioLast = 0.5;
cl_double g_NDRangeTimeRatioCB[256];
cl_uint g_NDRangeTimeRatioCBCounter=0;
const cl_uint g_NDRangeTimeRatioCBLength=10;
cl_bool g_NDRangeTimeRatioCBFull = false;

void SplitRange(cl_float dNDRangeRatio, size_t arrayHeight, size_t *arrayHeightDev1, size_t *arrayHeightDev2, size_t localSize)
{
    //estimate buffer split ratio
    //32 bodies or actually 128 bytes (==32*sizeof(float)) is the min granularity required for USE_HOST_PTR-enabled (sub-)buffer to be truly zero-copy;
    //you can query this value via clGetDeviceInfo(...CL_MEM_BASE_ADDR_ALIGN), refer to the Optimization Guide
    localSize = max(localSize, 32);
    *arrayHeightDev1 = (cl_uint)(dNDRangeRatio*(cl_float)arrayHeight);
    *arrayHeightDev1 = (*arrayHeightDev1 / localSize)*localSize;//make the arrayHeightDev1 to be dividable by local size
    *arrayHeightDev1 = max(localSize,*arrayHeightDev1);
    *arrayHeightDev2 = arrayHeight - *arrayHeightDev1;//the rest is for the second device
    *arrayHeightDev2 = max(localSize,*arrayHeightDev2);
    assert((*arrayHeightDev1 + *arrayHeightDev2)==arrayHeight);
}
//helper function for calculation of the splitting ratio (for input/output buffers)
float ComputeSplittingRatio(size_t arrayHeight, size_t *arrayHeightDev1, size_t *arrayHeightDev2, size_t localSize)
{
    //estimate ratio using the previous frame performance data
    cl_double dNDRangeRatio = (g_NDRangeTime2*g_NDRangeTimeRatioLast)/(g_NDRangeTime1*(1-g_NDRangeTimeRatioLast)+g_NDRangeTime2*g_NDRangeTimeRatioLast);

    //here we compute splitting ratio,while averaging it over last "frames"
    //fill cyclic buffer
    g_NDRangeTimeRatioCB[g_NDRangeTimeRatioCBCounter] = dNDRangeRatio;
    g_NDRangeTimeRatioCBCounter++;
    cl_double tmpNDRangeTimeRatioSum = 0.0;
    //average over cyclic buffer
    int num = g_NDRangeTimeRatioCBFull ? g_NDRangeTimeRatioCBLength : g_NDRangeTimeRatioCBCounter;
    for(int iii = 0; iii < num; iii++)
    {
        tmpNDRangeTimeRatioSum += g_NDRangeTimeRatioCB[iii];
    }
    tmpNDRangeTimeRatioSum = tmpNDRangeTimeRatioSum/num;//averaging
    //check cyclic buffer fullness
    if(g_NDRangeTimeRatioCBCounter==g_NDRangeTimeRatioCBLength)
    {
        g_NDRangeTimeRatioCBFull = true;
        g_NDRangeTimeRatioCBCounter = 0;//reset cyclic buffer counter
    }
    //update ratio
    dNDRangeRatio = tmpNDRangeTimeRatioSum;
    SplitRange((cl_float)dNDRangeRatio,arrayHeight,arrayHeightDev1,arrayHeightDev2,localSize);
    g_NDRangeTimeRatioLast = dNDRangeRatio;
    return (float)dNDRangeRatio;

}

static cl_float errorSize(cl_float f1, cl_float f2)
{
    return fabs(fabs(f1) - fabs(f2))/fabs(f1);
}

inline float __native_rsqrtf(float a)
{
    __m128 srctmp = _mm_set1_ps(a);
    __m128 dst;
    dst = _mm_sqrt_ps(srctmp);
    dst = _mm_div_ps(_mm_set1_ps(1.f), dst);
    return _mm_cvtss_f32(dst);
}

inline __m128 __native_rsqrtf4(__m128  srctmp)
{
    __m128 dst;
    dst = _mm_sqrt_ps(srctmp);
    dst = _mm_div_ps(_mm_set1_ps(1.f), dst);
    return dst;
}

#if defined(__INTEL_COMPILER)
inline __m256 __native_rsqrtf8(__m256  srctmp)
{    __m256 dst;
    dst = _mm256_sqrt_ps(srctmp);
    dst = _mm256_div_ps(_mm256_set1_ps(1.f), dst);
    return dst;
}
#endif

static cl_float randomFloat(const cl_float fMinValue, const cl_float fMaxValue)
{
    if (fMinValue == fMaxValue)
    {
        return fMaxValue;
    }
    cl_float tmp = static_cast<cl_float> (rand() % static_cast<int> (fMaxValue - fMinValue));
    tmp = (0 == tmp) ? 1.0f : tmp;
    tmp = 1.0f;
    return fMinValue + (static_cast<cl_float> (rand() % static_cast<int> (fMaxValue - fMinValue))) / tmp;
}

inline void freeCLMem(cl_mem &mem)
{
    if (NULL != mem)
    {
        clReleaseMemObject(mem);
        mem = NULL;
    }
}


char *ReadSources(const wchar_t *fileName)
{
    FILE *file = _wfopen(fileName, L"rb");
    if (!file)
    {
        fprintf(stderr, "Failed to open file '%s'\n", fileName);
        return NULL;
    }

    if (fseek(file, 0, SEEK_END))
    {
        fprintf(stderr, "Failed to seek file '%s'\n", fileName);
        return NULL;
    }

    long size = ftell(file);
    if (size == 0)
    {
        fprintf(stderr, "Failed to check position on file '%s'\n", fileName);
        return NULL;
    }

    rewind(file);

    char *src = (char *)malloc(sizeof(char) * size + 1);
    if (!src)
    {
        fprintf(stderr, "Failed to allocate memory for file '%s'\n", fileName);
        return NULL;
    }

    fprintf(stderr, "Reading file '%s' (size %ld bytes)\n", fileName, size);
    size_t res = fread(src, 1, sizeof(char) * size, file);
    if (res != sizeof(char) * size)
    {
        fprintf(stderr, "Failed to read file '%s' (read %ld)\n", fileName, res);
        return NULL;
    }

    src[size] = '\0'; /* NULL terminated */
    fclose(file);

    return src;
}

void BuildFailLog(
                  cl_program program,
                  cl_device_id device_id
                  )
{
    size_t paramValueSizeRet = 0;
    clGetProgramBuildInfo(program, device_id, CL_PROGRAM_BUILD_LOG, 0, NULL, &paramValueSizeRet);

    char* buildLog = new char[paramValueSizeRet];
    clGetProgramBuildInfo(program, device_id, CL_PROGRAM_BUILD_LOG, paramValueSizeRet, buildLog, &paramValueSizeRet);

    printf("Build Log:\n");
    for (size_t i = 0; i < paramValueSizeRet; ++i)
    {
        printf("%c", buildLog[i]);
    }
    printf("\n");
    fflush(stdout);
    delete[] buildLog;
}

cl_platform_id GetIntelOCLPlatform()
{
    cl_platform_id pPlatforms[10] = { 0 };
    char pPlatformName[128] = { 0 };

    cl_uint uiPlatformsCount = 0;
    cl_int err = clGetPlatformIDs(10, pPlatforms, &uiPlatformsCount);
    for (cl_uint ui = 0; ui < uiPlatformsCount; ++ui)
    {
        err = clGetPlatformInfo(pPlatforms[ui], CL_PLATFORM_NAME, 128 * sizeof(char), pPlatformName, NULL);
        if ( err != CL_SUCCESS )
        {
            printf("ERROR: Failed to retreive platform vendor name.\n", ui);
            return NULL;
        }

        if (!strcmp(pPlatformName, "Intel(R) OpenCL"))
            return pPlatforms[ui];
    }

    return NULL;
}

cl_platform_id GetATIOCLPlatform()
{
    cl_platform_id pPlatforms[10] = { 0 };
    char pPlatformName[128] = { 0 };

    cl_uint uiPlatformsCount = 0;
    cl_int err = clGetPlatformIDs(10, pPlatforms, &uiPlatformsCount);
    for (cl_uint ui = 0; ui < uiPlatformsCount; ++ui)
    {
        err = clGetPlatformInfo(pPlatforms[ui], CL_PLATFORM_NAME, 128 * sizeof(char), pPlatformName, NULL);
        if ( err != CL_SUCCESS )
        {
            printf("ERROR: Failed to retreive platform vendor name.\n", ui);
            return NULL;
        }

        if (!strcmp(pPlatformName, "ATI Stream") || !strcmp(pPlatformName, "AMD Accelerated Parallel Processing")) // "Advanced Micro Devices, Inc."
            return pPlatforms[ui];
    }

    return NULL;
}

cl_platform_id GetNVOCLPlatform()
{
	cl_platform_id pPlatforms[10] = { 0 };
	char pPlatformName[128] = { 0 };

	cl_uint uiPlatformsCount = 0;
	cl_int err = clGetPlatformIDs(10, pPlatforms, &uiPlatformsCount);
	for (cl_uint ui = 0; ui < uiPlatformsCount; ++ui)
	{
		err = clGetPlatformInfo(pPlatforms[ui], CL_PLATFORM_NAME, 128 * sizeof(char), pPlatformName, NULL);
		if ( err != CL_SUCCESS )
		{
			printf("ERROR: Failed to retreive platform vendor name.\n", ui);
			return NULL;
		}

		if (!strcmp(pPlatformName, "NVIDIA CUDA"))
			return pPlatforms[ui];
	}

	return NULL;
}

bool IsCPUDevicePresented(cl_platform_id id)
{
    cl_uint num = 0;
    clGetDeviceIDs(id,CL_DEVICE_TYPE_CPU, 0, NULL, &num);
    return num!=0;
}

bool IsGPUDevicePresented(cl_platform_id id )
{
    cl_uint num = 0;
    clGetDeviceIDs(id,CL_DEVICE_TYPE_GPU,0, NULL, &num);
    return num!=0;
}


int iNBody::Setup(cl_device_type deviceType, bool SDKProviderATI)
{
    cl_uint size_ret = 0;
    size_t cb;
    cl_device_id *devices;
    //cl_device_id devices[16]
    cl_int err;
    cl_uint alignment;
    /*--------------------------------------setup device, etc.-----------------------------------------*/
    cl_platform_id ocl_platform;
    if(SDKProviderATI)
    {
		//slq ATI to NV
        //find ATI platform
        //ocl_platform = GetATIOCLPlatform();

		ocl_platform = GetNVOCLPlatform();
    }
    else
    {
        //find Intel platform
        ocl_platform = GetIntelOCLPlatform();
    }

    cl_context_properties context_properties[3] = {CL_CONTEXT_PLATFORM, (cl_context_properties)ocl_platform, NULL };
    // create the OpenCL context on the device(s)
    context = clCreateContextFromType(context_properties, deviceType, NULL, NULL, NULL);

    if (context == (cl_context)0)
        return -1;

    // get the list of devices associated with context
    err = clGetContextInfo(context, CL_CONTEXT_DEVICES, 0, NULL, &cb);
    devices = (cl_device_id*)malloc(cb);
    clGetContextInfo(context, CL_CONTEXT_DEVICES, cb, devices, NULL);

    err = clGetDeviceInfo (devices[0],
                        CL_DEVICE_MIN_DATA_TYPE_ALIGN_SIZE,//CL_DEVICE_MEM_BASE_ADDR_ALIGN
                        sizeof(cl_uint),
                        &alignment,
                        NULL);

    printf("Data alignment is %d bytes.\n", alignment);

    err = clGetDeviceInfo(devices[0], CL_DEVICE_NAME, 128, g_device_name, NULL);
    printf("Using device %s...\n", g_device_name);

    m_deviceType = deviceType;
    // create a command-queue
    if(deviceType!=CL_DEVICE_TYPE_ALL)
    {
        cmd_queue = clCreateCommandQueue(context, devices[0], 0, NULL);
        if (cmd_queue == (cl_command_queue)0)
        {
            clReleaseContext(context);
            free(devices);
            return -1;
        }
    }
    else
    {
        g_NDRangeTime1 = 1;
        g_NDRangeTime2 = 1;
        g_NDRangeTimeRatioLast = 0.5;
        cl_device_type type;
        cl_device_id devicesCPU = 0, devicesGPU = 0;
        err = clGetDeviceInfo(devices[0],CL_DEVICE_TYPE, sizeof(cl_device_type),&type, NULL);
        if(CL_DEVICE_TYPE_CPU == type)
        {
            devicesCPU =devices[0];
        }else if(CL_DEVICE_TYPE_GPU == type)
        {
            devicesGPU =devices[0];
        }
        err = clGetDeviceInfo(devices[1],CL_DEVICE_TYPE, sizeof(cl_device_type),&type, NULL);
        if(CL_DEVICE_TYPE_CPU == type)
        {
            devicesCPU =devices[1];
        }else if(CL_DEVICE_TYPE_GPU == type)
        {
            devicesGPU =devices[1];
        }
         if(!devicesGPU  || !devicesCPU)
        {
            printf("One of devices (CPU or GPU) is missing\n");
            return err;
        }


        cmd_queue = clCreateCommandQueue(context, devicesCPU, CL_QUEUE_PROFILING_ENABLE, NULL);
        if (cmd_queue == (cl_command_queue)0)
        {
            clReleaseContext(context);
            free(devices);
            return -1;
        }
        err = clGetDeviceInfo(devicesCPU, CL_DEVICE_NAME, 128, &g_device_name, NULL);
        printf("Using CPU device %s...\n", g_device_name);
        err = clGetDeviceInfo(devicesGPU, CL_DEVICE_NAME, 128, &g_device_name, NULL);
        printf("Using GPU device %s...\n", g_device_name);

        sprintf(g_device_name, "Multi device\n");

        cmd_queue_second = clCreateCommandQueue(context, devicesGPU, CL_QUEUE_PROFILING_ENABLE, NULL);
        if (cmd_queue_second == (cl_command_queue)0)
        {
            clReleaseCommandQueue(cmd_queue);
            clReleaseContext(context);
            free(devices);
            return -1;
        }
    }

    /*-----------------------------------------compile the kernel-----------------------------------------*/
    char *sources = ReadSources(FULL_PATH_W("iNBody.cl"));    //read program .cl source file
    program = clCreateProgramWithSource(context, 1, (const char**)&sources, NULL, &err);    // create the program
    if (program == (cl_program)0)
        fprintf(stderr, "Failed to create program from source...\n");

    err = clBuildProgram(program, 0, NULL, NULL, NULL, NULL);    // build the program
    if (err != CL_SUCCESS)
    {
        fprintf(stderr, "Failed to build program...\n");
        BuildFailLog(program, devices[0]);
    }

    kernel = clCreateKernel(program, "nBodyScalarKernel", &err);        // create the kernel
    if (kernel == (cl_kernel)0)
        fprintf(stderr, "Failed to create scalar kernel...\n");

    free(sources);
    free(devices);

    m_szGlobalWork = (size_t)m_szBodies;
    m_fMinMass = fabs(m_fMinMass);
    m_fMaxMass = fabs(m_fMaxMass);
    m_fEpsilonDistance = fabs(m_fEpsilonDistance);
    m_fTimeDelta = fabs(m_fTimeDelta);

    //    allocate data arrays
#define ALLOC_FLOAT(_s) (cl_float*)_mm_malloc(_s*sizeof(cl_float), alignment)
    m_pfMass = ALLOC_FLOAT(m_szBodies);

    m_pfInputPositionsX = ALLOC_FLOAT(m_szBodies);
    m_pfInputPositionsY = ALLOC_FLOAT(m_szBodies);
    m_pfInputPositionsZ = ALLOC_FLOAT(m_szBodies);
    m_pfInputVelocitiesX = ALLOC_FLOAT(m_szBodies);
    m_pfInputVelocitiesY = ALLOC_FLOAT(m_szBodies);
    m_pfInputVelocitiesZ = ALLOC_FLOAT(m_szBodies);
    m_pfOutputPositionsX = ALLOC_FLOAT(m_szBodies);
    m_pfOutputPositionsY = ALLOC_FLOAT(m_szBodies);
    m_pfOutputPositionsZ = ALLOC_FLOAT(m_szBodies);
    m_pfOutputVelocitiesX = ALLOC_FLOAT(m_szBodies);
    m_pfOutputVelocitiesY = ALLOC_FLOAT(m_szBodies);
    m_pfOutputVelocitiesZ = ALLOC_FLOAT(m_szBodies);
#undef ALLOC_FLOAT

    //set input arrays to random legal values
    srand(2012);
    for (size_t i = 0; i < m_szBodies; i += 1)
    {
        float x = -m_fMaxPosition + 2*m_fMaxPosition*(float)rand()/RAND_MAX;
        float y = -m_fMaxPosition + 2*m_fMaxPosition*(float)rand()/RAND_MAX;
        float z = -0.1f*(m_fMaxPosition + 2*m_fMaxPosition*(float)rand()/RAND_MAX);
        float r = sqrt(x*x+y*y+z*z)/m_fMaxPosition;
        float s = 0.05f/(r+0.2f);

        m_pfMass[i] = m_fMaxMass/(r+0.1f);

        m_pfInputPositionsX[i] = x;
        m_pfInputPositionsY[i] = y;
        m_pfInputPositionsZ[i] = z;

        m_pfInputVelocitiesX[i] = y*s;
        m_pfInputVelocitiesY[i] = -x*s;
        float len = sqrt(m_pfInputVelocitiesX[i]*m_pfInputVelocitiesX[i]+m_pfInputVelocitiesY[i]*m_pfInputVelocitiesY[i]);
        m_pfInputVelocitiesZ[i] = 0.025f*len*(1+2.0f*(float)rand()/RAND_MAX);
        m_pfInputVelocitiesX[i] += 0.025f*len*(1+2.0f*(float)rand()/RAND_MAX);
        m_pfInputVelocitiesY[i] += 0.025f*len*(1+2.0f*(float)rand()/RAND_MAX);

        if(i==0)
        {
            m_pfMass[i]*=1000*m_fMaxMass;
            m_pfInputPositionsX[i] = 0;
            m_pfInputPositionsY[i] = 0;
            m_pfInputPositionsZ[i] = 0;

            m_pfInputVelocitiesX[i] = 0;
            m_pfInputVelocitiesY[i] = 0;
            m_pfInputVelocitiesZ[i] = 0;
        }
    }

    //    create cl_mem buffers
    const cl_mem_flags INFlags = CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR;
    const cl_mem_flags OUTFlags = CL_MEM_WRITE_ONLY | CL_MEM_USE_HOST_PTR;

    m_pfInputPositionsBufferX = clCreateBuffer(context, INFlags, sizeof(cl_float) * m_szBodies,  (void*) m_pfInputPositionsX,  &err);
    m_pfInputPositionsBufferY = clCreateBuffer(context, INFlags, sizeof(cl_float) * m_szBodies,  (void*) m_pfInputPositionsY,  &err);
    m_pfInputPositionsBufferZ = clCreateBuffer(context, INFlags, sizeof(cl_float) * m_szBodies,  (void*) m_pfInputPositionsZ,  &err);
    m_pfInputVelocitiesBufferX = clCreateBuffer(context, INFlags, sizeof(cl_float) * m_szBodies, (void*) m_pfInputVelocitiesX, &err);
    m_pfInputVelocitiesBufferY = clCreateBuffer(context, INFlags, sizeof(cl_float) * m_szBodies, (void*) m_pfInputVelocitiesY, &err);
    m_pfInputVelocitiesBufferZ = clCreateBuffer(context, INFlags, sizeof(cl_float) * m_szBodies, (void*) m_pfInputVelocitiesZ, &err);

    m_pfOutputPositionsBufferX = clCreateBuffer(context, OUTFlags, sizeof(cl_float) * m_szBodies,  (void*) m_pfOutputPositionsX,  &err);
    m_pfOutputPositionsBufferY = clCreateBuffer(context, OUTFlags, sizeof(cl_float) * m_szBodies,  (void*) m_pfOutputPositionsY,  &err);
    m_pfOutputPositionsBufferZ = clCreateBuffer(context, OUTFlags, sizeof(cl_float) * m_szBodies,  (void*) m_pfOutputPositionsZ,  &err);
    m_pfOutputVelocitiesBufferX = clCreateBuffer(context, OUTFlags, sizeof(cl_float) * m_szBodies, (void*) m_pfOutputVelocitiesX, &err);
    m_pfOutputVelocitiesBufferY = clCreateBuffer(context, OUTFlags, sizeof(cl_float) * m_szBodies, (void*) m_pfOutputVelocitiesY, &err);
    m_pfOutputVelocitiesBufferZ = clCreateBuffer(context, OUTFlags, sizeof(cl_float) * m_szBodies, (void*) m_pfOutputVelocitiesZ, &err);
    m_bFlipflop = false;

    m_pfMassBuffer = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(cl_float) * m_szBodies, m_pfMass, &err);

    return err;
}

void iNBody::Cleanup()
{
#define FREE_FLOAT(_ptr) if(_ptr){_mm_free(_ptr);_ptr=NULL;}
    //    free allocated data memory
    FREE_FLOAT(m_pfMass);
    FREE_FLOAT(m_pfInputPositionsX);
    FREE_FLOAT(m_pfInputPositionsY);
    FREE_FLOAT(m_pfInputPositionsZ);
    FREE_FLOAT(m_pfInputVelocitiesX);
    FREE_FLOAT(m_pfInputVelocitiesY);
    FREE_FLOAT(m_pfInputVelocitiesZ);
    FREE_FLOAT(m_pfOutputPositionsX);
    FREE_FLOAT(m_pfOutputPositionsY);
    FREE_FLOAT(m_pfOutputPositionsZ);
    FREE_FLOAT(m_pfOutputVelocitiesX);
    FREE_FLOAT(m_pfOutputVelocitiesY);
    FREE_FLOAT(m_pfOutputVelocitiesZ);
#undef FREE

    //    release cl buffers
    freeCLMem(m_pfMassBuffer);
    freeCLMem(m_pfInputPositionsBufferX);
    freeCLMem(m_pfInputPositionsBufferY);
    freeCLMem(m_pfInputPositionsBufferZ);
    freeCLMem(m_pfInputVelocitiesBufferX);
    freeCLMem(m_pfInputVelocitiesBufferY);
    freeCLMem(m_pfInputVelocitiesBufferZ);
    freeCLMem(m_pfOutputPositionsBufferX);
    freeCLMem(m_pfOutputPositionsBufferY);
    freeCLMem(m_pfOutputPositionsBufferZ);
    freeCLMem(m_pfOutputVelocitiesBufferX);
    freeCLMem(m_pfOutputVelocitiesBufferY);
    freeCLMem(m_pfOutputVelocitiesBufferZ);

    if(kernel)
    {
        clReleaseKernel(kernel);
        kernel = 0;
    }

    if(program)
    {
        clReleaseProgram(program);
        program = 0;
    }
    if(cmd_queue) clFlush(cmd_queue);
    if(cmd_queue)
    {
        clReleaseCommandQueue(cmd_queue);
        cmd_queue = 0;
    }
    if(cmd_queue_second) clFlush(cmd_queue_second);
    if(cmd_queue_second)
    {
        clReleaseCommandQueue(cmd_queue_second);
        cmd_queue_second = 0;
    }
    if(context)
    {
        clReleaseContext(context);
        context = 0;
    }

}

iNBody::iNBody() :
    m_pfMass(NULL),
    m_pfInputPositionsX(NULL),
    m_pfInputPositionsY(NULL),
    m_pfInputPositionsZ(NULL),
    m_pfInputVelocitiesX(NULL),
    m_pfInputVelocitiesY(NULL),
    m_pfInputVelocitiesZ(NULL),
    m_pfOutputPositionsX(NULL),
    m_pfOutputPositionsY(NULL),
    m_pfOutputPositionsZ(NULL),
    m_pfOutputVelocitiesX(NULL),
    m_pfOutputVelocitiesY(NULL),
    m_pfOutputVelocitiesZ(NULL),
    m_szBodies(32768),
    m_fMinMass(10),
    m_fMaxMass(1000),
    m_fMinPosition(1),
    m_fMaxPosition(10000),
    m_fMinVelocity(1),
    m_fMaxVelocity(10),
    m_fEpsilonDistance(0.0001f),
    m_fTimeDelta(0.5),
    m_szGlobalWork(32768),
    m_szLocalWork(16),
    m_pfMassBuffer(0),
    m_pfInputPositionsBufferX(0),
    m_pfInputPositionsBufferY(0),
    m_pfInputPositionsBufferZ(0),
    m_pfInputVelocitiesBufferX(0),
    m_pfInputVelocitiesBufferY(0),
    m_pfInputVelocitiesBufferZ(0),
    m_pfOutputPositionsBufferX(0),
    m_pfOutputPositionsBufferY(0),
    m_pfOutputPositionsBufferZ(0),
    m_pfOutputVelocitiesBufferX(0),
    m_pfOutputVelocitiesBufferY(0),
    m_pfOutputVelocitiesBufferZ(0),
    pfInputPositionsBufferX(0),
    pfInputPositionsBufferY(0),
    pfInputPositionsBufferZ(0),
    pfInputVelocitiesBufferX(0),
    pfInputVelocitiesBufferY(0),
    pfInputVelocitiesBufferZ(0),
    pfOutputPositionsBufferX(0),
    pfOutputPositionsBufferY(0),
    pfOutputPositionsBufferZ(0),
    pfOutputVelocitiesBufferX(0),
    pfOutputVelocitiesBufferY(0),
    pfOutputVelocitiesBufferZ(0),
    m_bValidate(false),
    m_bFlipflop(false),
    m_fSplitting (0.5f),
    kernel(0),
    program(0),
    cmd_queue(0),
    cmd_queue_second(0),
    context(0)
{
    m_szGlobalWork = m_szBodies;
    Setup(CL_DEVICE_TYPE_CPU, false);
}

iNBody::~iNBody()
{
    Cleanup();
}

bool iNBody::Execute_NBody_C_Serial()
{
    for (size_t i = 0; i < m_szGlobalWork; ++i)
    {
        float accelarationX = 0;
        float accelarationY = 0;
        float accelarationZ = 0;
        for (size_t j = 0; j < m_szGlobalWork; ++j)
        {
            if(i==j) continue;

            float vectorX = m_pfInputPositionsX[j] - m_pfInputPositionsX[i];
            float vectorY = m_pfInputPositionsY[j] - m_pfInputPositionsY[i];
            float vectorZ = m_pfInputPositionsZ[j] - m_pfInputPositionsZ[i];
            float distanceInv = __native_rsqrtf(vectorX * vectorX + vectorY * vectorY + vectorZ * vectorZ + m_fEpsilonDistance);
            //float distanceInv = 1.0f/sqrt(vectorX * vectorX + vectorY * vectorY + vectorZ * vectorZ + m_fEpsilonDistance);
            float K = m_pfMass[j]*distanceInv * distanceInv * distanceInv;
            accelarationX += K * vectorX;
            accelarationY += K * vectorY;
            accelarationZ += K * vectorZ;
        }

        m_pfOutputVelocitiesX[i] = m_pfInputVelocitiesX[i] + accelarationX * m_fTimeDelta;
        m_pfOutputVelocitiesY[i] = m_pfInputVelocitiesY[i] + accelarationY * m_fTimeDelta;
        m_pfOutputVelocitiesZ[i] = m_pfInputVelocitiesZ[i] + accelarationZ * m_fTimeDelta;

        m_pfOutputPositionsX[i] = m_pfInputPositionsX[i] + m_pfInputVelocitiesX[i] * m_fTimeDelta + accelarationX * m_fTimeDelta * m_fTimeDelta / 2;
        m_pfOutputPositionsY[i] = m_pfInputPositionsY[i] + m_pfInputVelocitiesY[i] * m_fTimeDelta + accelarationY * m_fTimeDelta * m_fTimeDelta / 2;
        m_pfOutputPositionsZ[i] = m_pfInputPositionsZ[i] + m_pfInputVelocitiesZ[i] * m_fTimeDelta + accelarationZ * m_fTimeDelta * m_fTimeDelta / 2;
    }
    return true;
}

bool iNBody::Execute_NBody_C_Parallel()
{
#pragma omp parallel for schedule(dynamic,1)
    for (int i = 0; i < (int)m_szGlobalWork; ++i)
    {
        float accelarationX = 0;
        float accelarationY = 0;
        float accelarationZ = 0;
        for (size_t j = 0; j < m_szGlobalWork; ++j)
        {
            float vectorX = m_pfInputPositionsX[j] - m_pfInputPositionsX[i];
            float vectorY = m_pfInputPositionsY[j] - m_pfInputPositionsY[i];
            float vectorZ = m_pfInputPositionsZ[j] - m_pfInputPositionsZ[i];
            float distanceInv = __native_rsqrtf(vectorX * vectorX + vectorY * vectorY + vectorZ * vectorZ + m_fEpsilonDistance);
            //float distanceInv = 1.0f/sqrt(vectorX * vectorX + vectorY * vectorY + vectorZ * vectorZ + m_fEpsilonDistance);
            float K = m_pfMass[j]*distanceInv * distanceInv * distanceInv;
            accelarationX += K * vectorX;
            accelarationY += K * vectorY;
            accelarationZ += K * vectorZ;
        }

        m_pfOutputVelocitiesX[i] = m_pfInputVelocitiesX[i] + accelarationX * m_fTimeDelta;
        m_pfOutputVelocitiesY[i] = m_pfInputVelocitiesY[i] + accelarationY * m_fTimeDelta;
        m_pfOutputVelocitiesZ[i] = m_pfInputVelocitiesZ[i] + accelarationZ * m_fTimeDelta;

        m_pfOutputPositionsX[i] = m_pfInputPositionsX[i] + m_pfInputVelocitiesX[i] * m_fTimeDelta + accelarationX * m_fTimeDelta * m_fTimeDelta / 2;
        m_pfOutputPositionsY[i] = m_pfInputPositionsY[i] + m_pfInputVelocitiesY[i] * m_fTimeDelta + accelarationY * m_fTimeDelta * m_fTimeDelta / 2;
        m_pfOutputPositionsZ[i] = m_pfInputPositionsZ[i] + m_pfInputVelocitiesZ[i] * m_fTimeDelta + accelarationZ * m_fTimeDelta * m_fTimeDelta / 2;
    }
    return true;
}

inline bool compareFloats(const cl_float f1, const cl_float f2, const float epsilon)
{
    return fabs(fabs(f1) - fabs(f2)) < epsilon;
}
#define EPSILON 0.0001f
bool iNBody::Execute_NBody_Validate(
                                      const cl_float* pInputVelocitiesX, const cl_float* pInputVelocitiesY, const cl_float* pInputVelocitiesZ,
                                      const cl_float* pInputPositionsX,  const cl_float* pInputPositionsY,  const cl_float* pInputPositionsZ,
                                      const cl_float* pOutputVelocitiesX, const cl_float* pOutputVelocitiesY, const cl_float* pOutputVelocitiesZ,
                                      const cl_float* pOutputPositionsX,  const cl_float* pOutputPositionsY,  const cl_float* pOutputPositionsZ
                                      ) const
{
    //TODO: as a future work we should implement better validation routine
    float maxpos = -FLT_MAX;//for minimum velocity/postion we know that it is ~0 (for the boy in the center)
    float maxvel = -FLT_MAX;

    //error quickly grows with the numbers of bodies (due to division), but (generally) it shouldn't exceed 0.1%; (some particles go well beyound m_fMaxPosition for bodies that fly to the infinity)
    for (int i = 0; i < (int)m_szGlobalWork; ++i)
    {
        maxpos = max(maxpos, max(pInputPositionsX[i],  max(pInputPositionsY[i],  pInputPositionsZ[i])));
        maxvel = max(maxvel, max(pInputVelocitiesX[i], max(pInputVelocitiesY[i], pInputVelocitiesZ[i])));
    }
    const float epsilon_pos = 2*EPSILON*maxpos;// 2 accounts for maximum potential distance between bodies
    const float epsilon_vel = 2*EPSILON*maxvel;// similalry 2 accounts for maximum potential velocity magnitude

    int num_errors = 0;
#pragma omp parallel for schedule(dynamic,1)
    for (int i = 0; i < (int)m_szGlobalWork; ++i)
    {
        float accelarationX = 0;
        float accelarationY = 0;
        float accelarationZ = 0;
        for (size_t j = 0; j < m_szGlobalWork; ++j)
        {
            float vectorX = pInputPositionsX[j] - pInputPositionsX[i];
            float vectorY = pInputPositionsY[j] - pInputPositionsY[i];
            float vectorZ = pInputPositionsZ[j] - pInputPositionsZ[i];
            float distanceInv = __native_rsqrtf(vectorX * vectorX + vectorY * vectorY + vectorZ * vectorZ + m_fEpsilonDistance);
            //float distanceInv = 1.0f/sqrt(vectorX * vectorX + vectorY * vectorY + vectorZ * vectorZ + m_fEpsilonDistance);
            float K = m_pfMass[j]*distanceInv * distanceInv * distanceInv;
            accelarationX += K * vectorX;
            accelarationY += K * vectorY;
            accelarationZ += K * vectorZ;
        }
        const float fOutputVelocitiesX = pInputVelocitiesX[i] + accelarationX * m_fTimeDelta;
        if(!compareFloats(pOutputVelocitiesX[i], fOutputVelocitiesX, epsilon_vel))
            {/*printf("inaccurate vel value for body#%d\n", i);*/num_errors++;}
        const float fOutputVelocitiesY = pInputVelocitiesY[i] + accelarationY * m_fTimeDelta;
        if(!compareFloats(pOutputVelocitiesY[i], fOutputVelocitiesY, epsilon_vel))
            {/*printf("inaccurate vel value for body#%d\n", i);*/num_errors++;}
        const float fOutputVelocitiesZ = pInputVelocitiesZ[i] + accelarationZ * m_fTimeDelta;
        if(!compareFloats(pOutputVelocitiesZ[i], fOutputVelocitiesZ, epsilon_vel))
            {/*printf("inaccurate vel value for body#%d\n", i);*/num_errors++;}

        const float fOutputPositionsX = pInputPositionsX[i] + pInputVelocitiesX[i] * m_fTimeDelta + accelarationX * m_fTimeDelta * m_fTimeDelta / 2;
        if(!compareFloats(pOutputPositionsX[i], fOutputPositionsX, epsilon_pos))
            {/*printf("inaccurate pos value for body#%d\n", i);*/num_errors++;}
        const float fOutputPositionsY = pInputPositionsY[i] + pInputVelocitiesY[i] * m_fTimeDelta + accelarationY * m_fTimeDelta * m_fTimeDelta / 2;
        if(!compareFloats(pOutputPositionsY[i], fOutputPositionsY, epsilon_pos))
            {/*printf("inaccurate pos value for body#%d\n", i);*/num_errors++;}
        const float fOutputPositionsZ = pInputPositionsZ[i] + pInputVelocitiesZ[i] * m_fTimeDelta + accelarationZ * m_fTimeDelta * m_fTimeDelta / 2;
        if(!compareFloats(pOutputPositionsZ[i], fOutputPositionsZ, epsilon_pos))
            {/*printf("inaccurate pos value for body#%d\n", i);*/num_errors++;}
    }
    if(num_errors)
        printf("%d inaccurate values (%.2f%%)\n", num_errors, 100*float(num_errors)/(6*m_szGlobalWork) /*six values being tracked per body*/);
    return num_errors==0;
}


bool iNBody::Execute_NBody_SSE_Parallel()
{
#pragma omp parallel for schedule(dynamic,1)
    for (int i = 0; i < (int)m_szGlobalWork; ++i)
    {
        __m128 posX  = _mm_set1_ps(m_pfInputPositionsX[i]);
        __m128 posY  = _mm_set1_ps(m_pfInputPositionsY[i]);
        __m128 posZ  = _mm_set1_ps(m_pfInputPositionsZ[i]);
        __m128 accX =  _mm_setzero_ps();
        __m128 accY =  _mm_setzero_ps();
        __m128 accZ =  _mm_setzero_ps();
        for (size_t j = 0; j < m_szGlobalWork; j+=4)
        {
            __m128 mass = _mm_load_ps(m_pfMass+j);
            __m128 posXCur = _mm_load_ps(m_pfInputPositionsX+j);
            __m128 posYCur = _mm_load_ps(m_pfInputPositionsY+j);
            __m128 posZCur = _mm_load_ps(m_pfInputPositionsZ+j);

            __m128 vectorX =  _mm_sub_ps(posXCur,posX);
            __m128 vectorY =  _mm_sub_ps(posYCur,posY);
            __m128 vectorZ =  _mm_sub_ps(posZCur,posZ);
            __m128 dist = _mm_set1_ps(m_fEpsilonDistance);
             dist = _mm_add_ps(dist,_mm_mul_ps(vectorX,vectorX));
             dist = _mm_add_ps(dist,_mm_mul_ps(vectorY,vectorY));
             dist = _mm_add_ps(dist,_mm_mul_ps(vectorZ,vectorZ));
            __m128 distInv =  __native_rsqrtf4(dist);
            __m128 K = _mm_mul_ps(_mm_mul_ps(mass,distInv),_mm_mul_ps(distInv,distInv));
            accX = _mm_add_ps(accX,_mm_mul_ps(vectorX,K));
            accY = _mm_add_ps(accY,_mm_mul_ps(vectorY,K));
            accZ = _mm_add_ps(accZ,_mm_mul_ps(vectorZ,K));
        }

        accX = _mm_hadd_ps(accX,accX);
        accX = _mm_hadd_ps(accX,accX);
        accY = _mm_hadd_ps(accY,accY);
        accY = _mm_hadd_ps(accY,accY);
        accZ = _mm_hadd_ps(accZ,accZ);
        accZ = _mm_hadd_ps(accZ,accZ);

        float   accelarationX =  _mm_cvtss_f32(accX);
        float   accelarationY =  _mm_cvtss_f32(accY);
        float   accelarationZ =  _mm_cvtss_f32(accZ);

        m_pfOutputVelocitiesX[i] = m_pfInputVelocitiesX[i] + accelarationX * m_fTimeDelta;
        m_pfOutputVelocitiesY[i] = m_pfInputVelocitiesY[i] + accelarationY * m_fTimeDelta;
        m_pfOutputVelocitiesZ[i] = m_pfInputVelocitiesZ[i] + accelarationZ * m_fTimeDelta;

        m_pfOutputPositionsX[i] = m_pfInputPositionsX[i] + m_pfInputVelocitiesX[i] * m_fTimeDelta + accelarationX * m_fTimeDelta * m_fTimeDelta / 2;
        m_pfOutputPositionsY[i] = m_pfInputPositionsY[i] + m_pfInputVelocitiesY[i] * m_fTimeDelta + accelarationY * m_fTimeDelta * m_fTimeDelta / 2;
        m_pfOutputPositionsZ[i] = m_pfInputPositionsZ[i] + m_pfInputVelocitiesZ[i] * m_fTimeDelta + accelarationZ * m_fTimeDelta * m_fTimeDelta / 2;
    }
    return true;
}


bool iNBody::ExecuteNBodyKernel( const size_t szLocalWork, bool bManual )
{
    int szElementsPerItem = 1;
    cl_int err = CL_SUCCESS;

    //for multi device execution
    cl_event events_list[2];
    size_t szGlobalWork1;
    size_t szGlobalWork2;
    const int no_offset = 0;

    cl_ulong start = 0;
    cl_ulong end = 0;

    pfInputPositionsBufferX  = m_pfInputPositionsBufferX  ;
    pfInputPositionsBufferY  = m_pfInputPositionsBufferY  ;
    pfInputPositionsBufferZ  = m_pfInputPositionsBufferZ  ;

    pfInputVelocitiesBufferX = m_pfInputVelocitiesBufferX ;
    pfInputVelocitiesBufferY = m_pfInputVelocitiesBufferY ;
    pfInputVelocitiesBufferZ = m_pfInputVelocitiesBufferZ ;

    pfOutputPositionsBufferX = m_pfOutputPositionsBufferX ;
    pfOutputPositionsBufferY = m_pfOutputPositionsBufferY ;
    pfOutputPositionsBufferZ = m_pfOutputPositionsBufferZ ;

    pfOutputVelocitiesBufferX =m_pfOutputVelocitiesBufferX ;
    pfOutputVelocitiesBufferY =m_pfOutputVelocitiesBufferY ;
    pfOutputVelocitiesBufferZ =m_pfOutputVelocitiesBufferZ ;

    if(m_bFlipflop)
    {
        pfInputPositionsBufferX  = m_pfOutputPositionsBufferX ;
        pfInputPositionsBufferY  = m_pfOutputPositionsBufferY ;
        pfInputPositionsBufferZ  = m_pfOutputPositionsBufferZ ;

        pfInputVelocitiesBufferX = m_pfOutputVelocitiesBufferX;
        pfInputVelocitiesBufferY = m_pfOutputVelocitiesBufferY;
        pfInputVelocitiesBufferZ = m_pfOutputVelocitiesBufferZ;

        pfOutputPositionsBufferX = m_pfInputPositionsBufferX  ;
        pfOutputPositionsBufferY = m_pfInputPositionsBufferY  ;
        pfOutputPositionsBufferZ = m_pfInputPositionsBufferZ  ;

        pfOutputVelocitiesBufferX = m_pfInputVelocitiesBufferX ;
        pfOutputVelocitiesBufferY = m_pfInputVelocitiesBufferY ;
        pfOutputVelocitiesBufferZ = m_pfInputVelocitiesBufferZ ;
    }
    m_bFlipflop=!m_bFlipflop;

    if(m_deviceType != CL_DEVICE_TYPE_ALL)
    {
        //single device version
        //    Set aruments for kernel kernel
        err = clSetKernelArg( kernel, 3, sizeof(cl_mem), (void*) &m_pfMassBuffer);
        err |= clSetKernelArg( kernel, 0, sizeof(cl_mem), (void*) &pfInputPositionsBufferX);
        err |= clSetKernelArg( kernel, 1, sizeof(cl_mem), (void*) &pfInputPositionsBufferY);
        err |= clSetKernelArg( kernel, 2, sizeof(cl_mem), (void*) &pfInputPositionsBufferZ);

        err |= clSetKernelArg( kernel, 4, sizeof(cl_mem), (void*) &pfInputVelocitiesBufferX);
        err |= clSetKernelArg( kernel, 5, sizeof(cl_mem), (void*) &pfInputVelocitiesBufferY);
        err |= clSetKernelArg( kernel, 6, sizeof(cl_mem), (void*) &pfInputVelocitiesBufferZ);

        err |= clSetKernelArg( kernel, 7, sizeof(cl_mem), (void*) &pfOutputPositionsBufferX);
        err |= clSetKernelArg( kernel, 8, sizeof(cl_mem), (void*) &pfOutputPositionsBufferY);
        err |= clSetKernelArg( kernel, 9, sizeof(cl_mem), (void*) &pfOutputPositionsBufferZ);
        err |= clSetKernelArg( kernel, 10, sizeof(cl_mem), (void*) &pfOutputVelocitiesBufferX);
        err |= clSetKernelArg( kernel, 11, sizeof(cl_mem), (void*) &pfOutputVelocitiesBufferY);
        err |= clSetKernelArg( kernel, 12, sizeof(cl_mem), (void*) &pfOutputVelocitiesBufferZ);

        err |= clSetKernelArg( kernel, 13, sizeof(float), (void*) &m_fEpsilonDistance);
        err |= clSetKernelArg( kernel, 14, sizeof(float), (void*) &m_fTimeDelta);
        err |= clSetKernelArg( kernel, 15, sizeof(float), (void*) &no_offset);
        err |= clSetKernelArg( kernel, 16, sizeof(float), (void*) &m_szGlobalWork);

        if( CL_SUCCESS == err )
        {
            //    execute kernel on device
            err = clEnqueueNDRangeKernel(cmd_queue, kernel, 1, NULL, &m_szGlobalWork, &szLocalWork, 0, NULL, NULL);
            if( CL_SUCCESS == err )
            {
                err = clFinish(cmd_queue);
            }
            else
            {
                WCHAR sz[100];
                swprintf( sz, 100, L"Error code is %d %s", err, err==-55 ? L"Invalid workgroup size\n" : L"");
                ::MessageBox(0,sz, L"Error executing kernel", MB_ICONERROR);
            }
        }

        return CL_SUCCESS == err;
    }
    else
    {
        //multi device execution
        //divide on regions
        //compute splitting ratio for buffers
        if(bManual)
            SplitRange(m_fSplitting,m_szGlobalWork, &szGlobalWork1, &szGlobalWork2, szLocalWork);
        else
            m_fSplitting = ComputeSplittingRatio(m_szGlobalWork, &szGlobalWork1, &szGlobalWork2, szLocalWork);

        //printf("Global work size device#1    %d\n", szGlobalWork1);
        //printf("Global work size device#2    %d\n", szGlobalWork2);
        cl_buffer_region BufferRegion = { 0,  sizeof(cl_float) * szGlobalWork1 };
        cl_buffer_region BufferRegionPG = { sizeof(cl_float) * szGlobalWork1, sizeof(cl_float) * szGlobalWork2 };

        cl_mem pfOutputPositionsSubBufferX;
        cl_mem pfOutputPositionsSubBufferPGX;
        cl_mem pfOutputPositionsSubBufferY;
        cl_mem pfOutputPositionsSubBufferPGY;
        cl_mem pfOutputPositionsSubBufferZ;
        cl_mem pfOutputPositionsSubBufferPGZ;

        cl_mem pfOutputVelocitiesSubBufferX;
        cl_mem pfOutputVelocitiesSubBufferPGX;
        cl_mem pfOutputVelocitiesSubBufferY;
        cl_mem pfOutputVelocitiesSubBufferPGY;
        cl_mem pfOutputVelocitiesSubBufferZ;
        cl_mem pfOutputVelocitiesSubBufferPGZ;

        pfOutputPositionsSubBufferX = clCreateSubBuffer(pfOutputPositionsBufferX, 0, CL_BUFFER_CREATE_TYPE_REGION, &BufferRegion, &err);
        pfOutputPositionsSubBufferPGX = clCreateSubBuffer(pfOutputPositionsBufferX, 0, CL_BUFFER_CREATE_TYPE_REGION, &BufferRegionPG, &err);
        pfOutputPositionsSubBufferY = clCreateSubBuffer(pfOutputPositionsBufferY, 0, CL_BUFFER_CREATE_TYPE_REGION, &BufferRegion, &err);
        pfOutputPositionsSubBufferPGY = clCreateSubBuffer(pfOutputPositionsBufferY, 0, CL_BUFFER_CREATE_TYPE_REGION, &BufferRegionPG, &err);
        pfOutputPositionsSubBufferZ = clCreateSubBuffer(pfOutputPositionsBufferZ, 0, CL_BUFFER_CREATE_TYPE_REGION, &BufferRegion, &err);
        pfOutputPositionsSubBufferPGZ = clCreateSubBuffer(pfOutputPositionsBufferZ, 0, CL_BUFFER_CREATE_TYPE_REGION, &BufferRegionPG, &err);

        pfOutputVelocitiesSubBufferX = clCreateSubBuffer(pfOutputVelocitiesBufferX, 0, CL_BUFFER_CREATE_TYPE_REGION, &BufferRegion, &err);
        pfOutputVelocitiesSubBufferPGX = clCreateSubBuffer(pfOutputVelocitiesBufferX, 0, CL_BUFFER_CREATE_TYPE_REGION, &BufferRegionPG, &err);
        pfOutputVelocitiesSubBufferY = clCreateSubBuffer(pfOutputVelocitiesBufferY, 0, CL_BUFFER_CREATE_TYPE_REGION, &BufferRegion, &err);
        pfOutputVelocitiesSubBufferPGY = clCreateSubBuffer(pfOutputVelocitiesBufferY, 0, CL_BUFFER_CREATE_TYPE_REGION, &BufferRegionPG, &err);
        pfOutputVelocitiesSubBufferZ = clCreateSubBuffer(pfOutputVelocitiesBufferZ, 0, CL_BUFFER_CREATE_TYPE_REGION, &BufferRegion, &err);
        pfOutputVelocitiesSubBufferPGZ = clCreateSubBuffer(pfOutputVelocitiesBufferZ, 0, CL_BUFFER_CREATE_TYPE_REGION, &BufferRegionPG, &err);

        //first device (CPU)
        //    Set aruments for the  kernel
        //scalar kernel is best on CPU (automatically promoted to AVX on SNB+),
        err = clSetKernelArg( kernel, 0, sizeof(cl_mem), (void*) &pfInputPositionsBufferX);
        err |= clSetKernelArg( kernel, 1, sizeof(cl_mem), (void*) &pfInputPositionsBufferY);
        err |= clSetKernelArg( kernel, 2, sizeof(cl_mem), (void*) &pfInputPositionsBufferZ);
        err |= clSetKernelArg( kernel, 3, sizeof(cl_mem), (void*) &m_pfMassBuffer);

        err |= clSetKernelArg( kernel, 4, sizeof(cl_mem), (void*) &pfInputVelocitiesBufferX);
        err |= clSetKernelArg( kernel, 5, sizeof(cl_mem), (void*) &pfInputVelocitiesBufferY);
        err |= clSetKernelArg( kernel, 6, sizeof(cl_mem), (void*) &pfInputVelocitiesBufferZ);

        err |= clSetKernelArg( kernel, 7, sizeof(cl_mem), (void*) &pfOutputPositionsSubBufferX);
        err |= clSetKernelArg( kernel, 8, sizeof(cl_mem), (void*) &pfOutputPositionsSubBufferY);
        err |= clSetKernelArg( kernel, 9, sizeof(cl_mem), (void*) &pfOutputPositionsSubBufferZ);
        err |= clSetKernelArg( kernel, 10, sizeof(cl_mem), (void*) &pfOutputVelocitiesSubBufferX);
        err |= clSetKernelArg( kernel, 11, sizeof(cl_mem), (void*) &pfOutputVelocitiesSubBufferY);
        err |= clSetKernelArg( kernel, 12, sizeof(cl_mem), (void*) &pfOutputVelocitiesSubBufferZ);

        err |= clSetKernelArg( kernel, 13, sizeof(float), (void*) &m_fEpsilonDistance);
        err |= clSetKernelArg( kernel, 14, sizeof(float), (void*) &m_fTimeDelta);
        err |= clSetKernelArg( kernel, 15, sizeof(float), (void*) &no_offset);
        err |= clSetKernelArg( kernel, 16, sizeof(float), (void*) &m_szGlobalWork);

        if( CL_SUCCESS == err )
        {
            //    execute kernel on device
            err = clEnqueueNDRangeKernel(cmd_queue, kernel, 1, NULL, &szGlobalWork1, &szLocalWork, 0, NULL, &events_list[0]);
            if( CL_SUCCESS != err )
            {
                WCHAR sz[100];
                swprintf( sz, 100, L"Error code is %d %s", err, err==-55 ? L"Invalid workgroup size\n" : L"");
                ::MessageBox(0,sz, L"Error executing kernel on CPU", MB_ICONERROR);
            }
        }
        else
        {
            return false;
        }

        //second device (GPU)
        //    Set aruments for the kernel
        err |= clSetKernelArg( kernel, 0, sizeof(cl_mem), (void*) &pfInputPositionsBufferX);
        err |= clSetKernelArg( kernel, 1, sizeof(cl_mem), (void*) &pfInputPositionsBufferY);
        err |= clSetKernelArg( kernel, 2, sizeof(cl_mem), (void*) &pfInputPositionsBufferZ);
        err |= clSetKernelArg( kernel, 3, sizeof(cl_mem), (void*) &m_pfMassBuffer);

        err |= clSetKernelArg( kernel, 4, sizeof(cl_mem), (void*) &pfInputVelocitiesBufferX);
        err |= clSetKernelArg( kernel, 5, sizeof(cl_mem), (void*) &pfInputVelocitiesBufferY);
        err |= clSetKernelArg( kernel, 6, sizeof(cl_mem), (void*) &pfInputVelocitiesBufferZ);

        err |= clSetKernelArg( kernel, 7, sizeof(cl_mem), (void*) &pfOutputPositionsSubBufferPGX);
        err |= clSetKernelArg( kernel, 8, sizeof(cl_mem), (void*) &pfOutputPositionsSubBufferPGY);
        err |= clSetKernelArg( kernel, 9, sizeof(cl_mem), (void*) &pfOutputPositionsSubBufferPGZ);
        err |= clSetKernelArg( kernel, 10, sizeof(cl_mem), (void*) &pfOutputVelocitiesSubBufferPGX);
        err |= clSetKernelArg( kernel, 11, sizeof(cl_mem), (void*) &pfOutputVelocitiesSubBufferPGY);
        err |= clSetKernelArg( kernel, 12, sizeof(cl_mem), (void*) &pfOutputVelocitiesSubBufferPGZ);

        err |= clSetKernelArg( kernel, 13, sizeof(float), (void*) &m_fEpsilonDistance);
        err |= clSetKernelArg( kernel, 14, sizeof(float), (void*) &m_fTimeDelta);
        err |= clSetKernelArg( kernel, 15, sizeof(float), (void*) &szGlobalWork1);
        err |= clSetKernelArg( kernel, 16, sizeof(float), (void*) &m_szGlobalWork);
        if( CL_SUCCESS == err )
        {
            //    execute kernel on device
            err = clEnqueueNDRangeKernel(cmd_queue_second, kernel, 1, &szGlobalWork1, &szGlobalWork2, &szLocalWork, 0, NULL, &events_list[1]);
            if( CL_SUCCESS != err )
            {
                WCHAR sz[100];
                swprintf( sz, 100, L"Error code is %d %s", err, err==-55 ? L"Invalid workgroup size\n" : L"");
                ::MessageBox(0,sz, L"Error executing kernel on GPU", MB_RETRYCANCEL);
            }
        }
        else
        {
            return false;
        }

        err |= clFlush(cmd_queue_second);//let's flush GPU's queue first (before CPU device occupies all the cores with its commands)
        err |= clFlush(cmd_queue);
        //now let's wait
        err |= clWaitForEvents (2, events_list);
        if (err != CL_SUCCESS)
        {
            printf("ERROR: Failure in clWaitForEvents ...\n");
            return false;
        }

        //update times
        //CPU time

        err = clGetEventProfilingInfo(events_list[0], CL_PROFILING_COMMAND_START, sizeof(cl_ulong), &start, NULL);
        if (err != CL_SUCCESS)
        {
            printf("ERROR: Failed to get clGetEventProfilingInfo CL_PROFILING_COMMAND_START...\n");
            return false;
        }
        err = clGetEventProfilingInfo(events_list[0], CL_PROFILING_COMMAND_END, sizeof(cl_ulong), &end, NULL);
        if (err != CL_SUCCESS)
        {
            printf("ERROR: Failed to get clGetEventProfilingInfo CL_PROFILING_COMMAND_END...\n");
            return false;
        }
        g_NDRangeTime1 = (cl_double)(end - start)*(cl_double)(1e-06);

        err = clGetEventProfilingInfo(events_list[1], CL_PROFILING_COMMAND_START, sizeof(cl_ulong), &start, NULL);
        if (err != CL_SUCCESS)
        {
            printf("ERROR: Failed to get clGetEventProfilingInfo CL_PROFILING_COMMAND_START...\n");
            return false;
        }
        err = clGetEventProfilingInfo(events_list[1], CL_PROFILING_COMMAND_END, sizeof(cl_ulong), &end, NULL);
        if (err != CL_SUCCESS)
        {
            printf("ERROR: Failed to get clGetEventProfilingInfo CL_PROFILING_COMMAND_END...\n");
            return false;
        }
        g_NDRangeTime2 = (cl_double)(end - start)*(cl_double)(1e-06);
        //printf("CPU %.3f GPU %.3f\n", g_NDRangeTime1, g_NDRangeTime2);
        err  = clReleaseEvent(events_list[0]);
        err |= clReleaseEvent(events_list[1]);
        if (err != CL_SUCCESS)
        {
            printf("ERROR: Could not release events\n");
            return false;
        }

        //doing map/unmap to sync the memory content with the host mem pointed by outputArray (this is required by spec)
        void* tmp_ptr = NULL;
        tmp_ptr = clEnqueueMapBuffer(cmd_queue, m_pfOutputPositionsBufferX, CL_TRUE, CL_MAP_READ, 0, sizeof(cl_float) * m_szBodies , 0, NULL, NULL, NULL);
        if(tmp_ptr!=m_pfOutputPositionsX)
        {
            printf("ERROR: clEnqueueMapBuffer failed to return original pointer\n");//since we used CL_USE_HOST_PTR we want to operate on the same mem not copy
            return false;
        }
        clEnqueueUnmapMemObject(cmd_queue, m_pfOutputPositionsBufferX, tmp_ptr, 0, NULL, NULL);
        tmp_ptr = clEnqueueMapBuffer(cmd_queue, m_pfOutputPositionsBufferY, CL_TRUE, CL_MAP_READ, 0, sizeof(cl_float) * m_szBodies , 0, NULL, NULL, NULL);
        if(tmp_ptr!=m_pfOutputPositionsY)
        {
            printf("ERROR: clEnqueueMapBuffer failed to return original pointer\n");//since we used CL_USE_HOST_PTR we want to operate on the same mem not copy
            return false;
        }
        clEnqueueUnmapMemObject(cmd_queue, m_pfOutputPositionsBufferY, tmp_ptr, 0, NULL, NULL);
        tmp_ptr = clEnqueueMapBuffer(cmd_queue, m_pfOutputPositionsBufferZ, CL_TRUE, CL_MAP_READ, 0, sizeof(cl_float) * m_szBodies , 0, NULL, NULL, NULL);
        if(tmp_ptr!=m_pfOutputPositionsZ)
        {
            printf("ERROR: clEnqueueMapBuffer failed to return original pointer\n");//since we used CL_USE_HOST_PTR we want to operate on the same mem not copy
            return false;
        }
        clEnqueueUnmapMemObject(cmd_queue, m_pfOutputPositionsBufferZ, tmp_ptr, 0, NULL, NULL);

        tmp_ptr = clEnqueueMapBuffer(cmd_queue, m_pfOutputVelocitiesBufferX, CL_TRUE, CL_MAP_READ, 0, sizeof(cl_float) * m_szBodies , 0, NULL, NULL, NULL);
        if(tmp_ptr!=m_pfOutputVelocitiesX)
        {
            printf("ERROR: clEnqueueMapBuffer failed to return original pointer\n");//since we used CL_USE_HOST_PTR we want to operate on the same mem not copy
            return false;
        }
        clEnqueueUnmapMemObject(cmd_queue, m_pfOutputVelocitiesBufferX, tmp_ptr, 0, NULL, NULL);
        tmp_ptr = clEnqueueMapBuffer(cmd_queue, m_pfOutputVelocitiesBufferY, CL_TRUE, CL_MAP_READ, 0, sizeof(cl_float) * m_szBodies , 0, NULL, NULL, NULL);
        if(tmp_ptr!=m_pfOutputVelocitiesY)
        {
            printf("ERROR: clEnqueueMapBuffer failed to return original pointer\n");//since we used CL_USE_HOST_PTR we want to operate on the same mem not copy
            return false;
        }
        clEnqueueUnmapMemObject(cmd_queue, m_pfOutputVelocitiesBufferY, tmp_ptr, 0, NULL, NULL);
        tmp_ptr = clEnqueueMapBuffer(cmd_queue, m_pfOutputVelocitiesBufferZ, CL_TRUE, CL_MAP_READ, 0, sizeof(cl_float) * m_szBodies , 0, NULL, NULL, NULL);
        if(tmp_ptr!=m_pfOutputVelocitiesZ)
        {
            printf("ERROR: clEnqueueMapBuffer failed to return original pointer\n");//since we used CL_USE_HOST_PTR we want to operate on the same mem not copy
            return false;
        }
        clEnqueueUnmapMemObject(cmd_queue, m_pfOutputVelocitiesBufferZ, tmp_ptr, 0, NULL, NULL);

        freeCLMem(pfOutputPositionsSubBufferX);
        freeCLMem( pfOutputPositionsSubBufferPGX);
        freeCLMem(pfOutputPositionsSubBufferY);
        freeCLMem(pfOutputPositionsSubBufferPGY);
        freeCLMem(pfOutputPositionsSubBufferZ);
        freeCLMem(pfOutputPositionsSubBufferPGZ);

        freeCLMem(pfOutputVelocitiesSubBufferX);
        freeCLMem(pfOutputVelocitiesSubBufferPGX);
        freeCLMem(pfOutputVelocitiesSubBufferY);
        freeCLMem(pfOutputVelocitiesSubBufferPGY);
        freeCLMem(pfOutputVelocitiesSubBufferZ);
        freeCLMem(pfOutputVelocitiesSubBufferPGZ);
        return true;
    }
}

bool iNBody::Execute( int sz, int wg_sz, int optimization, PARTICLES& particle_data, bool bManual, int nColorSplitMode )
{
    int rmn = sz & (wg_sz-1);
    m_szGlobalWork = sz - rmn;
    assert(0==(m_szGlobalWork%wg_sz));

    if(m_szGlobalWork > (size_t)m_szBodies)
        m_szGlobalWork = m_szBodies;

    switch( optimization )
    {
    case 0:
        Execute_NBody_C_Serial();
        break;
    case 1:
        Execute_NBody_C_Parallel();
        break;
    case 2:
        Execute_NBody_SSE_Parallel();
        break;
    case 3:
        {
            bool res = ExecuteNBodyKernel( wg_sz, bManual);
            assert(res);
            break;
        }
    default:
        assert(false);
        break;
    }

    if(optimization>2)//OCL
    {
        //copy velocities for new iteration
        cl_int err;
        err =clEnqueueReadBuffer(cmd_queue, pfInputPositionsBufferX, CL_TRUE, 0, sizeof(float)*m_szGlobalWork, particle_data.PosX,0, NULL,NULL);
        err|=clEnqueueReadBuffer(cmd_queue, pfInputPositionsBufferY, CL_TRUE, 0, sizeof(float)*m_szGlobalWork, particle_data.PosY,0, NULL,NULL);
        err|=clEnqueueReadBuffer(cmd_queue, pfInputPositionsBufferZ, CL_TRUE, 0, sizeof(float)*m_szGlobalWork, particle_data.PosZ,0, NULL,NULL);
        if (err != CL_SUCCESS)
        {
            printf("ERROR: Failure in clEnqueueReadBuffer ...\n");
            return false;
        }

        if(m_bValidate)
        {
            //a little bit inefficient, but ok for validation mode
            void* ptr0=clEnqueueMapBuffer(cmd_queue, pfInputVelocitiesBufferX, CL_TRUE, CL_MAP_READ,0, sizeof(float)*m_szGlobalWork,NULL,NULL,NULL,&err);
            void* ptr1=clEnqueueMapBuffer(cmd_queue, pfInputVelocitiesBufferY, CL_TRUE, CL_MAP_READ,0, sizeof(float)*m_szGlobalWork,NULL,NULL,NULL,&err);
            void* ptr2=clEnqueueMapBuffer(cmd_queue, pfInputVelocitiesBufferZ, CL_TRUE, CL_MAP_READ,0, sizeof(float)*m_szGlobalWork,NULL,NULL,NULL,&err);
            void* ptr3=clEnqueueMapBuffer(cmd_queue, pfInputPositionsBufferX,  CL_TRUE, CL_MAP_READ,0, sizeof(float)*m_szGlobalWork,NULL,NULL,NULL,&err);
            void* ptr4=clEnqueueMapBuffer(cmd_queue, pfInputPositionsBufferY,  CL_TRUE, CL_MAP_READ, 0, sizeof(float)*m_szGlobalWork,NULL,NULL,NULL,&err);
            void* ptr5=clEnqueueMapBuffer(cmd_queue, pfInputPositionsBufferZ,  CL_TRUE, CL_MAP_READ,0, sizeof(float)*m_szGlobalWork,NULL,NULL,NULL,&err);

            void* ptr6=clEnqueueMapBuffer(cmd_queue, pfOutputVelocitiesBufferX, CL_TRUE, CL_MAP_READ,0, sizeof(float)*m_szGlobalWork,NULL,NULL,NULL,&err);
            void* ptr7=clEnqueueMapBuffer(cmd_queue, pfOutputVelocitiesBufferY, CL_TRUE, CL_MAP_READ,0, sizeof(float)*m_szGlobalWork,NULL,NULL,NULL,&err);
            void* ptr8=clEnqueueMapBuffer(cmd_queue, pfOutputVelocitiesBufferZ, CL_TRUE, CL_MAP_READ,0, sizeof(float)*m_szGlobalWork,NULL,NULL,NULL,&err);
            void* ptr9=clEnqueueMapBuffer(cmd_queue, pfOutputPositionsBufferX,  CL_TRUE, CL_MAP_READ,0, sizeof(float)*m_szGlobalWork,NULL,NULL,NULL,&err);
            void* ptr10=clEnqueueMapBuffer(cmd_queue, pfOutputPositionsBufferY, CL_TRUE, CL_MAP_READ,0, sizeof(float)*m_szGlobalWork,NULL,NULL,NULL,&err);
            void* ptr11=clEnqueueMapBuffer(cmd_queue, pfOutputPositionsBufferZ, CL_TRUE, CL_MAP_READ,0, sizeof(float)*m_szGlobalWork,NULL,NULL,NULL,&err);

            bool res = Execute_NBody_Validate((cl_float*) ptr0, (cl_float*) ptr1, (cl_float*) ptr2, (cl_float*) ptr3, (cl_float*) ptr4, (cl_float*) ptr5, (cl_float*) ptr6, (cl_float*) ptr7, (cl_float*) ptr8, (cl_float*) ptr9, (cl_float*) ptr10, (cl_float*) ptr11);

            clEnqueueUnmapMemObject(cmd_queue, pfInputVelocitiesBufferX,  ptr0, 0, NULL, NULL); clEnqueueUnmapMemObject(cmd_queue, pfInputVelocitiesBufferY,ptr1, 0, NULL, NULL);
            clEnqueueUnmapMemObject(cmd_queue, pfInputVelocitiesBufferZ,  ptr2, 0, NULL, NULL); clEnqueueUnmapMemObject(cmd_queue, pfInputPositionsBufferX, ptr3, 0, NULL, NULL);
            clEnqueueUnmapMemObject(cmd_queue, pfInputPositionsBufferY,   ptr4, 0, NULL, NULL); clEnqueueUnmapMemObject(cmd_queue,  pfInputPositionsBufferZ,ptr5, 0, NULL, NULL);

            clEnqueueUnmapMemObject(cmd_queue, pfOutputVelocitiesBufferX,ptr6, 0, NULL, NULL); clEnqueueUnmapMemObject(cmd_queue, pfOutputVelocitiesBufferY,ptr7, 0, NULL, NULL);
            clEnqueueUnmapMemObject(cmd_queue, pfOutputVelocitiesBufferZ,ptr8, 0, NULL, NULL); clEnqueueUnmapMemObject(cmd_queue, pfOutputPositionsBufferX, ptr9, 0, NULL, NULL);
            clEnqueueUnmapMemObject(cmd_queue, pfOutputPositionsBufferY, ptr10, 0, NULL, NULL); clEnqueueUnmapMemObject(cmd_queue, pfOutputPositionsBufferZ,ptr11, 0, NULL, NULL);
            if(!res)
            {
                printf("validation failed!!!\n\n");
                return false;
            }
        }
    }
    else
    {
        memcpy(m_pfInputVelocitiesX, m_pfOutputVelocitiesX, sizeof(cl_float) * m_szGlobalWork);
        memcpy(m_pfInputVelocitiesY, m_pfOutputVelocitiesY, sizeof(cl_float) * m_szGlobalWork);
        memcpy(m_pfInputVelocitiesZ, m_pfOutputVelocitiesZ, sizeof(cl_float) * m_szGlobalWork);
        memcpy(m_pfInputPositionsX, m_pfOutputPositionsX, sizeof(cl_float) * m_szGlobalWork);
        memcpy(m_pfInputPositionsY, m_pfOutputPositionsY, sizeof(cl_float) * m_szGlobalWork);
        memcpy(m_pfInputPositionsZ, m_pfOutputPositionsZ, sizeof(cl_float) * m_szGlobalWork);
        memcpy(particle_data.PosX, m_pfOutputPositionsX, sizeof(cl_float) * m_szGlobalWork);
        memcpy(particle_data.PosY, m_pfOutputPositionsY, sizeof(cl_float) * m_szGlobalWork);
        memcpy(particle_data.PosZ, m_pfOutputPositionsZ, sizeof(cl_float) * m_szGlobalWork);
    }
    memcpy(particle_data.Mass, m_pfMass, sizeof(cl_float) * m_szGlobalWork);

    if(nColorSplitMode)
    {
        if(m_deviceType != CL_DEVICE_TYPE_ALL || optimization<3)
        {

            memset(particle_data.ColorCode, m_deviceType == CL_DEVICE_TYPE_CPU || optimization<3, m_szGlobalWork*sizeof(float));//all are of first color (CPU) or of second (GPU)
        }
        else
        {
            memset(particle_data.ColorCode, 1, (int)(m_fSplitting*((float)m_szGlobalWork))*sizeof(float));
            memset(particle_data.ColorCode + (int)(m_fSplitting*m_szGlobalWork), 0, (int)((1-m_fSplitting)*(float)(m_szGlobalWork))*sizeof(float));
        }
    }
    else for (size_t i = 0; i < m_szGlobalWork; ++i)//linear colorcode for random color values
    {
        particle_data.ColorCode[i]= float(i)/m_szGlobalWork;/*(m_pfOutputVelocitiesX[i] - fInputVelocitiesXMin)/(fInputVelocitiesXMax- fInputVelocitiesXMin)*/;
    }

    return true;
}
