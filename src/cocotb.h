// Copyright cocotb contributors
// Licensed under the Revised BSD License, see LICENSE for details.
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <chrono>
#include <cmath>
#include <coroutine>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <exception>
#include <format>
#include <iostream>
#include <mutex>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include <gpi.h>

namespace cocotb {

enum class unit : int32_t { fs = -15, ps = -12, ns = -9, us = -6, ms = -3, sec = 1, step = 0 };

[[nodiscard]] inline unit get_precision() {
    int32_t precision = 0;
    gpi_get_sim_precision(&precision);
    return static_cast<unit>(precision);
}

[[nodiscard]] inline uint64_t get_sim_time(unit time_unit = unit::step) {
    uint32_t high = 0;
    uint32_t low = 0;
    gpi_get_sim_time(&high, &low);
    const uint64_t time = (static_cast<uint64_t>(high) << 32) | low;
    if (time_unit == unit::step) {
        return time;
    }
    const auto power = std::pow(10.0, -static_cast<int>(time_unit));
    return static_cast<uint64_t>(static_cast<double>(time) / power);
}

enum class LogLevel { Info, Warn, Error };

class Logger {
  public:
    explicit Logger(std::string component) : component_(std::move(component)) {}

    template <typename... Args> void info(std::format_string<Args...> fmt, Args &&...args) { log(LogLevel::Info, std::format(fmt, std::forward<Args>(args)...)); }

    void info(std::string_view message) { log(LogLevel::Info, message); }

    void log(LogLevel level, std::string_view message, bool with_time = true) {
        std::scoped_lock lock{mutex_};

        uint32_t high = 0;
        uint32_t low = 0;
        gpi_get_sim_time(&high, &low);
        const uint64_t time_val = (static_cast<uint64_t>(high) << 32) | low;
        const auto unit_str = unit_to_string(get_precision());
        const std::string time_str = with_time ? std::format("{:>9.2f}{}", static_cast<double>(time_val), unit_str) : std::string{"------"};
        std::cout << std::format("{:>9}   {:<8} {:<32}   {}", time_str, level_to_string(level), component_, message) << std::endl;
    }

  private:
    static constexpr std::string_view level_to_string(LogLevel level) {
        switch (level) {
        case LogLevel::Info:
            return "INFO";
        case LogLevel::Warn:
            return "WARN";
        case LogLevel::Error:
            return "ERROR";
        }
        return "UNKNOWN";
    }

    static constexpr std::string_view unit_to_string(unit time_unit) {
        switch (time_unit) {
        case unit::fs:
            return "fs";
        case unit::ps:
            return "ps";
        case unit::ns:
            return "ns";
        case unit::us:
            return "us";
        case unit::ms:
            return "ms";
        case unit::sec:
            return "sec";
        case unit::step:
            return "step";
        }
        return "UNKNOWN";
    }

    std::string component_;
    std::mutex mutex_;
};

inline Logger log{"cocotb"};

class Scheduler; // forward declaration

class Value {
  public:
    explicit Value(gpi_sim_hdl handle) : handle_(handle) {}

    Value &operator=(int32_t value);
    Value &operator=(uint32_t value) {
        *this = static_cast<int32_t>(value);
        return *this;
    }

    bool operator==(int32_t value) const { return gpi_get_signal_value_long(handle_) == static_cast<int32_t>(value); }

    operator int32_t() const { return int32_t(gpi_get_signal_value_long(handle_)); }
    operator uint32_t() const { return uint32_t(gpi_get_signal_value_long(handle_)); }
    operator bool() const { return bool(gpi_get_signal_value_real(handle_)); }

  private:
    gpi_sim_hdl handle_{nullptr};
};

class Handle {
  public:
    Handle() = default;
    Value value{nullptr};

    explicit Handle(gpi_sim_hdl handle) : value(handle), handle_(handle) {}

