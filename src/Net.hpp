#pragma once

#include <curl/curl.h>

#include <atomic>
#include <cstdio>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "Util.hpp"

namespace nt {

struct HttpResult {
  bool ok = false;
  long status = 0;
  std::string body;
  std::string error;
};

struct Transfer {
  std::atomic<double> got{0};
  std::atomic<double> total{0};
  std::atomic<bool> cancel{false};
  std::atomic<bool> finished{false};
  std::atomic<bool> success{false};
  float progress() const {
    double t = total.load();
    return t > 0 ? (float)std::min(1.0, got.load() / t) : 0.0f;
  }
};

class Net {
 public:
  std::atomic<bool> insecure{false};

  Net() { curl_global_init(CURL_GLOBAL_DEFAULT); }
  ~Net() {
    stop();
    curl_global_cleanup();
  }

  void start(int apiThreads = 3, int transferThreads = 2) {
    api_.start(apiThreads);
    xfer_.start(transferThreads);
  }

  void stop() {
    api_.stop();
    xfer_.stop();
    std::lock_guard<std::mutex> lock(rm_);
    results_.clear();
  }

  void pump() {
    for (;;) {
      std::function<void()> fn;
      {
        std::lock_guard<std::mutex> lock(rm_);
        if (results_.empty()) return;
        fn = std::move(results_.front());
        results_.pop_front();
      }
      fn();
    }
  }

  void get(const std::string& url, std::function<void(HttpResult)> cb, long timeoutSec = 25) {
    api_.push([this, url, cb = std::move(cb), timeoutSec]() {
      HttpResult r = doGet(url, timeoutSec);
      post([cb, r = std::move(r)]() mutable { cb(std::move(r)); });
    });
  }

  std::shared_ptr<Transfer> download(const std::string& url, const std::string& dest,
                                     std::function<void(bool, std::string)> cb, bool small = false) {
    auto t = std::make_shared<Transfer>();
    auto job = [this, url, dest, t, cb = std::move(cb)]() {
      std::string err;
      bool ok = doDownload(url, dest, *t, err);
      t->success = ok;
      t->finished = true;
      post([cb, ok, err]() { if (cb) cb(ok, err); });
    };
    if (small) api_.push(std::move(job));
    else xfer_.push(std::move(job));
    return t;
  }

 private:
  platform::WorkerPool api_, xfer_;
  std::mutex rm_;
  std::deque<std::function<void()>> results_;

  void post(std::function<void()> fn) {
    std::lock_guard<std::mutex> lock(rm_);
    results_.push_back(std::move(fn));
  }

  void configure(CURL* c, const std::string& url) {
    curl_easy_setopt(c, CURLOPT_URL, url.c_str());
    curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(c, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(c, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(c, CURLOPT_USERAGENT, "NekoDrome/1.0");

    platform::configureCurl(c);
    if (insecure.load()) {
      curl_easy_setopt(c, CURLOPT_SSL_VERIFYPEER, 0L);
      curl_easy_setopt(c, CURLOPT_SSL_VERIFYHOST, 0L);
    }
  }

  static size_t writeString(char* p, size_t sz, size_t n, void* user) {
    static_cast<std::string*>(user)->append(p, sz * n);
    return sz * n;
  }

  HttpResult doGet(const std::string& url, long timeout) {
    HttpResult r;
    CURL* c = curl_easy_init();
    if (!c) {
      r.error = "curl init failed";
      return r;
    }
    configure(c, url);
    curl_easy_setopt(c, CURLOPT_TIMEOUT, timeout);
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, &Net::writeString);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, &r.body);

    CURLcode rc = curl_easy_perform(c);
    if (rc != CURLE_OK) {
      r.error = curl_easy_strerror(rc);
    } else {
      curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &r.status);
      r.ok = r.status >= 200 && r.status < 300;
      if (!r.ok) r.error = "HTTP " + std::to_string(r.status);
    }
    curl_easy_cleanup(c);
    return r;
  }

  struct DlCtx {
    FILE* f;
    Transfer* t;
  };

  static size_t writeFile(char* p, size_t sz, size_t n, void* user) {
    DlCtx* d = static_cast<DlCtx*>(user);
    return std::fwrite(p, sz, n, d->f);
  }

  static int xferInfo(void* user, curl_off_t dltotal, curl_off_t dlnow, curl_off_t, curl_off_t) {
    DlCtx* d = static_cast<DlCtx*>(user);
    d->t->got = (double)dlnow;
    if (dltotal > 0) d->t->total = (double)dltotal;
    return d->t->cancel.load() ? 1 : 0;
  }

  bool doDownload(const std::string& url, const std::string& dest, Transfer& t, std::string& err) {
    ensureDir(parentDir(dest));
    std::string part = dest + ".part";
    FILE* f = std::fopen(part.c_str(), "wb");
    if (!f) {
      err = "cannot write file";
      return false;
    }
    CURL* c = curl_easy_init();
    if (!c) {
      std::fclose(f);
      err = "curl init failed";
      return false;
    }
    DlCtx ctx{f, &t};
    configure(c, url);
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, &Net::writeFile);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, &ctx);
    curl_easy_setopt(c, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(c, CURLOPT_XFERINFOFUNCTION, &Net::xferInfo);
    curl_easy_setopt(c, CURLOPT_XFERINFODATA, &ctx);
    curl_easy_setopt(c, CURLOPT_LOW_SPEED_LIMIT, 1L);
    curl_easy_setopt(c, CURLOPT_LOW_SPEED_TIME, 25L);
    if (const char* limit = std::getenv("NEKOTUNE_THROTTLE")) {
      long long bps = std::atoll(limit);
      if (bps > 0) curl_easy_setopt(c, CURLOPT_MAX_RECV_SPEED_LARGE, (curl_off_t)bps);
    }
    CURLcode rc = curl_easy_perform(c);
    long status = 0;
    char* ct = nullptr;
    curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &status);
    curl_easy_getinfo(c, CURLINFO_CONTENT_TYPE, &ct);
    std::string ctype = ct ? lower(ct) : "";
    curl_easy_cleanup(c);
    std::fclose(f);
    bool ok = rc == CURLE_OK && status >= 200 && status < 300;
    if (ok && (ctype.find("json") != std::string::npos || ctype.find("xml") != std::string::npos ||
               ctype.find("html") != std::string::npos))
      ok = false;
    if (!ok) {
      if (t.cancel.load()) err = "cancelled";
      else if (rc != CURLE_OK) err = curl_easy_strerror(rc);
      else if (status >= 200 && status < 300) err = "Server returned an error";
      else err = "HTTP " + std::to_string(status);
      removeFile(part);
      return false;
    }
    if (!renameFile(part, dest)) {
      err = "cannot finalize file";
      return false;
    }
    return true;
  }
};

}
