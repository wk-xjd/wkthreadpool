#include "utils.h"
#ifdef WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

namespace WKUtils
{
	unsigned int currentThreadId()
	{
#ifdef WIN32
		return GetCurrentThreadId();
#else
		return thread_self()
#endif
	};
}