    Handle operator[](const std::string &name) const {
        if (!handle_) {
            std::cerr << "Attempted to index an invalid handle with '" << name << "'\n";
            return Handle{};
        }

        if (auto it = cache_.find(name); it != cache_.end()) {
            return it->second.value_or(Handle{});
        }

        Handle child(gpi_get_handle_by_name(handle_, name.c_str(), GPI_AUTO));
        if (!child.valid()) {
            std::cerr << "Failed to find child '" << name << "'\n";
            return Handle{};
        }

        cache_.emplace(name, child);
        return child;
    }

    [[nodiscard]] bool valid() const noexcept { return handle_ != nullptr; }
    [[nodiscard]] gpi_sim_hdl raw() const noexcept { return handle_; }

  private:
    gpi_sim_hdl handle_{nullptr};

    // name -> cached Handle if found
    mutable std::unordered_map<std::string, std::optional<Handle>> cache_;
};

class Dut : public Handle {
  public:
    Dut() = default;
    explicit Dut(gpi_sim_hdl handle) : Handle(handle) {}
};

template <typename T = void> class task {
  public:
    struct promise_type {
        bool detached{false};
        bool completed{false};
        bool cancelled{false};
        std::coroutine_handle<> join_waiter{};
        std::exception_ptr exception_{nullptr};

        task<T> get_return_object() { return task<T>{std::coroutine_handle<promise_type>::from_promise(*this)}; }
        std::suspend_always initial_suspend() const noexcept { return {}; }
        std::suspend_always final_suspend() const noexcept { return {}; }
        void return_void() const noexcept {}
        void unhandled_exception() { exception_ = std::current_exception(); }
    };

    using handle_type = std::coroutine_handle<promise_type>;

    explicit task(handle_type handle) : coro_(handle) {}
    task(task &&other) noexcept : coro_(std::exchange(other.coro_, {})) {}
    task &operator=(task &&other) noexcept {
        if (this != &other) {
            if (coro_) {
                coro_.destroy();
            }
            coro_ = std::exchange(other.coro_, {});
        }
        return *this;
    }
    task(const task &) = delete;
    task &operator=(const task &) = delete;

    ~task() {
        if (coro_) {
            coro_.destroy();
        }
    }

    [[nodiscard]] handle_type release() noexcept { return std::exchange(coro_, {}); }

    void detach() noexcept {
        if (coro_) {
            coro_.promise().detached = true;
        }
    }

    [[nodiscard]] bool done() const noexcept { return coro_ && coro_.done(); }

    [[nodiscard]] handle_type handle() const noexcept { return coro_; }

    [[nodiscard]] std::exception_ptr exception() const noexcept { return coro_ ? coro_.promise().exception_ : nullptr; }

    struct join_awaiter {
        handle_type handle;
        bool await_ready() const noexcept {
            if (!handle) {
                return true;
            }
            // Check if handle is done (completed) - this is safe even if coroutine
            // was destroyed If handle.done() is true, the coroutine has finished
            if (handle.done()) {
                // Access promise only if coroutine is done (safe)
                return handle.promise().completed;
            }
            // Coroutine is still running, not ready
            return false;
        }
        void await_suspend(std::coroutine_handle<> cont) const noexcept;
        void await_resume() const {
            if (handle) {
                auto &promise = handle.promise();
                if (promise.exception_) {
                    std::rethrow_exception(promise.exception_);
                }
                handle.destroy();
            }
        }
    };

    join_awaiter join() const { return join_awaiter{coro_}; }

    // Make task<> directly awaitable (like Python coroutines)
    // Note: This transfers ownership - the coroutine will be destroyed when it
    // completes
    join_awaiter operator co_await() {
        auto handle = coro_;
        coro_ = {}; // Release ownership so destructor doesn't destroy it
        return join_awaiter{handle};
    }

  private:
    handle_type coro_;
};

using test_fn = task<> (*)(Dut &);

class TestRunner; // forward declaration

class Scheduler {
  public:
    static Scheduler &instance();

    void set_dut_handle(gpi_sim_hdl handle);
    void register_test(const std::string &name, test_fn fn);
    void start_all_tests();

