/* Copyright (C) 2004 Chris Vine

The library comprised in this file or of which this file is part is
distributed by Chris Vine under the GNU Lesser General Public
License as follows:

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public License
   as published by the Free Software Foundation; either version 2.1 of
   the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License, version 2.1, for more details.

   You should have received a copy of the GNU Lesser General Public
   License, version 2.1, along with this library (see the file LGPL.TXT
   which came with this source code package in the src/utils sub-directory);
   if not, write to the Free Software Foundation, Inc.,
   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA

*/

#ifndef SEM_SYNC_H
#define SEM_SYNC_H

#include <semaphore.h>
#include <errno.h>

class SemSync {

  bool valid;
  sem_t sem;

  // this class cannot be copied - the semaphore is owned by the class and not shared
  SemSync(const SemSync&);
  SemSync& operator=(const SemSync&);
public:
  
  bool is_valid(void) const {return valid;}
  int getvalue(void);
  int post(void) {return sem_post(&sem);}
  void wait(void);
  SemSync(unsigned int init_count = 0);
  ~SemSync(void);
};

inline int SemSync::getvalue(void) {
  int count;
  sem_getvalue(&sem, &count);
  return count;
}

inline void SemSync::wait(void) {

  while (sem_wait(&sem) == -1 && errno == EINTR);
}

inline SemSync::SemSync(unsigned int init_count): valid(false) {

  if(sem_init(&sem, 0, init_count) == 0) valid = true;
}

inline SemSync::~SemSync(void) {

  while (getvalue() < 1) post();
  sem_destroy(&sem);
}

#endif
