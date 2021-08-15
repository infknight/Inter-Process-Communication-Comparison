#include "common.h"
#include "MQreqchannel.h"
#include <mqueue.h>
using namespace std;

/*--------------------------------------------------------------------------*/
/* CONSTRUCTOR/DESTRUCTOR FOR CLASS   R e q u e s t C h a n n e l  */
/*--------------------------------------------------------------------------*/

MQRequestChannel::MQRequestChannel(const string _name, const Side _side) : RequestChannel (_name, _side){
	s1 = "/MQ_" + my_name + "1";
	s2 = "/MQ_" + my_name + "2";
		
	if (_side == SERVER_SIDE){
		wfd = open_ipc(s1, O_RDWR | O_CREAT);
		rfd = open_ipc(s2, O_RDONLY | O_CREAT);
	}
	else{		
		rfd = open_ipc(s1, O_RDONLY | O_CREAT);
		wfd = open_ipc(s2, O_RDWR | O_CREAT);
	}
	
}

MQRequestChannel::~MQRequestChannel(){ 
	mq_close(wfd);
	mq_close(rfd);

	mq_unlink(s1.c_str());
	mq_unlink(s2.c_str());
}

int MQRequestChannel::open_ipc(string _pipe_name, int mode){
	struct mq_attr attrib;
	attrib.mq_flags = 0;
	attrib.mq_maxmsg = 4; 
	attrib.mq_msgsize = 300;
	attrib.mq_curmsgs = 0; 
	int fd = (int)mq_open(_pipe_name.c_str(), O_RDWR | O_CREAT, 0600, &attrib); // 0 later
	if (fd < 0){
		EXITONERROR(_pipe_name);
	}
	return fd;
}

int MQRequestChannel::cread(void* msgbuf, int bufcapacity){
	return mq_receive(rfd, (char *)msgbuf, 8192, NULL); 
}

int MQRequestChannel::cwrite(void* msgbuf, int len){
	return mq_send(wfd, (char *) msgbuf, len, 0);
}