    // Track active coroutines for cleanup
    void register_active_coroutine(task<>::handle_type handle);
    void unregister_active_coroutine(task<>::handle_type handle);
    void cancel_all_coroutines();

    void schedule_task(task<> &&t);
    void schedule_handle(task<>::handle_type handle);
    void schedule_after_time(std::coroutine_handle<> handle, uint64_t delay);
    void schedule_on_edge(std::coroutine_handle<> handle, gpi_sim_hdl signal, gpi_edge edge);
    void enqueue_ready(std::coroutine_handle<> handle);
    void schedule_readwrite(task<>::handle_type handle);
    void schedule_readonly(task<>::handle_type handle);
    void request_readwrite_callback();
    void run_ready(bool flush_writes = true);
    void queue_write(gpi_sim_hdl handle, int32_t value);

  private:
    using TaskHandle = task<>::handle_type;
    struct WriteRequest {
        gpi_sim_hdl handle{nullptr};
        int32_t value{0};
    };

    struct BaseCallback {
        Scheduler *sched{nullptr};
        TaskHandle coro;
        gpi_cb_hdl cb_handle{nullptr};
    };

    struct EdgeCallback : BaseCallback {
        gpi_sim_hdl signal{nullptr};
        gpi_edge edge{GPI_VALUE_CHANGE};
    };

    static int timer_callback(void *userdata);
    static int edge_callback(void *userdata);
    static int readwrite_callback(void *userdata);
    static int readonly_callback(void *userdata);
    static int nexttime_rw_callback(void *userdata);
    void flush_pending_writes();

    std::deque<TaskHandle> ready_;
    std::deque<WriteRequest> pending_writes_;
    bool rw_cb_pending_{false};
    bool in_readonly_{false};
    bool need_rw_after_ro_{false};
    std::optional<Dut> dut_;
    gpi_sim_hdl dut_handle_{nullptr};
    Logger log_{"cocotb.scheduler"};

    // Track all active coroutines for cleanup
    std::set<task<>::handle_type> active_coroutines_;

    friend class TestRunner;
};

class TestRunner {
  public:
    struct TestResult {
        std::string name;
        bool passed{false};
        double execution_time_s{0.0};
        std::string error_message;
    };

    static TestRunner &instance();

    void set_dut_handle(gpi_sim_hdl handle);
    void register_test(const std::string &name, test_fn fn);
    void start_all_tests();
    void on_test_complete();
    void report_results();
    bool is_current_test(task<>::handle_type handle) const;
    task<>::handle_type get_current_test_handle() const;

  private:
    TestRunner() = default;

    using Clock = std::chrono::high_resolution_clock;

    std::vector<std::pair<std::string, test_fn>> tests_;
    std::vector<TestResult> results_;
    std::optional<Dut> dut_;
    gpi_sim_hdl dut_handle_{nullptr};
    size_t current_test_index_{0};
    task<>::handle_type current_test_handle_{};
    Logger log_{"cocotb.regression"};
    Clock::time_point test_start_time_{};

    void run_next_test();
    void cleanup_all_coroutines();
};

class Timer {
  public:
    explicit Timer(uint64_t delay, unit time_unit = unit::step) : delay_(delay), time_unit_(time_unit) {}
    bool await_ready() const noexcept { return delay_ == 0; }
    void await_suspend(std::coroutine_handle<> handle) const {
        const auto precision = get_precision();
        double factor = 1.0;
        if (time_unit_ != unit::step) {
            const auto precision_power = std::pow(10.0, -static_cast<int>(precision));
            const auto unit_power = std::pow(10.0, -static_cast<int>(time_unit_));
            factor = precision_power / unit_power;
        }
        const auto delay_final = static_cast<uint64_t>(static_cast<double>(delay_) * factor);
        Scheduler::instance().schedule_after_time(handle, delay_final);
    }
    void await_resume() const noexcept {}

