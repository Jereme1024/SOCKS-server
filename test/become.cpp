#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <cstdio>
#include <cstdlib>

class Become
{
public:

	void main(int argc, char *argv[])
	{
		printf("** Execuing program: ls -l /bin/gzip **\n\n");

		char bin_path[] = "/bin/ls";
		int status;
		pid_t child_pid = vfork();
		
		if (child_pid < 0)
		{
			exit(-1);
		}

		if (child_pid != 0)
		{
			waitpid(child_pid, &status, 0);
		}
		else
		{
			execl(bin_path, bin_path, "-l", "/bin/gzip", NULL);
		}

		printf("\nchild return exit code: %d\n", WEXITSTATUS(status));
	}
};

int main(int argc, char *argv[])
{
	Become become;

	become.main(argc, argv);

	return 0;
}
