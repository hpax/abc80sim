/*
 * Wrapper around abc80.exe for Windows, since Windows requires
 * applications to be compiled as either CLI or GUI applications,
 * and will not allow that to be managed at runtime. abc80.exe is
 * a console application and does I/O to the current console;
 * this wrapper launches it with the output redirected to a
 * pipe, and opens a window with the output if and only if there
 * is anything written to stdout or stderr.
 *
 * This is a generic program; it can be used to wrap any CLI application;
 * just name it foowin.exe to run foo.exe.
 *
 * This must be a wide char application, or things might break horribly
 * depending on the pathname of the original application.
 */

#include "compiler.h"
#include <tchar.h>
#undef main			/* Undo this particular SDL hack */

static wchar_t *format_error(wchar_t *msg, DWORD dw)
{
	wchar_t *buf, *errtxt;
	DWORD len;
	size_t buflen;

	if (!dw)
		return msg;

	len = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | 
			     FORMAT_MESSAGE_FROM_SYSTEM |
			     FORMAT_MESSAGE_IGNORE_INSERTS,
			     NULL, dw,
			     MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			     (wchar_t *)&errtxt, 0, NULL);
	
	if (!len)
		return msg;

	buflen = wcslen(msg) + len + 3;
	buf = calloc(buflen, sizeof *buf);

	if (buf) {
		swprintf(buf, buflen, L"%s: %s", msg, errtxt);
		msg = buf;
	}

	LocalFree(errtxt);
	return msg;
}

static no_return ErrorExit(wchar_t *msg)
{
	DWORD dw = GetLastError();
	
	MessageBoxW(NULL, format_error(msg, dw), L"Error", MB_OK); 
	ExitProcess(dw ? dw : ERROR_PATH_NOT_FOUND);
	abort();
}

/* Return the current executable filename in a malloc()'d buffer */
wchar_t *getmyname(size_t *lenp)
{
	wchar_t *buf, *bufx;
	size_t alen, len;

	/*
	 * GetModuleFileNameW() helpfully doesn't let us know if
	 * the input buffer is too small what size we actually need,
	 * so we have to guess...
	 */

	alen = MAX_PATH;	/* Good starting point */
	while (1) {
		buf = malloc(alen*sizeof(*buf));
		if (!buf)
			ErrorExit(L"Allocating memory");

		len = GetModuleFileNameW(NULL, buf, alen);
		if (len < alen)
			break;	/* We are good! */

		free(buf);
		alen <<= 1;
	}

	bufx = realloc(buf, (len+1)*sizeof(*buf)); /* Shrink buffer */
	buf  = bufx ? bufx : buf;

	if (lenp)
		*lenp = len;
	
	return buf;
}

static HANDLE create_window(void)
{
	AllocConsole();		/* May fail if we already have a console */
	return GetStdHandle(STD_OUTPUT_HANDLE);
}

#define BUF_SIZE 4096

static void handle_output(HANDLE in)
{
	char inbuf[BUF_SIZE];
	DWORD bytesread = 0;
	DWORD byteswritten;
	HANDLE out = INVALID_HANDLE_VALUE;

	while (1) {
		do {
			if (!ReadFile(in, inbuf, sizeof inbuf,
				      &bytesread, NULL))
				return; /* Pipe closed, assume app finished */
		} while (bytesread == 0);

		/*
		 * We got input... need to create a window
		 * and output the data
		 */
		if (out == INVALID_HANDLE_VALUE)
			out = create_window();

		if (!out || out == INVALID_HANDLE_VALUE)
			continue;

		byteswritten = 0;
		while (byteswritten < bytesread) {
			DWORD wlen;
			if (!WriteFile(out, inbuf+byteswritten,
				       bytesread-byteswritten, &wlen, NULL))
				break;

			byteswritten += wlen;
		}
	}
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
		    LPWSTR lpCmdLine, int nShowCmd)
{
	HANDLE NullDevR = NULL;
	HANDLE OutPipeW = NULL;
	HANDLE OutPipeR = NULL;
	SECURITY_ATTRIBUTES inheritable;
	STARTUPINFOW si;
	PROCESS_INFORMATION pi;
	wchar_t *filename, *tail;
	size_t mynamelen;

	(void)hInstance;
	(void)hPrevInstance;
	(void)nShowCmd;
	
	memset(&inheritable, 0, sizeof inheritable);
	inheritable.nLength = sizeof inheritable;
	inheritable.bInheritHandle = TRUE;

	/* Open relevant handle */

	/* Create null device read handle for input */
	NullDevR = CreateFileW(L"\\Device\\Null",
			       GENERIC_READ,
			       FILE_SHARE_DELETE|FILE_SHARE_READ|
			       FILE_SHARE_WRITE,
			       &inheritable,
			       OPEN_EXISTING,
			       FILE_ATTRIBUTE_NORMAL,
			       NULL);

	if (NullDevR == INVALID_HANDLE_VALUE)
		ErrorExit(L"Opening Null device failed");

	/* Create output pipe */
	if (!CreatePipe(&OutPipeR, &OutPipeW, &inheritable, BUF_SIZE))
		ErrorExit(L"Creating pipe failed");

	/* Only inherit the *write* descriptor, please */
	if (!SetHandleInformation(&OutPipeR, HANDLE_FLAG_INHERIT, 0))
		ErrorExit(L"STDOUT SetHandleInformation");

	/* Get our own filename */
	filename = getmyname(&mynamelen);
	tail = filename + mynamelen - 7;

	/* Strip "win" from filename */
	if (mynamelen <= 7 || _wcsicmp(tail, L"win.exe"))
		ErrorExit(L"Unable to determine subprocess filename");

	memmove(tail, tail+3, 4*sizeof(*tail));
	
	/* Launch CLI process */
	memset(&si, 0, sizeof si);
	si.cb          = sizeof si;
	si.dwFlags     = STARTF_USESTDHANDLES|STARTF_USESHOWWINDOW;
	si.wShowWindow = SW_HIDE;
	si.hStdInput   = NullDevR;
	si.hStdOutput  = OutPipeW;
	si.hStdError   = OutPipeW;
	if (!CreateProcessW(filename, lpCmdLine, NULL, NULL,
			    TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi))
		ErrorExit(L"Launching subprocess");

	/* We don't need these anymore... */
	CloseHandle(NullDevR);
	CloseHandle(OutPipeW);

	/*
	 * We don't need these anymore, either - instead we just
	 * wait for the pipe to close when the application exits
	 */
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);

	/* Strip the pathname from the child process to use as window name */
	*tail = L'\0';		/* Remove .exe */
	while (--tail >= filename) {
		if (*tail == L'/' || *tail == L'\\' || *tail == L':')
			break;
	}
	tail++;
	
	/*
	 * Read from the pipe until the application exits
	 */
	handle_output(OutPipeR);
	return 0;
}
