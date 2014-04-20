#pragma once

#include <thread>

class FolderWatcher
{
public:
	FolderWatcher(const string& path);
    ~FolderWatcher();
    
    void addWatch(const vector<string>& paths, const function<void()>& callback);

    void fireCallbacks(const vector<string>& paths);
    
private:
    thread watchThread;
    function<void()> stopSignal;
    
    mutex watchMapMutex;
    unordered_multimap<string, shared_ptr<function<void()>>> watchMap;
};