  private:
    uint64_t delay_;
    unit time_unit_;
};

class RisingEdge {
  public:
    explicit RisingEdge(Handle signal) : signal_(signal) {}
    bool await_ready() const noexcept { return false; }
    void await_suspend(std::coroutine_handle<> handle) const { Scheduler::instance().schedule_on_edge(handle, signal_.raw(), GPI_RISING); }
    void await_resume() const noexcept {}

  private:
    Handle signal_;
};

inline Scheduler &Scheduler::instance() {
    static Scheduler sched;
    return sched;
}

inline void Scheduler::set_dut_handle(gpi_sim_hdl handle) {
    dut_handle_ = handle;
    dut_.reset();
}

inline void Scheduler::register_test(const std::string &name, test_fn fn) { TestRunner::instance().register_test(name, fn); }

inline void Scheduler::start_all_tests() { TestRunner::instance().start_all_tests(); }

inline void Scheduler::register_active_coroutine(task<>::handle_type handle) {
    if (handle) {
        active_coroutines_.insert(handle);
    }
}

inline void Scheduler::unregister_active_coroutine(task<>::handle_type handle) {
    if (handle) {
        active_coroutines_.erase(handle);
    }
}

inline void Scheduler::cancel_all_coroutines() {
    // Get the current test handle to avoid destroying it
    task<>::handle_type current_test = TestRunner::instance().get_current_test_handle();

    for (auto handle : active_coroutines_) {
        if (handle && !handle.done()) {
            // Don't cancel the current test - it's already completed
            if (current_test && handle.address() != current_test.address()) {
                handle.promise().cancelled = true;
            }
        }
    }
    // Clean up completed coroutines (except the current test)
    auto it = active_coroutines_.begin();
    while (it != active_coroutines_.end()) {
        if (*it && (*it).done()) {
            // Don't destroy the current test handle - let TestRunner handle it
            if (!current_test || (*it).address() != current_test.address()) {
                (*it).destroy();
                it = active_coroutines_.erase(it);
            } else {
                ++it;
            }
        } else {
            ++it;
        }
    }
}

inline void Scheduler::schedule_task(task<> &&t) {
    auto handle = t.release();
    if (handle) {
        register_active_coroutine(handle);
        ready_.push_back(handle);
    }
}

inline void Scheduler::schedule_handle(task<>::handle_type handle) {
    if (handle) {
        register_active_coroutine(handle);
        ready_.push_back(handle);
    }
}

inline void Scheduler::enqueue_ready(std::coroutine_handle<> handle) {
    if (!handle) {
        return;
    }
    ready_.push_back(TaskHandle::from_address(handle.address()));
    // Always request readwrite callback to ensure writes are flushed before
    // resuming
    request_readwrite_callback();
}

// Helper function to enqueue a coroutine handle (for use in join_awaiter)
inline void enqueue_coroutine_handle(std::coroutine_handle<> handle) { Scheduler::instance().enqueue_ready(handle); }

// Implementation of join_awaiter::await_suspend (defined after Scheduler is
// complete)
template <typename T> inline void task<T>::join_awaiter::await_suspend(std::coroutine_handle<> cont) const noexcept {
    if (!handle) {
        enqueue_coroutine_handle(cont);
        return;
    }

    // Check if coroutine is done - handle.done() is safe to call even if
    // coroutine completed It just checks the coroutine state, doesn't access
    // freed memory
    if (handle.done()) {
        // Coroutine already completed, resume immediately
        enqueue_coroutine_handle(cont);
        return;
    }

    // Access promise - at this point coroutine is not done, so it should be valid
    // But there's a race condition: coroutine might complete between
    // handle.done() and here So we need to be careful
    auto &promise = handle.promise();

    // Double-check completion status (race condition: might have completed
    // between checks)
    if (promise.completed) {
        // Coroutine already completed, resume immediately
        enqueue_coroutine_handle(cont);
        return;
    }

    // Set the join waiter - this must happen atomically before the coroutine
    // completes Once we set this, even if the coroutine completes, it won't be
    // destroyed
    promise.join_waiter = cont;

    // Coroutine not completed yet - start it if it hasn't been started
    // Only schedule if it's not already detached (which means it's already
    // running) If it's detached, it's already been scheduled via start_soon
    if (!promise.detached) {
        // Schedule the coroutine to start (if it's at initial suspend, this will
        // start it)
        Scheduler::instance().schedule_handle(handle);
    }
}

inline void Scheduler::schedule_readwrite(TaskHandle handle) {
    if (!handle) {
        return;
    }
    ready_.push_back(handle);
    request_readwrite_callback();
}

inline void Scheduler::schedule_readonly(TaskHandle handle) {
    if (!handle) {
        return;
    }
    ready_.push_back(handle);
    gpi_register_readonly_callback(&Scheduler::readonly_callback, this);
}

inline void Scheduler::request_readwrite_callback() {
    if (in_readonly_) {
        need_rw_after_ro_ = true;
        return;
    }
    if (rw_cb_pending_) {
        return;
    }
    rw_cb_pending_ = true;
    gpi_register_readwrite_callback(&Scheduler::readwrite_callback, this);
}

inline void Scheduler::queue_write(gpi_sim_hdl handle, int32_t value) {
    pending_writes_.push_back(WriteRequest{handle, value});
    request_readwrite_callback();
}

inline void Scheduler::flush_pending_writes() {
    while (!pending_writes_.empty()) {
        auto wr = pending_writes_.front();
        pending_writes_.pop_front();
        gpi_set_signal_value_int(wr.handle, wr.value, GPI_DEPOSIT);
    }
    rw_cb_pending_ = false;
}

inline void Scheduler::run_ready(bool flush_writes) {
    if (flush_writes) {
        flush_pending_writes();
    }
    while (!ready_.empty()) {
        auto handle = ready_.front();
        ready_.pop_front();
        if (!handle) {
            continue;
        }
        if (handle.promise().cancelled) {
            handle.destroy();
            continue;
        }
        handle.resume();
        if (handle.done()) {
            auto &promise = handle.promise();
            promise.completed = true;
            unregister_active_coroutine(handle);

            if (promise.join_waiter) {
                // When a coroutine completes, any writes it made are queued.
                // We need to ensure writes are flushed before resuming the join waiter.
                // If we're already in a readwrite callback (flush_writes=true),
                // flush any pending writes from the completed coroutine now.
                // Then schedule the join waiter to run in the next readwrite callback
                // after a delta cycle, so the simulator has time to process the writes.
                if (flush_writes && !pending_writes_.empty()) {
                    // Flush any pending writes from the completed coroutine
                    flush_pending_writes();
                    // Schedule the join waiter with a zero-time delay to ensure
                    // it runs in the next callback cycle after writes are applied
                    schedule_after_time(promise.join_waiter, 0);
                } else {
                    // Not in a readwrite callback, just enqueue normally
                    enqueue_ready(promise.join_waiter);
                }
                // Don't destroy detached coroutines if they have a join waiter
                // The join waiter will destroy it in await_resume
            } else {
                // No join waiter
                bool is_current_test = TestRunner::instance().is_current_test(handle);
                // Check if this is the current test completing - don't destroy it yet,
                // let on_test_complete() handle cleanup so it can check for exceptions
                if (is_current_test) {
                    TestRunner::instance().on_test_complete();
                } else if (promise.detached) {
                    // Not the current test, safe to destroy
                    handle.destroy();
                }
            }
        }
    }
    // std::cout << "run_ready: " << ready_.size() << std::endl;
}

inline int Scheduler::timer_callback(void *userdata) {
    auto *cb = static_cast<BaseCallback *>(userdata);
    cb->sched->schedule_readwrite(cb->coro);
    delete cb;
    return 0;
}

inline int Scheduler::edge_callback(void *userdata) {
    // std::cout << "Edge callback" << std::endl;
    auto *cb = static_cast<EdgeCallback *>(userdata);
    cb->sched->ready_.push_back(cb->coro);
    cb->sched->run_ready(false);
    delete cb;
    return 0;
}

inline int Scheduler::readwrite_callback(void *userdata) {
    auto *sched = static_cast<Scheduler *>(userdata);
    sched->run_ready(true);
    return 0;
}

inline int Scheduler::readonly_callback(void *userdata) {
    auto *sched = static_cast<Scheduler *>(userdata);
    sched->in_readonly_ = true;
    sched->run_ready(false);
    sched->in_readonly_ = false;
    if (sched->need_rw_after_ro_) {
        sched->need_rw_after_ro_ = false;
        gpi_register_nexttime_callback(&Scheduler::nexttime_rw_callback, sched);
    }
    return 0;
}

inline int Scheduler::nexttime_rw_callback(void *userdata) {
    auto *sched = static_cast<Scheduler *>(userdata);
    sched->request_readwrite_callback();
    return 0;
}

inline void Scheduler::schedule_after_time(std::coroutine_handle<> handle, uint64_t delay) {
    auto *cb = new BaseCallback();
    cb->sched = this;
    cb->coro = TaskHandle::from_address(handle.address());
    cb->cb_handle = gpi_register_timed_callback(&Scheduler::timer_callback, cb, delay);
    if (!cb->cb_handle) {
        std::cerr << "Failed to register timed callback" << std::endl;
        delete cb;
        enqueue_ready(handle);
    }
}

inline void Scheduler::schedule_on_edge(std::coroutine_handle<> handle, gpi_sim_hdl signal, gpi_edge edge) {
    // std::cout << "Schedule on edge" << std::endl;
    auto *cb = new EdgeCallback();
    cb->sched = this;
    cb->coro = TaskHandle::from_address(handle.address());
    cb->signal = signal;
    cb->edge = edge;
    cb->cb_handle = gpi_register_value_change_callback(&Scheduler::edge_callback, cb, signal, edge);
    if (!cb->cb_handle) {
        std::cerr << "Failed to register value change callback" << std::endl;
        delete cb;
        enqueue_ready(handle);
    }
}

inline bool register_test(const std::string &name, test_fn fn) {
    TestRunner::instance().register_test(name, fn);
    return true;
}

#define COCOTB_TEST(name)                                                                                                                                                          \
    task<> name(cocotb::Dut &);                                                                                                                                                    \
    static bool name##_registered = ::cocotb::register_test(#name, &name);

class JoinHandle {
  public:
    explicit JoinHandle(task<>::handle_type handle) : handle_(handle) {}
    JoinHandle(JoinHandle &&other) noexcept : handle_(std::exchange(other.handle_, {})), joined_(std::exchange(other.joined_, false)) {}
    JoinHandle &operator=(JoinHandle &&other) noexcept {
        if (this != &other) {
            cleanup();
            handle_ = std::exchange(other.handle_, {});
            joined_ = std::exchange(other.joined_, false);
        }
        return *this;
    }
    JoinHandle(const JoinHandle &) = delete;
    JoinHandle &operator=(const JoinHandle &) = delete;

