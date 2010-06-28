noinst_LTLIBRARIES = liboobase.la liboosvrbase.la

liboobase_la_CPPFLAGS = -I@top_srcdir@/include
liboobase_la_CFLAGS = $(PTHREAD_CFLAGS) $(LTDLINCL)
liboobase_la_CXXFLAGS = $(PTHREAD_CFLAGS) $(LTDLINCL)
liboobase_la_LDFLAGS = $(PTHREAD_LIBS)

liboobase_la_SOURCES = \
	Buffer.cpp \
	Condition.cpp \
	DLL.cpp \
	Error.cpp \
	Mutex.cpp \
	Once.cpp \
	TLS.cpp \
	Thread.cpp \
	TimeVal.cpp \
	PosixSocket.cpp \
	Win32.cpp \
	Win32Socket.cpp \
	tr24731.cpp \
	utf8.cpp

liboosvrbase_la_CPPFLAGS = -I@top_srcdir@/include
liboosvrbase_la_CFLAGS = $(PTHREAD_CFLAGS) $(SQLITE3_CFLAGS)
liboosvrbase_la_CXXFLAGS = $(PTHREAD_CFLAGS) $(SQLITE3_CFLAGS)
liboosvrbase_la_LDFLAGS = $(PTHREAD_LIBS)

if WIN32
liboosvrbase_la_LDFLAGS += -lnetapi32 -luserenv -lshlwapi
endif

liboosvrbase_la_SOURCES = \
	CmdArgs.cpp \
	Database.cpp \
	Logger.cpp \
	PipeWin32.cpp \
	Proactor.cpp \
	ProactorEv.cpp \
	ProactorWin32.cpp \
	pw_info.cpp \
	SecurityWin32.cpp \
	SQLite3_comp.c
