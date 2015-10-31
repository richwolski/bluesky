#ifndef __WORKQUEUE_H__
#define __WORKQUEUE_H__ 1

#include <boost/thread.hpp>
#include <boost/shared_ptr.hpp>
#include <queue>
#include "util.h"

using namespace boost;
using namespace std;

namespace bicker
{
    struct interrupted_error : public virtual std::exception { };

    class WorkUnit
    {
    public:
        virtual ~WorkUnit() {};
        virtual void run() = 0;
    };

    class WorkQueue
    {
    public:
        WorkQueue();
        WorkQueue(int thread_count);

        ~WorkQueue();

        shared_ptr<WorkUnit> get();
        void put(shared_ptr<WorkUnit> work_unit);

    protected:
        void spawnThread();

    private:
        class Worker
        {
        public:
            Worker(WorkQueue* queue);
            void operator()();
        private:
            WorkQueue *_queue;
        };

        int _thread_count;
        int _min_threads;
        int _max_threads;
        mutex _queue_lock;
        condition_variable _queue_non_empty;
        queue<shared_ptr<WorkUnit> > _queue;
        thread_group _threads;
        volatile bool _running;
    };

    class TaskNotification
    {
    public:
        TaskNotification();

        void registerTask();
        void completeTask(bool success = true);

        void waitForComplete();

        bool failCount();
    private:
        int                _expected;
        int                _count;
        int                _fail_count;
        mutex              _lock;
        condition_variable _cond;
    };

} // namespace bicker

#endif
