#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>

class Pipe
{
public:

	void main(int argc, char *argv[])
	{
		int fd[2], nbytes;
		pid_t child_pid;
		char string[] = "Hello, world!\n";
		char readbuffer[80];

		pipe(fd);

		child_pid = vfork();

		if (child_pid < 0)
		{
			perror("fork failed.\n");
			exit(1);
		}

		if (child_pid == 0)
		{
			//close(fd[0]);
			//write(fd[1], string, (strlen(string)+1));

			std::cout << "fd[1] = " << fd[1] << "\n";
			dup2(fd[1], 1);
			execlp("ls", "ls", NULL);
		}
		else
		{
			//close(fd[1]);
			//nbytes = read(fd[0], readbuffer, sizeof(readbuffer));
			//printf("Received string: %s", readbuffer);
			
			//write(fd[1], string, (strlen(string)+1));
		}
	}

	void main2(int argc, char *argv[])
	{
		int fd[2];
		pipe(fd);

		pid_t child_pid = fork();

		if (child_pid == 0)
		{
			dup2(fd[1], 1);
			close(fd[0]);
			execlp("ls", "ls" , NULL);
		}
		else
		{
			close(fd[1]);

			child_pid = fork();

			if (child_pid == 0)
			{
				dup2(fd[0], 0);
				execlp("cat", "cat" , NULL);
			}
			else
			{
				close(fd[0]);
				//
				int status;
				wait (&status);  //EDITED to see what's going on
				wait (&status);  //EDITED to see what's going on
				if (WIFEXITED(status))
					printf("CHILD exited with &#37;d\n", WEXITSTATUS(status));
			 
				if (WIFSIGNALED(status))
					printf("signaled by %d\n", WTERMSIG(status));
			 
				if (WIFSTOPPED(status))      
					printf("stopped by %d\n", WSTOPSIG(status));
			 
				printf("DONE FATHER\n");
			}
		}
	}
};

int main(int argc, char *argv[])
{
	Pipe pipe;

	pipe.main2(argc, argv);

	return 0;
}
