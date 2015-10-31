#include "workqueue.h"
#include <boost/thread.hpp>

namespace bicker
{

WorkQueue::WorkQueue()
    :_thread_count(0), _min_threads(1), _max_threads(50), _running(true)
{
    for (int i = 0; i < _min_threads; ++i)
    {
        spawnThread();
    }
}

WorkQueue::WorkQueue(int thread_count)
    :_thread_count(0), 
     _min_threads(1), _max_threads(thread_count), _running(true)
{
    for (int i = 0; i < _min_threads; ++i)
    {
        spawnThread();
    }
}

WorkQueue::~WorkQueue()
{
    _running = false;

    {
        mutex::scoped_lock lock(_queue_lock);
        _queue_non_empty.notify_all();
    }

    _threads.join_all();
}

void WorkQueue::spawnThread()
{
    ++_thread_count;
    _threads.create_thread(Worker(this));
}

shared_ptr<WorkUnit> WorkQueue::get()
{
    mutex::scoped_lock lock(_queue_lock);

    while (_queue.size() == 0)
    {
        _queue_non_empty.wait(lock);
        if (!_running) throw interrupted_error();
    }

    shared_ptr<WorkUnit> back = _queue.front();
    _queue.pop();

    if (_queue.size() > 0 && _thread_count < _max_threads) spawnThread();

    return back;
}

void WorkQueue::put(shared_ptr<WorkUnit> work_unit)
{
    mutex::scoped_lock lock(_queue_lock);

    _queue.push(work_unit);

    _queue_non_empty.notify_one();
}

WorkQueue::Worker::Worker(WorkQueue* queue)
    :_queue(queue)
{
}

void WorkQueue::Worker::operator()()
{
    while (true)
    {
        try
        {
            shared_ptr<WorkUnit> unit = _queue->get();

            unit->run();
        }
        catch (interrupted_error)
        {
            return;
        }
    }
}

TaskNotification::TaskNotification()
:_expected(0), _count(0), _fail_count(0)
{
}

void TaskNotification::registerTask()
{
    mutex::scoped_lock lock(_lock);
    ++_expected;
}

void TaskNotification::completeTask(bool success)
{
    mutex::scoped_lock lock(_lock);
    if (!success) ++_fail_count;
    if (++_count == _expected) _cond.notify_all();
}

void TaskNotification::waitForComplete()
{
    mutex::scoped_lock lock(_lock);
    while (_count < _expected)
    {
        _cond.wait(lock);
    }
}

bool TaskNotification::failCount()
{
    return _fail_count;
}

} // namespace bicker
