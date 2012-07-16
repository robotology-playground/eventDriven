// -*- mode:C++; tab-width:4; c-basic-offset:4; indent-tabs-mode:nil -*-

/*
  * Copyright (C)2012  Department of Robotics Brain and Cognitive Sciences - Istituto Italiano di Tecnologia
  * Author:Francesco Rea
  * email: francesco.rea@iit.it
  * Permission is granted to copy, distribute, and/or modify this program
  * under the terms of the GNU General Public License, version 2 or any
  * later version published by the Free Software Foundation.
  *
  * A copy of the license can be found at
  * http://www.robotcub.org/icub/license/gpl.txt
  *
  * This program is distributed in the hope that it will be useful, but
  * WITHOUT ANY WARRANTY; without even the implied warranty of
  * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
  * Public License for more details
*/

/**
 * @file velocityExtractorThread.cpp
 * @brief Implementation of the thread (see header velocityExtractorThread.h)
 */

#include <iCub/velocityExtractorThread.h>
#include <cstring>
#include <cassert>
#include <cstdlib>
#include <time.h>

using namespace yarp::os;
using namespace yarp::sig;
using namespace std;

#define dim_window   5
#define synch_time   1
#define INTERVFACTOR 1              // resolution 160ns = 6.25Mhz, 128ns = 7.8125Mhz ... gaep 1us
#define COUNTERRATIO 1              // 1.25 is the ratio 0.160/0.128
#define THRATE       5
#define MAXVALUE 0xFFFFFF //4294967295

#define STAMPINFRAME  // 10 ms of period times the us in 1 millisecond + time for computing
#define CHUNKSIZE 32768 //65536 //8192

//#define retinalSize 128


//#define VERBOSE

velocityExtractorThread::velocityExtractorThread() : RateThread(THRATE) {
    responseGradient = 127;
    retinalSize  = 128;  //default value before setting 
  
    synchronised = false;
    greaterHalf  = false;
    firstRun     = true;
    count        = 0;
    minCount     = 0; //initialisation of the timestamp limits of the first frame
    idle = false;
    bufferCopy = (char*) malloc(CHUNKSIZE);
    countStop = 0;
    verb = false;
    string i_fileName("events.log");
    raw = fopen(i_fileName.c_str(), "wb");
    lc = rc = 0;	
    minCount      = 0;
    minCountRight = 0;
}

velocityExtractorThread::~velocityExtractorThread() {
    printf("freeing memory in collector");
    delete bufferCopy;
}

bool velocityExtractorThread::threadInit() {
    printf(" \n------------------------- velocityExtractorThread::threadInit:starting the threads.... \n");
    //outPort.open(getName("/left:o").c_str());
    //outPortRight.open(getName("/right:o").c_str());

    fout = fopen("./dump.txt","w+");
    resize(retinalSize, retinalSize);

    printf("starting the converter!!!.... \n");
    
    //cfConverter=new cFrameConverter();
    //cfConverter->useCallback();
    //cfConverter->setRetinalSize(retinalSize);
    //cfConverter->open(getName("/retina:i").c_str());
    
    printf("\n opening retina\n");
    printf("starting the plotter \n");

    //minCount = cfConverter->getEldestTimeStamp();
    startTimer = Time::now();
    //clock(); //startTime ;
    //T1 = times(&start_time);
    //microseconds = 0;
    //microsecondsPrev = 0;
    //gettimeofday(&tvend, NULL);

    count = 0;
    microsecondsPrev = 0;
    minCount = 0;
    minCountRight= 0;

    //bottleHandler = new eventBottleHandler();
    //bottleHandler->useCallback();
    //bottleHandler->setRetinalSize(retinalSize);
    //bottleHandler->open(getName("/retinaBottle:i").c_str());

    if(!outBottlePort.open(getName("/retinaBottle:o").c_str())) {
        printf("error in opening the output port \n");
        return false;
    }
    
    receivedBottle = new Bottle();
    bottleToSend   = new Bottle();


    printf("-----------------------------  velocityExtractorThread::threadInit:Initialisation of collector thread correctly ended \n");
    return true;
}

void velocityExtractorThread::interrupt() {
    outPort.interrupt();
    outPortRight.interrupt();
}

void velocityExtractorThread::setName(string str) {
    this->name=str;
    printf("name: %s", name.c_str());
}

std::string velocityExtractorThread::getName(const char* p) {
    string str(name);
    str.append(p);
    return str;
}

void velocityExtractorThread::resize(int widthp, int heightp) {
    imageLeft = new ImageOf<PixelMono>;
    imageLeft->resize(widthp,heightp);
    imageRight = new ImageOf<PixelMono>;
    imageRight->resize(widthp,heightp);
}


void velocityExtractorThread::getMonoImage(ImageOf<yarp::sig::PixelMono>* image, unsigned long minCount,unsigned long maxCount, bool camera){

}

int velocityExtractorThread::prepareUnmasking(char* bufferCopy, Bottle* res) {
    // navigate the 32bit words in the bufferCopy and create a bottle outofvalid
    int numberofwords = CHUNKSIZE / 4; //4bytes made a 32bits word
    u32* pointerWord = (u32*) bufferCopy;
    int countValid  = 0;
    for (int i = 0; i< numberofwords; i ++) {
        
        if(*pointerWord != 0) {
            //this is a valid word
            Bottle tmp;
            tmp.addInt(*pointerWord);
            if(i == 100) {
                printf("%08X \n", *pointerWord);
            }
            res->append(tmp);
            countValid++;
            pointerWord++;
        }
    }
    return countValid;
}

void velocityExtractorThread::run() {
  
    while(!isRunning()) {
        if(inBottlePort.getInputCount()) {
            Bottle* readBottle = inBottlePort.read(true);
            printf("bottle %s \n", readBottle->toString().c_str());
        }
        else {
            Time::delay(0.5);
        }
    }
}

void velocityExtractorThread::threadRelease() {
    idle = false;
    fclose(fout);
    printf("velocityExtractorThread release:freeing bufferCopy \n");
    //free(bufferCopy);
    printf("velocityExtractorThread release:closing ports \n");
    //outPort.close();
    outBottlePort.close();
    //outPortRight.close();

    // delete bottleHandler;
    delete receivedBottle;
    delete bottleToSend;
    printf("correctly freed memory from the bottleHandler \n");
}


//----- end-of-file --- ( next line intentionally left blank ) ------------------

