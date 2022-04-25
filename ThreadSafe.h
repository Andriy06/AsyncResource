// A wrapper over a thread-unsafe resource that needs to be accessed concurrently.
// The only way to access the resource is to call `Sync()` or `Async()` with a functor 
// that takes a reference to the resource as input.
// Using `Sync()` is the same as using a reader-writer mutex to synchronize access to the 
// resource, but it's impossible to forget to acquire mutex.
// `Async()` launches the given functor as a read-only task (if the functor takes 
// `const ResourceType&` as a parameter) or read-write task (if the functor takes mutable 
// `ResourceType&` as a parameter).
// Either multiple read-only tasks are executed concurrently or a single read-write task, 
// thus providing "multiple readers or single writer" thread-safety guarantee.
// `Async()` retuns a completion handle to the launched task. that can be used to access the 
// execution result. or as a prerequisite for another task(s).
template<typename ResourceType>
class TThreadSafe
{
public:
    template<typename... ArgTypes>
    TThreadSafe(ArgTypes&&... ResourceCtorArgs);

    // Executes the given accessor when it's thread-safe and returns the execution result.
    // An accesor must be a callable with signature `ResultType Accessor(const ResourceType&)` or 
    // `ResultType Accessor(ResourceType&)`, for read-only or read-write access respectively.
    template<typename AccessorType, typename ResultType = 
        decltype(std::declval<AccessorType>(std::declval<ResourceType>()))>
    ResultType Sync(FunctorType&& Functor);

    // Executes the given accessor asynchronously and thread-safely for the resource, and returns 
    // a completion handle, that can be used to get the execution result or as a prerequisite 
    // for another task(s).
    // An accesor must be a callable with signature `ResultType Accessor(const ResourceType&)` or 
    // `ResultType Accessor(ResourceType&)`, for read-only or read-write access respectively.
    template<typename AccessorType, typename ResultType = 
        decltype(std::declval<AccessorType>(std::declval<ResourceType>()))> // the result of 
        // `Accessor(Resource)` call
    TTask<ResultType> Async(AccessorType&& Accessor);

    // `PrerequisitesType` - a single prerequisite task or an iterable collection of `TTask<T>` that 
    // must be completed before this task is executed
    template<typename AccessorType, typename PrerequisitesType, 
        typename ResultType = decltype(std::declval<AccessorType>(std::declval<ResourceType>()))>
    TTask<ResultType> Async(AccessorType&& Accessor, PrerequisitesType&& Prerequisites);
};

// An example

// A thread-unsafe resource
class FResource
{
private:
    // Can be constructed only as `TThreadSafe`
    FResource(int InValue)
        : Value(InValue)
    {}
   
    friend class TThreadSafe<FResource>; 

public:
    int Read() const { return Value; }
    void Write(int InValue) { Value = InValue; }

private:
    int Value;
};

int main()
{
    TThreadSafe<FResource> Resource{ 0 };

    // a locked synchrounous call, same as `Resource.Async(...).GetResult()`, but faster
    int Res = Resource.Sync([]{const FResource& Resource} { return Resource.Read(); });

    // read-only (RO) tasks take `const&` to the resource, and are executed concurrently
    TTask<int> Read1 = Resource.Async([](const FResource& Resource) { return Resource.Read(); });
    TTask<int> Read2 = Resource.Async([](const FResource& Resource) { return Resource.Read(); });
    
    // read-write (RW) tasks take non-const reference and are executed exclusively
    TTask<void> Write = Resource.Async([](FResource& Resource) { Resource.Write(42); });
    
    TArray<int> Results = GetResults(Read1, Read2, Write);
}
