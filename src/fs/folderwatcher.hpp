#pragma once

#include "core/blockingqueue.hpp"

#include <thread>

class FolderWatcher
{
public:
    class Listener
    {
    public:
        virtual ~Listener() {}
        
        virtual void onFileChanged(const string& path) = 0;
    };
    
	FolderWatcher(const string& path);
    ~FolderWatcher();
    
    void addListener(Listener* listener);
    void removeListener(Listener* listener);
    
    void processChanges();

private:
    thread watchThread;
    function<void()> stopSignal;
    
    BlockingQueue<string> changeQueue;
    unordered_set<Listener*> listeners;
};