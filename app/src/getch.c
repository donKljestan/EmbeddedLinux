#include <errno.h>
#include <termios.h>
#include <unistd.h>

/* Blocking read of a single character, without Enter and without echo. */
char getch(void)
{
	struct termios oldt, newt;
	int have_termios = (tcgetattr(STDIN_FILENO, &oldt) == 0);
	char c = 0;
	ssize_t ret;

	if (have_termios)
	{
		newt = oldt;
		newt.c_lflag &= ~(ICANON | ECHO);
		newt.c_cc[VMIN] = 1;
		newt.c_cc[VTIME] = 0;
		tcsetattr(STDIN_FILENO, TCSANOW, &newt);
	}

	/* SIGALRM from the timer interrupts read() with EINTR, so retry. */
	do
	{
		ret = read(STDIN_FILENO, &c, 1);
	} while (ret < 0 && errno == EINTR);

	if (have_termios)
		tcsetattr(STDIN_FILENO, TCSANOW, &oldt);

	return (ret == 1) ? c : 0;
}
