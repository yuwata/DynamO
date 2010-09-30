/*  DYNAMO:- Event driven molecular dynamics simulator 
    http://www.marcusbannerman.co.uk/dynamo
    Copyright (C) 2010  Marcus N Campbell Bannerman <m.bannerman@gmail.com>

    This program is free software: you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    version 3 as published by the Free Software Foundation.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include <magnet/memory/pool.hpp>
#include <magnet/thread/mutex.hpp>
#include <magnet/function/task.hpp>

namespace magnet {
  namespace thread {
    class TaskQueue
    {
    public:
      //Actual queuer
      inline virtual void queueTask(function::Task* threadfunc)
      {
	thread::ScopedLock lock1(_queue_mutex);    
	_waitingFunctors.push(threadfunc);
      }

      void drainQueue()
      {
	thread::ScopedLock lock1(_queue_mutex);

	while (!_waitingFunctors.empty())
	  {
	    (*_waitingFunctors.front())();
	    delete _waitingFunctors.front();
	    _waitingFunctors.pop();
	  }
      }

    protected:
      std::queue<function::Task*> _waitingFunctors;
      thread::Mutex _queue_mutex;

    };
  }
}
