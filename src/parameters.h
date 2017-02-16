#ifndef _PARS__H_
#define _PARS__H_

#include <sys/types.h>
#include <unistd.h>

#include <string>

using namespace std;

#include <asm/elphel/c313a.h>

//#define FRAMES_AHEAD_FPS	3 /// number of video frames ahead of current to frite FPS limit
//#define FRAMES_SKIP_FPS		3 /// number of video frames to wait after target so circbuf will have at least 2 frames with new fps for calculation

class Parameters {
public:
/*
	enum vevent {
		VEVENT0,
		DAEMON_DISABLED,
		FPS_CHANGE,
		SIZE_CHANGE
	};
*/
	inline static Parameters *instance(void) {
		if(_parameters == NULL)
			_parameters = new Parameters();
		return _parameters;
	}
	~Parameters(void);

//	unsigned long get
	/// interface to global camera parameters 
	unsigned long getGPValue(unsigned long GPNumber);
	void setGValue(unsigned long GNumber, unsigned long value);
	unsigned long getFrameValue(unsigned long FPNumber);
	int write(unsigned long *data, int length) { return ::write(fd_fparmsall, (void *)data, length); }
	off_t lseek(off_t offset, int whence) { return ::lseek(fd_fparmsall, offset, whence); }
	bool daemon_enabled(void);
	void setPValue(unsigned long *val_array, int count);
protected:
	static Parameters *_parameters;
	Parameters(void);

	struct framepars_all_t *frameParsAll;
	struct framepars_t *framePars;
	unsigned long *globalPars;
	int fd_fparmsall;
};

//extern Video *video;

#endif // _VIDEO__H_
