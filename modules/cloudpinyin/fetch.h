//
// Copyright (C) 2017~2017 by CSSlayer
// wengxt@gmail.com
//
// This library is free software; you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as
// published by the Free Software Foundation; either version 2.1 of the
// License, or (at your option) any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
// Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with this library; see the file COPYING. If not,
// see <http://www.gnu.org/licenses/>.
//
#ifndef _CLOUDPINYIN_FETCH_H_
#define _CLOUDPINYIN_FETCH_H_

#include "cloudpinyin_public.h"
#include <cstdint>
#include <curl/curl.h>
#include <fcitx-utils/event.h>
#include <fcitx-utils/intrusivelist.h>
#include <fcitx-utils/unixfd.h>
#include <iterator>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>
#define MAX_HANDLE 100l
#define MAX_BUFFER_SIZE 2048

class CurlQueue : public fcitx::IntrusiveListNode {
public:
    CurlQueue(bool keep = true) : keep_(keep), curl_(curl_easy_init()) {
        curl_easy_setopt(curl_, CURLOPT_PRIVATE, this);
        curl_easy_setopt(curl_, CURLOPT_WRITEDATA, this);
        curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION,
                         &CurlQueue::curlWriteFunction);
        curl_easy_setopt(curl_, CURLOPT_TIMEOUT, 10l);
        curl_easy_setopt(curl_, CURLOPT_NOSIGNAL, 1);
    }

    ~CurlQueue() { curl_easy_cleanup(curl_); }

    void release() {
        busy_ = false;
        if (!keep_) {
            delete this;
        } else {
            data_.clear();
            pinyin_.clear();
            // make sure lambda is free'd
            callback_ = CloudPinyinCallback();
            httpCode_ = 0;
        }
    }

    const auto &pinyin() const { return pinyin_; }
    void setPinyin(const std::string &pinyin) { pinyin_ = pinyin; }

    auto curl() { return curl_; }
    void finish(int result) {
        curlResult_ = result;
        curl_easy_getinfo(curl_, CURLINFO_RESPONSE_CODE, &httpCode_);
    }

    bool busy() const { return busy_; }
    void setBusy() { busy_ = true; }

    const std::vector<char> &result() { return data_; }

    CloudPinyinCallback callback() { return callback_; }
    void setCallback(CloudPinyinCallback callback) { callback_ = callback; }

    int httpCode() const { return httpCode_; }

private:
    static size_t curlWriteFunction(char *ptr, size_t size, size_t nmemb,
                                    void *userdata) {
        auto self = static_cast<CurlQueue *>(userdata);
        return self->curlWrite(ptr, size, nmemb);
    }

    size_t curlWrite(char *ptr, size_t size, size_t nmemb) {
        size_t realsize = size * nmemb;
        /*
         * We know that it isn't possible to overflow during multiplication if
         * neither operand uses any of the most significant half of the bits in
         * a size_t.
         */

        if ((unsigned long long)((nmemb | size) & ((unsigned long long)SIZE_MAX
                                                   << (sizeof(size_t) << 2))) &&
            (realsize / size != nmemb))
            return 0;

        if (SIZE_MAX - data_.size() < realsize) {
            realsize = SIZE_MAX - data_.size();
        }

        // make sure we won't be hacked
        if (data_.size() + realsize > MAX_BUFFER_SIZE) {
            return 0;
        }

        data_.reserve(data_.size() + realsize);
        std::copy(ptr, ptr + realsize, std::back_inserter(data_));
        return realsize;
    }

    bool keep_ = true;
    bool busy_ = false;
    CURL *curl_ = nullptr;
    int curlResult_ = 0;
    long httpCode_ = 0;
    std::vector<char> data_;
    std::string pinyin_;
    CloudPinyinCallback callback_;
};

typedef std::function<void(CurlQueue *)> SetupRequestCallback;

class FetchThread {
public:
    FetchThread(fcitx::UnixFD notifyFd);
    ~FetchThread();

    bool addRequest(SetupRequestCallback);
    CurlQueue *popFinished();

private:
    static void runThread(FetchThread *self);
    static int curlCallback(CURL *easy,      /* easy handle */
                            curl_socket_t s, /* socket */
                            int action,      /* see values below */
                            void *userp,     /* private callback pointer */
                            void *socketp);
    static int curlTimerCallback(CURLM *multi,    /* multi handle */
                                 long timeout_ms, /* see above */
                                 void *userp);    /* private callback
                                                     pointer */
    void curl(curl_socket_t s,                    /* socket */
              int action);
    void curlTimer(long timeout_ms);

    void handleIO(int fd, fcitx::IOEventFlags flags);
    void processMessages();

    void run();
    void finished(CurlQueue *queue);
    void quit();

    std::unique_ptr<std::thread> thread_;
    std::unique_ptr<fcitx::EventLoop> loop_;
    std::unordered_map<int, std::unique_ptr<fcitx::EventSourceIO>> events_;
    std::unique_ptr<fcitx::EventSourceTime> timer_;

    CURLM *curlm_;
    fcitx::UnixFD selfPipeFd_[2];
    fcitx::UnixFD notifyFd_;

    CurlQueue handles_[MAX_HANDLE];
    fcitx::IntrusiveList<CurlQueue> pendingQueue;
    fcitx::IntrusiveList<CurlQueue> workingQueue;
    fcitx::IntrusiveList<CurlQueue> finishingQueue;

    std::mutex pendingQueueLock;
    std::mutex finishQueueLock;
};

#endif // _CLOUDPINYIN_FETCH_H_
