#pragma once

// ========================================
// Job System 사용 예시
// ========================================

/*

// 1. 간단한 Job 실행

void MyJobFunction(void* data)
{
    int* value = static_cast<int*>(data);
    *value = *value * 2;
}

int main()
{
    JobSystem jobSystem;
    jobSystem.Initialize();
    
    // FrameAllocator 설정
    FrameAllocator frameAllocator;
    frameAllocator.Initialize(1024 * 1024);
    jobSystem.SetFrameAllocator(&frameAllocator);
    
    // Job 데이터 할당
    int* data = jobSystem.AllocateJobData<int>(42);
    
    // Job 디스패치
    JobHandle handle = jobSystem.Dispatch(MyJobFunction, data);
    
    // Job 완료 대기
    jobSystem.Wait(handle);
    
    // 결과 확인
    assert(*data == 84);
    
    jobSystem.Shutdown();
    frameAllocator.Shutdown();
}

// 2. 구조체 데이터를 사용하는 Job

struct MyJobData
{
    float* input;
    float* output;
    size_t count;
};

void ProcessArrayJob(void* data)
{
    MyJobData* jobData = static_cast<MyJobData*>(data);
    
    for (size_t i = 0; i < jobData->count; ++i)
    {
        jobData->output[i] = jobData->input[i] * 2.0f;
    }
}

void Example2()
{
    JobSystem jobSystem;
    jobSystem.Initialize();
    
    FrameAllocator frameAllocator;
    frameAllocator.Initialize(1024 * 1024);
    jobSystem.SetFrameAllocator(&frameAllocator);
    
    // 입력/출력 배열
    float input[100];
    float output[100];
    
    // Job 데이터 할당 및 초기화
    MyJobData* jobData = jobSystem.AllocateJobData<MyJobData>();
    jobData->input = input;
    jobData->output = output;
    jobData->count = 100;
    
    // Job 디스패치
    JobHandle handle = jobSystem.Dispatch(ProcessArrayJob, jobData);
    
    // Job 완료 대기
    jobSystem.Wait(handle);
    
    jobSystem.Shutdown();
    frameAllocator.Shutdown();
}

// 3. 여러 Job 병렬 실행

void ParallelProcessJob(void* data)
{
    int* index = static_cast<int*>(data);
    // 각 Job이 독립적으로 작업 수행
    // ...
}

void Example3()
{
    JobSystem jobSystem;
    jobSystem.Initialize();
    
    FrameAllocator frameAllocator;
    frameAllocator.Initialize(1024 * 1024);
    jobSystem.SetFrameAllocator(&frameAllocator);
    
    const int numJobs = 10;
    JobHandle handles[numJobs];
    
    // 여러 Job 디스패치
    for (int i = 0; i < numJobs; ++i)
    {
        int* index = jobSystem.AllocateJobData<int>(i);
        handles[i] = jobSystem.Dispatch(ParallelProcessJob, index);
    }
    
    // 모든 Job 완료 대기
    for (int i = 0; i < numJobs; ++i)
    {
        jobSystem.Wait(handles[i]);
    }
    
    jobSystem.Shutdown();
    frameAllocator.Shutdown();
}

// 4. Lambda를 사용한 Job (C++11 이상)

template<typename Func>
void LambdaJobWrapper(void* data)
{
    Func* func = static_cast<Func*>(data);
    (*func)();
}

template<typename Func>
JobHandle DispatchLambda(JobSystem& jobSystem, Func&& func)
{
    using FuncType = std::decay_t<Func>;
    FuncType* funcData = jobSystem.AllocateJobData<FuncType>(std::forward<Func>(func));
    return jobSystem.Dispatch(LambdaJobWrapper<FuncType>, funcData);
}

void Example4()
{
    JobSystem jobSystem;
    jobSystem.Initialize();
    
    FrameAllocator frameAllocator;
    frameAllocator.Initialize(1024 * 1024);
    jobSystem.SetFrameAllocator(&frameAllocator);
    
    int result = 0;
    
    // Lambda를 Job으로 실행
    JobHandle handle = DispatchLambda(jobSystem, [&result]() {
        result = 42;
    });
    
    jobSystem.Wait(handle);
    
    assert(result == 42);
    
    jobSystem.Shutdown();
    frameAllocator.Shutdown();
}

// 5. 프레임 단위 Job 실행 (게임 루프)

void GameLoopExample()
{
    JobSystem jobSystem;
    jobSystem.Initialize();
    
    FrameAllocator frameAllocator;
    frameAllocator.Initialize(16 * 1024 * 1024); // 16 MB
    jobSystem.SetFrameAllocator(&frameAllocator);
    
    bool running = true;
    while (running)
    {
        // 프레임 시작 - FrameAllocator 리셋
        frameAllocator.Reset();
        
        // 이 프레임의 Job들 디스패치
        JobHandle updateJob = jobSystem.Dispatch(UpdateGameLogic, nullptr);
        JobHandle physicsJob = jobSystem.Dispatch(UpdatePhysics, nullptr);
        JobHandle renderJob = jobSystem.Dispatch(PrepareRendering, nullptr);
        
        // 모든 Job 완료 대기
        jobSystem.Wait(updateJob);
        jobSystem.Wait(physicsJob);
        jobSystem.Wait(renderJob);
        
        // 렌더링
        // ...
        
        // 다음 프레임으로
    }
    
    jobSystem.Shutdown();
    frameAllocator.Shutdown();
}

*/