    ~JoinHandle() { cleanup(); }

    auto join() {
        joined_ = true;
        return task<>::join_awaiter{handle_};
    }

    // Make JoinHandle directly awaitable (like Python's Task)
    auto operator co_await() {
        joined_ = true;
        return task<>::join_awaiter{handle_};
    }

  private:
    void cleanup() {
        if (!handle_) {
            return;
        }
        if (!joined_) {
            // Signal cancellation; scheduler will destroy when it next sees it.
            handle_.promise().cancelled = true;
        }
        handle_ = {};
    }

    task<>::handle_type handle_{};
    bool joined_{false};
};

template <class T> inline JoinHandle start_soon(T &&t) {
    auto handle = std::move(t).release();
    if (handle) {
        // Mark as detached so it manages its own lifecycle
        // But it won't be destroyed if it has a join_waiter
        handle.promise().detached = true;
        Scheduler::instance().schedule_handle(handle);
    }
    return JoinHandle(handle);
}

inline TestRunner &TestRunner::instance() {
    static TestRunner runner;
    return runner;
}

inline void TestRunner::set_dut_handle(gpi_sim_hdl handle) {
    dut_handle_ = handle;
    dut_.reset();
    Scheduler::instance().set_dut_handle(handle);
}

inline void TestRunner::register_test(const std::string &name, test_fn fn) { tests_.push_back({name, fn}); }

inline void TestRunner::start_all_tests() {
    if (!dut_handle_) {
        std::cerr << "No DUT handle available" << std::endl;
        return;
    }
    log_.info("Running tests");
    dut_.emplace(dut_handle_);
    current_test_index_ = 0;
    results_.clear();
    run_next_test();
}

inline void TestRunner::run_next_test() {
    if (!dut_) {
        return;
    }

    if (current_test_index_ >= tests_.size()) {
        report_results();
        gpi_finish();
        return;
    }

    auto idx = current_test_index_++;
    auto &test_name = tests_[idx].first;
    log_.info(std::format("\033[34mrunning\033[0m {} ({}/{})", test_name, current_test_index_, tests_.size()));

    test_start_time_ = Clock::now();

    try {
        auto t = tests_[idx].second(*dut_);
        current_test_handle_ = t.release();
        if (current_test_handle_) {
            current_test_handle_.promise().detached = true;
            Scheduler::instance().schedule_handle(current_test_handle_);
            Scheduler::instance().run_ready();
        } else {
            // Test returned immediately, mark as complete
            on_test_complete();
        }
    } catch (...) {
        // Catch exceptions during test creation
        const auto test_end_time = Clock::now();
        const auto duration_s = std::chrono::duration<double>(test_end_time - test_start_time_).count();

        TestResult result;
        result.name = test_name;
        result.passed = false;
        result.execution_time_s = duration_s;
        try {
            std::rethrow_exception(std::current_exception());
        } catch (const std::exception &e) {
            result.error_message = e.what();
        } catch (...) {
            result.error_message = "Unknown exception";
        }
        results_.push_back(result);

        run_next_test();
    }
}

inline void TestRunner::on_test_complete() {
    if (current_test_index_ == 0) {
        return; // No test was running
    }

    const auto test_end_time = Clock::now();
    const auto duration_s = std::chrono::duration<double>(test_end_time - test_start_time_).count();

    auto &test_name = tests_[current_test_index_ - 1].first;
    TestResult result;
    result.name = test_name;
    result.execution_time_s = duration_s;

    // Check for exceptions before cleanup
    std::exception_ptr test_exception = nullptr;
    if (current_test_handle_) {
        test_exception = current_test_handle_.promise().exception_;
    }

    // Clean up all remaining coroutines (but preserve current_test_handle_ for
    // now)
    cleanup_all_coroutines();

    // Check exception
    if (test_exception) {
        result.passed = false;
        try {
            std::rethrow_exception(test_exception);
        } catch (const std::exception &e) {
            result.error_message = e.what();
        } catch (...) {
            result.error_message = "Unknown exception";
        }
    } else {
        result.passed = true;
    }

    // Now destroy the current test handle and unregister it
    if (current_test_handle_) {
        Scheduler::instance().unregister_active_coroutine(current_test_handle_);
        current_test_handle_.destroy();
        current_test_handle_ = {};
    }

    results_.push_back(result);

    // Report result
    if (result.passed) {
        log_.info(std::format("{} \033[32mpassed\033[0m execution time: {:.3f} s", test_name, duration_s));
    } else {
        log_.info(std::format("{} \033[31mfailed\033[0m execution time: {:.3f} s", test_name, duration_s));
        if (!result.error_message.empty()) {
            log_.log(LogLevel::Error, std::format("  Error: {}", result.error_message));
        }
    }

    run_next_test();
}

inline bool TestRunner::is_current_test(task<>::handle_type handle) const { return current_test_handle_ && current_test_handle_.address() == handle.address(); }

inline task<>::handle_type TestRunner::get_current_test_handle() const { return current_test_handle_; }

inline void TestRunner::cleanup_all_coroutines() {
    Scheduler::instance().cancel_all_coroutines();
    // Run ready queue to process cancellations
    Scheduler::instance().run_ready(true);
}

inline void TestRunner::report_results() {
    std::string separator(87, '*');
    log_.info(separator);
    log_.info(std::format("** TEST {} STATUS  REAL TIME (s) {} **", std::string(24, ' '), std::string(29, ' ')));
    log_.info(separator);

    size_t passed = 0;
    size_t failed = 0;
    double total_time = 0.0;

    for (const auto &result : results_) {
        total_time += result.execution_time_s;
        if (result.passed) {
            passed++;
        } else {
            failed++;
        }
    }

    for (const auto &result : results_) {
        std::string status = result.passed ? "\033[32mPASS\033[0m" : "\033[31mFAIL\033[0m";
        log_.info(std::format("** {:<30} {:<9} {:>15.3f} {} **", result.name, status, result.execution_time_s, std::string(29, ' ')));
        if (!result.passed && !result.error_message.empty()) {
            log_.log(LogLevel::Error, std::format("  {}", result.error_message));
        }
    }

    log_.info(separator);
    log_.info(std::format("** TESTS={} PASS={} FAIL={} {:<28} **", passed + failed, passed, failed, std::string(87 - 28, ' ')));
    log_.info(separator);

    if (failed > 0) {
        std::exit(1);
    }
}

inline Value &Value::operator=(int32_t value) {
    // std::cout << "Set Handle[" << gpi_get_signal_name_str(handle_) << "] = " <<
    // value << std::endl;
    if (!handle_) {
        std::cerr << "Attempted to drive an invalid handle" << std::endl;
        return *this;
    }
    Scheduler::instance().queue_write(handle_, value);
    return *this;
}

static int on_sim_start(void * /*cb_data*/, int /*argc*/, char const *const * /*argv*/) {
    LOG_INFO("Start of simulation");
    gpi_sim_hdl top = gpi_get_root_handle(nullptr);
    if (!top) {
        if (const char *env_top = std::getenv("TOPLEVEL")) {
            top = gpi_get_root_handle(env_top);
        }
    }

    if (!top) {
        std::cerr << "Failed to get root handle" << std::endl;
        return -1;
    }
    TestRunner::instance().set_dut_handle(top);
    TestRunner::instance().start_all_tests();
    return 0;
}

static void on_sim_end(void * /*cb_data*/) { LOG_INFO("End of simulation"); }

inline void assert(bool condition, std::string_view message = {}) {
    if (!condition) {
        if (!message.empty()) {
            throw std::runtime_error(std::format("Assertion failed: {}", message));
        }
        throw std::runtime_error("Assertion failed.");
    }
}

// Clock generator coroutine
task<> Clock(Dut &dut, uint period, unit unit) {
    // cocotb::log.info("Starting clock_generator coroutine");
    // Lookup once
    auto clk = dut["clk"];
    auto timer = Timer(period / 2, unit);
    while (true) {
        clk.value = 0;
        co_await timer;
        clk.value = 1;
        co_await timer;
    }
}

} // namespace cocotb

// Entry point function that will be called by GPI
// This function name should match what you specify in GPI_USERS
// NOTE: This is called during library initialization, so we can't access
// simulation objects here. We must register callbacks instead.
extern "C" void cocotb_entry_point() {
    if (!gpi_has_registered_impl()) {
        LOG_ERROR("Error: No GPI implementation registered");
        return;
    }

    if (gpi_register_start_of_sim_time_callback(&cocotb::on_sim_start, nullptr) != 0) {
        LOG_ERROR("Failed to register start of simulation callback");
        return;
    }

    gpi_register_end_of_sim_time_callback(&cocotb::on_sim_end, nullptr);
    LOG_INFO("Entry point registered");
}
