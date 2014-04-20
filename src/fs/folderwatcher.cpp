#include "folderwatcher.hpp"

#include <CoreServices/CoreServices.h>

static void watchCallback(ConstFSEventStreamRef streamRef, void* clientCallBackInfo,
    size_t numEvents, void* eventPaths, const FSEventStreamEventFlags eventFlags[], const FSEventStreamEventId eventIds[])
{
    auto changeQueue = static_cast<BlockingQueue<string>*>(clientCallBackInfo);
    
    for (size_t i = 0; i < numEvents; ++i)
    {
        const char* path = static_cast<char**>(eventPaths)[i];
        unsigned int flag = eventFlags[i];
        
        if (flag & kFSEventStreamEventFlagItemIsFile)
        {
            changeQueue->push(path);
        }
    }
}

static void watcher(const string& path, BlockingQueue<string>* changeQueue, function<void()>* stopSignal)
{
    pthread_setname_np("FolderWatcher");
    
    CFRunLoopRef rl = CFRunLoopGetCurrent();
    *stopSignal = [=]() { CFRunLoopStop(rl); };
    
    CFStringRef cpath = CFStringCreateWithCString(nullptr, path.c_str(), kCFStringEncodingUTF8);
    CFArrayRef cpaths = CFArrayCreate(nullptr, reinterpret_cast<const void**>(&cpath), 1, nullptr);

    CFAbsoluteTime latency = 0.1;

    FSEventStreamContext context = {};
    context.info = changeQueue;
    
    FSEventStreamRef stream =
        FSEventStreamCreate(nullptr, watchCallback, &context, cpaths, kFSEventStreamEventIdSinceNow, latency, kFSEventStreamCreateFlagFileEvents);

    FSEventStreamScheduleWithRunLoop(stream, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
    FSEventStreamStart(stream);
    
    CFRunLoopRun();
}

FolderWatcher::FolderWatcher(const string& path)
: watchThread(bind(watcher, path, &changeQueue, &stopSignal))
{
}

FolderWatcher::~FolderWatcher()
{
    stopSignal();
    
    watchThread.join();
}

void FolderWatcher::addListener(Listener *listener)
{
    listeners.insert(listener);
}

void FolderWatcher::removeListener(Listener *listener)
{
    listeners.erase(listener);
}

void FolderWatcher::processChanges()
{
    string path;
    
    while (changeQueue.pop(path))
    {
        for (auto& l: listeners)
            l->onFileChanged(path);
    }
}