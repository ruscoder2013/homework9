#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <queue>
#include <algorithm>
#include <thread>
#include <condition_variable>

#include "threadsafe_queue.h"
#include "async.h"

struct file_record {
    std::string file_name;
    std::string bulk;
};

std::string get_bulk_str(std::vector<std::string>& commands) {
    std::ostringstream oss;
    if (commands.size()==0) return "";
    oss << "bulk: ";
    for(int i = 0; i < commands.size(); i++)
    {
        if (i>0)
            oss << ", ";
        oss << commands[i];
    }
    oss << std::endl;
    return oss.str();
}

std::string time_from_epoch()
{
    std::ostringstream oss;
    using namespace std::chrono;
    system_clock::time_point tp = system_clock::now();
    system_clock::duration dtn = tp.time_since_epoch();
    auto tt = dtn.count() * system_clock::period::num / system_clock::period::den;
    oss << tt;
    return oss.str();
}

std::string name_file() {
    static std::string prev_name = "";
    std::string time_str = time_from_epoch();
    std::string name = "bulk" + time_str;

    std::size_t found = prev_name.find(name);
    if (found!=std::string::npos) {
        std::string sub = prev_name.substr(found+name.size());
        if(sub.empty()) {
            name += "2";
        }
        else {
            int n = atoi(sub.c_str());
            n++;
            name += std::to_string(n);
        }
    }
    prev_name = name;
    return name+".log";
}

namespace async {
    
    struct HandleInfo {
        std::string string_buffer;
        std::vector<std::string> commands;
        int brace_count = 0;
        bool adding = false;
    };

    class HandleManager {
    public:
        HandleManager(size_t n) {
            N = n;
            log_thread = new std::thread(&HandleManager::write_to_cout, this);
            file1_thread = new std::thread(&HandleManager::write_to_file_thread, this);
            file2_thread = new std::thread(&HandleManager::write_to_file_thread, this);
        }
        ~HandleManager() {
            try_to_show_all();
            for(int i = 0; i < handlesInfo.size(); i++)
                try_to_show(handlesInfo[i]);
            finish = true;
            cv.notify_one();
            cv_file.notify_all();
            log_thread->join();
            file1_thread->join();
            file2_thread->join();
            if(log_thread!=nullptr)
                delete log_thread;
            if(file1_thread!=nullptr)
                delete file1_thread;
            if(file2_thread!=nullptr)
                delete file2_thread;
        }
        void receive(HandleInfo* handle_info, const char* data, std::size_t size) {
            std::string s(data);
            if(handle_info->string_buffer.size()!=0)
            {
                s = handle_info->string_buffer + s;
                handle_info->string_buffer.clear();
            }
            std::stringstream ss(s);
            std::string cmd;
            while (std::getline(ss, cmd)) {
                if(ss.eof()) continue;
                if(cmd.compare("{")==0) {
                    try_to_show_all();
                    handle_info->brace_count++;
                    handle_info->adding = true;
                } else if(cmd.compare("}")==0) {
                    handle_info->brace_count--;
                    try_to_show(handle_info);
                    if(handle_info->brace_count<0) {
                        handle_info->brace_count++;
                        continue;
                    }
                } else {
                    if(handle_info->adding) {
                        if (handle_info->commands.size()==0) 
                            file_name = name_file();
                        handle_info->commands.push_back(cmd);
                        if(handle_info->commands.size()>=N)
                            try_to_show(handle_info);
                    }
                    else {
                        if (commands.size()==0) 
                            file_name = name_file();
                        
                        commands.push_back(cmd);
                        if(commands.size()>=N)
                            try_to_show_all();
                    }
                    
                }
            }
            if(data[size-1]!='\n')
                handle_info->string_buffer = cmd;
        }
        HandleInfo* addHandle() {
            HandleInfo* h_info = new HandleInfo();
            handlesInfo.push_back(h_info);
            return h_info;
        }
        void removeHandle(HandleInfo* handleInfo) {
            try_to_show_all();
            try_to_show(handleInfo);
            handlesInfo.erase(std::remove(handlesInfo.begin(), handlesInfo.end(), 
            handleInfo), handlesInfo.end());
        }
    private:
        std::thread *log_thread;
        std::thread *file1_thread;
        std::thread *file2_thread;
        std::condition_variable cv;
        std::mutex mut;

        std::condition_variable cv_file;
        std::mutex mut_file;

        threadsafe_queue<std::string> messages;
        threadsafe_queue<file_record> messages_2;
        
        std::chrono::milliseconds ms;
        
        std::string string_buffer;
        std::vector<std::string> commands;
        int brace_count = 0;
        
        
        std::string cmd;
        std::string file_name; 
        
        int handle = 1;
        bool finish = false;
        bool ready_flag = false;
        int N;

        std::vector<HandleInfo*> handlesInfo;
        std::ofstream out;
        void write_to_file_thread() {
        std::unique_lock<std::mutex> lk(mut_file);
            while(!finish) {
                cv_file.wait(lk, [this](){ 
                    return !messages_2.empty() || finish; 
                });
                while(!messages_2.empty()) {
                    file_record record;
                    auto success = messages_2.try_pop(record);
                    if(success)
                    {
                        std::ofstream out;
                        out.open(record.file_name);
                        out << record.bulk;
                        out.close();
                    }
                }
                if(finish)
                    break; 
            }
        }
        


        void write_to_cout() {
            std::unique_lock<std::mutex> lk(mut);
            while(!finish) {
                cv.wait(lk, [this](){ 
                    return !messages.empty() || finish; 
                    });
                while(!messages.empty())
                {
                    std::string str;
                    auto success = messages.try_pop(str);
                    if(success)
                        std::cout << str;
                } 
            }
        }
        void try_to_show(HandleInfo* hanldeInfo) {
            
            if(hanldeInfo->brace_count==0) {
                hanldeInfo->adding = false;
                auto bulk_str = get_bulk_str(hanldeInfo->commands);
                messages.push(bulk_str);
                cv.notify_one();
                
                messages_2.push(file_record{file_name, bulk_str});
                cv_file.notify_one();
                
                hanldeInfo->commands.clear();
            }
        }

        void try_to_show_all() {
                auto bulk_str = get_bulk_str(commands);
                messages.push(bulk_str);
                cv.notify_one();
                
                messages_2.push(file_record{file_name, bulk_str});
                cv_file.notify_one();
                
                commands.clear();
        }
    };

    using handle_t = void*;
    HandleManager* handleManager;
    
    handle_t connect(std::size_t bulk) {
        if (handleManager==nullptr)
            handleManager = new HandleManager(bulk);
        return reinterpret_cast<handle_t>(handleManager->addHandle());
    }

    void receive(handle_t handle_, const char* data, std::size_t size) {
        std::string s(data);
        HandleInfo* handle = reinterpret_cast<HandleInfo*>(handle_);
        handleManager->receive(handle, data, size);
    }
    void disconnect(handle_t handle_) {
        handleManager->removeHandle(reinterpret_cast<HandleInfo*>(handle_));
    }
}