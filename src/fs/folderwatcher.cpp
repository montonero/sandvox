#include "folderwatcher.hpp"

#include <thread>

#include <CoreServices/CoreServices.h>

static void watchCallback(ConstFSEventStreamRef streamRef, void* clientCallBackInfo,
    size_t numEvents, void* eventPaths, const FSEventStreamEventFlags eventFlags[], const FSEventStreamEventId eventIds[])
{
    vector<string> paths;
    
    for (size_t i = 0; i < numEvents; ++i)
    {
        if (eventFlags[i] & kFSEventStreamEventFlagItemIsFile)
        {
            char fullpath[PATH_MAX];
            
            if (realpath(static_cast<char**>(eventPaths)[i], fullpath))
            {
                paths.push_back(fullpath);
            }
        }
    }
    
    FolderWatcher* folderWatcher = static_cast<FolderWatcher*>(clientCallBackInfo);
    
    folderWatcher->fireCallbacks(paths);
}

static void watcher(const string& path, FolderWatcher* folderWatcher, function<void()>* stopSignal)
{
    pthread_setname_np("FolderWatcher");
    
    CFRunLoopRef rl = CFRunLoopGetCurrent();
    *stopSignal = [=]() { CFRunLoopStop(rl); };
    
    CFStringRef cpath = CFStringCreateWithCString(nullptr, path.c_str(), kCFStringEncodingUTF8);
    CFArrayRef cpaths = CFArrayCreate(nullptr, reinterpret_cast<const void**>(&cpath), 1, nullptr);

    CFAbsoluteTime latency = 1.0;

    FSEventStreamContext context = {};
    context.info = folderWatcher;
    
    FSEventStreamRef stream =
        FSEventStreamCreate(nullptr, watchCallback, &context, cpaths, kFSEventStreamEventIdSinceNow, latency, kFSEventStreamCreateFlagFileEvents);

    FSEventStreamScheduleWithRunLoop(stream, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
    FSEventStreamStart(stream);
    
    CFRunLoopRun();
}

FolderWatcher::FolderWatcher(const string& path)
: watchThread(bind(watcher, path, this, &stopSignal))
{
}

FolderWatcher::~FolderWatcher()
{
    stopSignal();
    
    watchThread.join();
}

void FolderWatcher::addWatch(const vector<string>& paths, const function<void()>& callback)
{
    auto ptr = make_shared<function<void()>>(callback);
    
    {
        unique_lock<mutex> lock(watchMapMutex);
        
        for (auto& p: paths)
        {
            char fullpath[PATH_MAX];
            
            if (realpath(p.c_str(), fullpath))
            {
                watchMap.insert(make_pair(fullpath, ptr));
            }
        }
    }
}

void FolderWatcher::fireCallbacks(const vector<string>& paths)
{
    unordered_set<shared_ptr<function<void()>>> callbacks;
    
    {
        unique_lock<mutex> lock(watchMapMutex);
        
        for (auto& p: paths)
        {
            auto it = watchMap.find(p);
            
            if (it != watchMap.end())
                callbacks.insert(it->second);
        }
    }
    
    for (auto& cb: callbacks)
        (*cb)();
}