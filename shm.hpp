#ifndef _SHM_HPP_
#define _SHM_HPP_

/// @file
///// @author Jeremy
///// @version 1.0
///// @section DESCRIPTION
/////
///// The new / delete operations for Shared Memory are implemented here.

#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <fcntl.h>
#include <cstdio>
#include <cstdlib>

template <class T>
T *new_shm(int &shm_id, int shm_key = 5487)
{
	if ((shm_id = shmget(shm_key, sizeof(T), S_IRUSR | S_IWUSR | IPC_CREAT)) < 0)
	{
		perror("Shmget");
		exit(EXIT_FAILURE);
	}

	T *shm = (T *)shmat(shm_id, 0, 0);
	new(shm) T();

	return shm;
}


template <class T>
void delete_shm(int &shm_id, T *t)
{
	t->T::~T();
	shmdt(t);
	shmctl(shm_id, IPC_RMID, NULL);
}

#endif
