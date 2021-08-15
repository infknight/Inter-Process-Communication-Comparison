/*
    Tanzir Ahmed
    Department of Computer Science & Engineering
    Texas A&M University
    Date  : 2/8/19
 */
#include "common.h"
#include <sys/wait.h>
#include "FIFOreqchannel.h"
#include "MQreqchannel.h"
#include "SHMreqchannel.h"
#include <vector>
using namespace std;


int main(int argc, char *argv[]){
    
    int c;
    ibuffercap = MAX_MESSAGE;
    int p = 0, ecg = 1;
    double t = -1.0;
    bool isnewchan = false;
    bool isfiletransfer = false;
    string filename;
    string ipcmethod = "f"; 
    int nchannels = 1; 


    while ((c = getopt (argc, argv, "p:t:e:m:f:c:i:")) != -1){
        switch (c){
            case 'p':
                p = atoi (optarg);
                break;
            case 't':
                t = atof (optarg);
                break;
            case 'e':
                ecg = atoi (optarg);
                break;
            case 'm':
                buffercap = atoi (optarg);
                break;
            case 'c':
                isnewchan = true;
                nchannels = atoi(optarg); 
                break;
            case 'f':
                isfiletransfer = true;
                filename = optarg;
                break;
            case 'i':
                ipcmethod = optarg;
                break; 
        }
    }
    
    // fork part
    if (fork()==0){ // child 
	
		char* args [] = {"./server", "-m", (char *) to_string(buffercap).c_str(), "-i", (char *)ipcmethod.c_str(),  NULL};
        if (execvp (args [0], args) < 0){
            perror ("exec filed");
            exit (0);
        }
    }
''

    RequestChannel* control_chan = NULL;
    if(ipcmethod == "f") 
        control_chan =  new FIFORequestChannel ("control", FIFORequestChannel::CLIENT_SIDE);
    else if(ipcmethod == "q")
        control_chan =  new MQRequestChannel ("control", FIFORequestChannel::CLIENT_SIDE);
    else if(ipcmethod == "m")
        control_chan =  new SHMRequestChannel ("control", FIFORequestChannel::CLIENT_SIDE, buffercap);

    vector<RequestChannel*> vec_chan; 
    // RequestChannel* chan = control_chan;
    RequestChannel* chan = NULL;
    int time_t2 = 0; 


    if (isnewchan){
        for(int i = 0; i < nchannels; i++){
            cout << "Using the new channel everything following" << endl;
            MESSAGE_TYPE m = NEWCHANNEL_MSG;
            control_chan->cwrite (&m, sizeof (m));
            char newchanname [100];
            control_chan->cread (newchanname, sizeof (newchanname));
            if(ipcmethod == "f")
                chan = new FIFORequestChannel (newchanname, RequestChannel::CLIENT_SIDE);
            else if(ipcmethod == "q")
                chan =  new MQRequestChannel (newchanname, RequestChannel::CLIENT_SIDE);
            else if(ipcmethod == "m")
                chan =  new SHMRequestChannel (newchanname, RequestChannel::CLIENT_SIDE,buffercap);
            vec_chan.push_back(chan); 
            cout << "New channel by the name " << newchanname << " is created" << endl;
            cout << "All further communication will happen through it instead of the main channel" << endl;
        }

    }


    if (!isfiletransfer){   // requesting data msgs
        int time_t; 
        struct timeval st, ed; 
        gettimeofday(&st, NULL); 
        ios_base::sync_with_stdio(false);
        for(auto& chan : vec_chan){
            // double st_s = st.tv_sec; 
            // double st_mil = st.tv_usec;
            // MESSAGE_TYPE q = QUIT_MSG; 
            // chan->cwrite(&q, sizeof(MESSAGE_TYPE)); 

            if (t >= 0){    // 1 data point
                datamsg d (p, t, ecg);
                chan->cwrite (&d, sizeof (d));
                double ecgvalue;
                chan->cread (&ecgvalue, sizeof (double));
                cout << "Ecg " << ecg << " value for patient "<< p << " at time " << t << " is: " << ecgvalue << endl;
            }else{          // bulk (i.e., 1K) data requests 
                double ts = 0;  
                datamsg d (p, ts, ecg);
                double ecgvalue;
                for (int i=0; i<1000; i++){
                    chan->cwrite (&d, sizeof (d));
                    chan->cread (&ecgvalue, sizeof (double));
                    d.seconds += 0.004; //increment the timestamp by 4ms
                    // cout << ecgvalue << endl;
                }
            }
            // time+= .. 
            // gettimeofday(&ed, NULL);
            // double end_s = ed.tv_sec;
            // double end_mil = ed.tv_usec;
            // double end_st = (end_s - st_s) * 1000 + (end_mil - st_mil)/1000; 
            // cout << "time for this channel: " << end_st << "ms" << endl; 
            // time_t+=end_st; 

        }
        gettimeofday(&ed, NULL);
        cout << "Time total " << ((ed.tv_sec *1000000 + ed.tv_usec)
        - (st.tv_sec * 1000000 + st.tv_usec)) << "ms" << endl; 
        // cout <<"total time is : " << time_t << endl;

    }
    else if (isfiletransfer){
        // part 2 requesting a file
        filemsg f (0,0);  // special first message to get file size
        int to_alloc = sizeof (filemsg) + filename.size() + 1; // extra byte for NULL
        char* buf = new char [to_alloc];
        memcpy (buf, &f, sizeof(filemsg));
        strcpy (buf + sizeof (filemsg), filename.c_str());
        // chan->cwrite (buf, to_alloc);
        control_chan->cwrite (buf, to_alloc);

        __int64_t filesize;
        // chan->cread (&filesize, sizeof (__int64_t));
        control_chan->cread (&filesize, sizeof (__int64_t));
        cout << "File size: " << filesize << endl;

        //int transfers = ceil (1.0 * filesize / MAX_MESSAGE);
        filemsg* fm = (filemsg*) buf;
        __int64_t rem = filesize;
        string outfilepath = string("received/") + filename;
        FILE* outfile = fopen (outfilepath.c_str(), "wb");  
        fm->offset = 0;

        char* recv_buffer = new char [MAX_MESSAGE];
        int i = 0; 
        struct timeval st, ed; 
        gettimeofday(&st, NULL); 
        ios_base::sync_with_stdio(false);
        while (rem>0){
            // if(i >= nchannels)
            //     break;
            fm->length = (int) min (rem, (__int64_t) MAX_MESSAGE);
            // struct timeval st, ed; 
            // ios_base::sync_with_stdio(false);
            // gettimeofday(&st, NULL); 
            // double st_s = st.tv_sec; 
            // double st_mil = st.tv_usec;
            if(isnewchan){  // do i have to loop thru? 

                vec_chan[i]->cwrite (buf, to_alloc);
                vec_chan[i]->cread (recv_buffer, buffercap);
                // gettimeofday(&ed, NULL);
                // double end_s = ed.tv_sec;
                // double end_mil = ed.tv_usec;
                // double end_st = (end_s - st_s) * 1000 + (end_mil - st_mil)/1000; 
                // cout << "time for this channel: " << end_st << "ms" << endl; 
                // time_t2+=end_st; 
                // cout << "T2: "<<  time_t2 << endl; 
            }
            else{
                chan->cwrite (buf, to_alloc);
                chan->cread (recv_buffer, buffercap);
            }
            fwrite (recv_buffer, 1, fm->length, outfile);
            rem -= fm->length;
            fm->offset += fm->length;
            //cout << fm->offset << endl;
            i++;
            i%=nchannels;
        }
        gettimeofday(&ed, NULL);
        cout << "Time total " << ((ed.tv_sec *1000000 + ed.tv_usec)
        - (st.tv_sec * 1000000 + st.tv_usec)) << "ms" << endl; 
        // cout <<"total time is : " << time_t << endl;
        fclose (outfile);
        delete recv_buffer;
        delete buf;
        cout << "File transfer completed" << endl;
        // cout <<"total time is : " << time_t2 << "ms" << endl;
    }




    MESSAGE_TYPE q = QUIT_MSG;
    chan->cwrite (&q, sizeof (MESSAGE_TYPE));
    for(auto &i : vec_chan){
        i->cwrite (&q, sizeof (MESSAGE_TYPE));
        delete i; 
    }

    if (chan != control_chan){ // this means that the user requested a new channel, so the control_channel must be destroyed as well 
        control_chan->cwrite (&q, sizeof (MESSAGE_TYPE));
        delete control_chan; 
    }
	// wait for the child process running server
    // this will allow the server to properly do clean up
    // if wait is not used, the server may sometimes crash
	wait (0);
    
}
