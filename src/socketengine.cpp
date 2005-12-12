#include "inspircd_config.h"
#include "globals.h"
#include "inspircd.h"
#ifdef USE_EPOLL
#include <sys/epoll.h>
#define EP_DELAY 5
#endif
#ifdef USE_KQUEUE
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#endif

char ref[65535];

const char X_LISTEN		= 0;
const char X_ESTAB_CLIENT	= 1;
const char X_ESTAB_MODULE	= 2;
const char X_ESTAB_DNS		= 3;

const char X_READBIT		= 0x80;

SocketEngine::SocketEngine()
{
#ifdef USE_EPOLL
	EngineHandle = epoll_create(65535);
#endif
#ifdef USE_KQUEUE
	EngineHandle = kqueue();
#endif
}

SocketEngine::~SocketEngine()
{
#ifdef USE_EPOLL || USE_KQUEUE
	close(EngineHandle);
#endif
}

bool SocketEngine::AddFd(int fd, bool readable, char type)
{
	this->fds.push_back(fd);
	if (readable)
		ref[fd] |= X_READBIT;
#ifdef USE_EPOLL
	struct epoll_event ev;
	log(DEBUG,"epoll: Adduser to events, ep=%d socket=%d",EngineHandle,fd);
	readable ? ev.events = EPOLLIN | EPOLLET : ev.events = EPOLLOUT | EPOLLET;
	ev.data.fd = fd;
	int i = epoll_ctl(EngineHandle, EPOLL_CTL_ADD, fd, &ev);
	if (i < 0)
	{
		log(DEBUG,"epoll: List insertion failure!");
		return false;
	}
#endif
#ifdef USE_KQUEUE
	struct kevent ke;
	log(DEBUG,"kqueue: Add user to events, kq=%d socket=%d",EngineHandle,fd);
	EV_SET(&ke, socket, readable ? EVFILT_READ : EVFILT_WRITE, EV_ADD, 0, 0, NULL);
	int i = kevent(EngineHandle, &ke, 1, 0, 0, NULL);
	if (i == -1)
	{
		log(DEBUG,"kqueue: List insertion failure!");
		return false;
	}
#endif
return true;
}

bool SocketEngine::DelFd(int fd)
{
	std::vector<int>::iterator i = this->fds.find(fd);
	if (i != this->fds.end())
		this->fds.erase(i);
#ifdef USE_KQUEUE
	struct kevent ke;
	EV_SET(&ke, fd, ref[fd] && X_READBIT ? EVFILT_READ : EVFILT_WRITE, EV_DELETE, 0, 0, NULL);
	int i = kevent(EngineHandle, &ke, 1, 0, 0, NULL);
	if (i == -1)
	{
		log(DEBUG,"kqueue: Failed to remove user from queue!");
		return false;
	}
#endif
#ifdef USE_EPOLL
	struct epoll_event ev;
	ref[fd] && X_READBIT ? ev.events = EPOLLIN | EPOLLET : ev.events = EPOLLOUT | EPOLLET;
	ev.data.fd = fd;
	int i = epoll_ctl(EngineHandle, EPOLL_CTL_DEL, fd, &ev);
	if (i < 0)
	{
		log(DEBUG,"epoll: List deletion failure!");
		return false;
	}
#endif
	ref[fd] = 0;
	return (i != this->fds.end());
}

bool SocketEngine::Wait(unsigned long millisecs, std::vector<int> &fdlist)
{
	fdlist.clear();
#ifdef USE_SELECT
	int highest_fd = 0;
	timeval tval;
	int sresult;
	for (unsigned int a = 0; a < fds.size(); a++)
	{
		if (ref[fds[a]] && X_READBIT)
		{
			FD_SET (fds[a], &rfdset);
		}
		else
		{
			FD_SET (fds[a], &wfdset);
		}
		
	}
	tval.tv_sec = 0;
	tval.tv_usec = 1000L;
	sresult = select(FD_SETSIZE, &rfdset, &wfdset, NULL, &tval);
	if (sresult > 0)
	{
		for (unsigned int a = 0; a < fds.size(); a++)
		{
			if ((FD_ISSET (fds[a], &rfdset)) || (FD_ISSET (fds[a], &wfdset)))
				fdlist.push_back(fds[a]);
		}
	}
#endif
#ifdef USE_KQUEUE
	ts.tv_nsec = 1000L;
	ts.tv_sec = 0;
	int i = kevent(EngineHandle, NULL, 0, &ke_list, 65535, &ts);
	for (int j = 0; j < i; j++)
		fdlist.push_back(ke_list[j].ident);
#endif
#ifdef USE_EPOLL
	int i = epoll_wait(EngineHandle, events, 65535, 1);
	for (int j = 0; j < i; j++)
		fdlist.push_back(event[0].data.fd);
#endif
	return true;
}

std::string SocketEngine::GetName()
{
#ifdef USE_SELECT
	return "select";
#endif
#ifdef USE_KQUEUE
	return "kqueue";
#endif
#ifdef USE_EPOLL
	return "epoll";
#endif
	return "misconfigured";
}
