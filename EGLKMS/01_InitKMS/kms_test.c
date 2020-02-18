#include <fcntl.h>
#include <unistd.h>
#include "init_kms.h"

int main(int argc, const char* argv[])
{
	int fd;
	struct kms kms = {0};

	fd = open("/dev/dri/renderD128", O_RDWR);
	if(fd < 0)
	{
		printf("Can't open fd\n");
		return -1;
	}

	init_kms(fd, &kms);

	close(fd);

	return 0;
}
