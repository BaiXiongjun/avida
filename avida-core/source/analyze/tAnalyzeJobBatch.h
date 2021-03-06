/*
 *  tAnalyzeJobBatch.h
 *  Avida
 *
 *  Created by David on 1/11/09.
 *  Copyright 2009-2011 Michigan State University. All rights reserved.
 *
 *
 *  This file is part of Avida.
 *
 *  Avida is free software; you can redistribute it and/or modify it under the terms of the GNU Lesser General Public License
 *  as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
 *
 *  Avida is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License along with Avida.
 *  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef tAnalyzeJobBatch_h
#define tAnalyzeJobBatch_h

#include "apto/core.h"
#include "apto/platform.h"

#include "cAnalyzeJobQueue.h"
#include "tAnalyzeJob.h"

class cAvidaContext;

#if APTO_PLATFORM(WINDOWS) && defined(AddJob)
# undef AddJob
#endif


template<class JobClass> class tAnalyzeJobBatch
{
protected:
  template<class T> class tAnalyzeBatchJob;
  friend class tAnalyzeBatchJob<JobClass>;
  
protected:
  cAnalyzeJobQueue& m_queue;
  
  int m_jobs;
  
  Apto::Mutex m_mutex;
  Apto::ConditionVariable m_cond;
  
  
public:
  tAnalyzeJobBatch(cAnalyzeJobQueue& queue) : m_queue(queue), m_jobs(0) { ; }
  
  void AddJob(JobClass* target, void (JobClass::*funJ)(cAvidaContext&))
  {
    m_mutex.Lock();
    m_jobs++;
    m_mutex.Unlock();
    m_queue.AddJob(new tAnalyzeBatchJob<JobClass>(this, target, funJ));
  }
  
  void RunBatch()
  {
    m_queue.Start();
    m_mutex.Lock();
    while (m_jobs > 0) {
      m_cond.Wait(m_mutex);
    }
    m_mutex.Unlock();
  }
  
protected:
  template<class T> class tAnalyzeBatchJob : public tAnalyzeJob<T>
  {
  protected:
    tAnalyzeJobBatch<T>* m_batch;
    
  public:
    tAnalyzeBatchJob(tAnalyzeJobBatch<T>* batch, T* target, void (T::*funJ)(cAvidaContext&))
      : tAnalyzeJob<T>(target, funJ), m_batch(batch) { ; }
    
    void Run(cAvidaContext& ctx)
    {
      tAnalyzeJob<T>::Run(ctx);
      
      m_batch->m_mutex.Lock();
      m_batch->m_jobs--;
      m_batch->m_mutex.Unlock();
      m_batch->m_cond.Signal();
    }
  };
};


#endif
