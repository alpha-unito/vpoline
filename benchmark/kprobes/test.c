#include <stdio.h>
#include <unistd.h>

int main(void)
{
	printf("PID: %ld\nLoad example.ko with this PID, then press Enter.\n",
		(long)getpid());
	getchar();
    write(1, "Hello\n", 6);
	return 0;
}
