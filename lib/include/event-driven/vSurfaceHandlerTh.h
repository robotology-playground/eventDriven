/*
 *   Copyright (C) 2017 Event-driven Perception for Robotics
 *   Author: arren.glover@iit.it
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU Lesser General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public License
 *   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef __VSURFACEHANDLER__
#define __VSURFACEHANDLER__

#include <yarp/os/all.h>
#include "event-driven/vBottle.h"
#include "event-driven/vCodec.h"
#include "event-driven/vWindow_basic.h"
#include "event-driven/vWindow_adv.h"
#include "event-driven/vFilters.h"
#include "event-driven/vPort.h"
#include <deque>
#include <string>
#include <map>

namespace ev {

/// \brief an asynchronous reading port that accepts vBottles and decodes them
class queueAllocator : public yarp::os::BufferedPort<ev::vBottle>
{
private:

    std::deque<ev::vQueue *> qq;
    std::deque<yarp::os::Stamp> sq;
    std::mutex m;
    yarp::os::Semaphore dataready;

    unsigned int qlimit;
    unsigned int delay_nv;
    long unsigned int delay_t;
    double event_rate;

public:

    /// \brief constructor
    queueAllocator()
    {
        qlimit = 0;
        delay_nv = 0;
        delay_t = 0;
        event_rate = 0;

        dataready.wait();

        useCallback();
        setStrict();
    }

    /// \brief desctructor
    ~queueAllocator()
    {
        m.lock();
        for(std::deque<ev::vQueue *>::iterator i = qq.begin(); i != qq.end(); i++)
            delete *i;
        qq.clear();
        m.unlock();
    }

    /// \brief the callback decodes the incoming vBottle and adds it to the
    /// list of received vBottles. The yarp, and event timestamps are updated.
    void onRead(ev::vBottle &inputbottle)
    {
        //make a new vQueue
        m.lock();

        if(qlimit && qq.size() >= qlimit) {
            m.unlock();
            return;
        }
        qq.push_back(new vQueue);
        yarp::os::Stamp yarpstamp;
        getEnvelope(yarpstamp);
        sq.push_back(yarpstamp);

        m.unlock();


        //and decode the data
        inputbottle.addtoendof<ev::AddressEvent>(*(qq.back()));

        //update the meta data
        m.lock();
        delay_nv += qq.back()->size();
        int dt = qq.back()->back()->stamp - qq.back()->front()->stamp;
        if(dt < 0) dt += vtsHelper::max_stamp;
        delay_t += dt;
        if(dt)
            event_rate = qq.back()->size() / (double)dt;
        m.unlock();

        //if getNextQ is blocking - let it get the new data
        dataready.post();
    }

    /// \brief ask for a pointer to the next vQueue. Blocks if no data is ready.
    ev::vQueue* read(yarp::os::Stamp &yarpstamp)
    {
        static vQueue * working_queue = nullptr;
        if(working_queue) {
            m.lock();

            delay_nv -= qq.front()->size();
            int dt = qq.front()->back()->stamp - qq.front()->front()->stamp;
            if(dt < 0) dt += vtsHelper::max_stamp;
            delay_t -= dt;

            delete qq.front();
            qq.pop_front();
            sq.pop_front();
            m.unlock();
        }
        dataready.wait();
        if(qq.size()) {
            yarpstamp = sq.front();
            working_queue = qq.front();
            return working_queue;
        }  else {
            return 0;
        }

    }

    /// \brief remove the most recently read vQueue from the list and deallocate
    /// the memory
    void scrapQ()
    {
        m.lock();

        delay_nv -= qq.front()->size();
        int dt = qq.front()->back()->stamp - qq.front()->front()->stamp;
        if(dt < 0) dt += vtsHelper::max_stamp;
        delay_t -= dt;

        delete qq.front();
        qq.pop_front();
        sq.pop_front();
        m.unlock();
    }

    /// \brief set the maximum number of qs that can be stored in the buffer.
    /// A value of 0 keeps all qs.
    void setQLimit(unsigned int number_of_qs)
    {
        qlimit = number_of_qs;
    }

    /// \brief unBlocks the blocking call in getNextQ. Useful to ensure a
    /// graceful shutdown. No guarantee the return of getNextQ will be valid.
    void releaseDataLock()
    {
        dataready.post();
    }

    /// \brief ask for the number of vQueues currently allocated.
    int queryunprocessed()
    {
        return qq.size();
    }

    /// \brief ask for the number of events in all vQueues.
    unsigned int queryDelayN()
    {
        return delay_nv;
    }

    /// \brief ask for the total time spanned by all vQueues.
    double queryDelayT()
    {
        return delay_t * vtsHelper::tsscaler;
    }

    /// \brief ask for the high precision event rate
    double queryRate()
    {
        return event_rate * vtsHelper::vtsscaler;
    }

    std::string delayStatString()
    {
        std::ostringstream oss;
        oss << queryunprocessed() << " " << queryDelayN() <<
               " " << queryDelayT() << " " << queryRate();
        return oss.str();
    }

};

/// \brief asynchronously read events and push them in a vSurface
class surfaceThread : public yarp::os::Thread
{
private:

    ev::temporalSurface surfaceLeft;
    ev::temporalSurface surfaceRight;

    queueAllocator allocatorCallback;

    std::mutex m;
    yarp::os::Stamp yarpstamp;
    unsigned int ctime;

    int vcount;


public:

    surfaceThread()
    {
        vcount = 0;
        ctime = 0;
    }

    void configure(int height, int width)
    {
        surfaceLeft = ev::temporalSurface(width, height);
        surfaceRight = ev::temporalSurface(width, height);
    }

    bool open(std::string portname)
    {
        if(!allocatorCallback.open(portname))
            return false;
        start();
        return true;
    }

    void onStop()
    {
        allocatorCallback.close();
        allocatorCallback.releaseDataLock();
    }

    void run()
    {
        while(true) {

            ev::vQueue *q = 0;
            while(!q && !isStopping()) {
                q = allocatorCallback.read(yarpstamp);
            }
            if(isStopping()) break;

            for(ev::vQueue::iterator qi = q->begin(); qi != q->end(); qi++) {

                m.lock();

                vcount++;

                ctime = (*qi)->stamp;

                if((*qi)->getChannel() == 0)
                    surfaceLeft.fastAddEvent(*qi);
                else if((*qi)->getChannel() == 1)
                    surfaceRight.fastAddEvent(*qi);
                else
                    std::cout << "Unknown channel" << std::endl;

                m.unlock();

            }

            //allocatorCallback.scrapQ();

        }

    }

    yarp::os::Stamp queryROI(ev::vQueue &fillq, int c, unsigned int t, int x, int y, int r)
    {

        //if(!vcount) return false;

        m.lock();
        if(c == 0)
            fillq = surfaceLeft.getSurf_Tlim(t, x, y, r);
        else
            fillq = surfaceRight.getSurf_Tlim(t, x, y, r);
        vcount = 0;
        m.unlock();
        return yarpstamp;
    }

    yarp::os::Stamp queryWindow(ev::vQueue &fillq, int c, unsigned int t)
    {

        m.lock();
        if(c == 0)
            fillq = surfaceLeft.getSurf_Tlim(t);
        else
            fillq = surfaceRight.getSurf_Tlim(t);
        vcount = 0;
        m.unlock();

        return yarpstamp;
    }

    unsigned int queryVTime()
    {
        return ctime;
    }

};

/// \brief asynchronously read events and push them in a historicalSurface
class hSurfThread : public yarp::os::Thread
{
private:

    int maxcpudelay; //maximum delay between v time and cpu time (in v time)

    queueAllocator allocatorCallback;
    historicalSurface surfaceleft;
    historicalSurface surfaceright;
    std::mutex m;

    //current stamp to propagate
    yarp::os::Stamp ystamp;
    unsigned int vstamp;

    //synchronising value (add to it when stamps come in, subtract from it
    // when querying events).
    double cputimeL;
    int cpudelayL;
    double cputimeR;
    int cpudelayR;

public:

    hSurfThread()
    {
        vstamp = 0;
        cpudelayL = cpudelayR = 0;
        cputimeL = cputimeR = yarp::os::Time::now();
        maxcpudelay = 0.05 * vtsHelper::vtsscaler;
    }

    void configure(int height, int width, double maxcpudelay)
    {
        this->maxcpudelay = maxcpudelay * vtsHelper::vtsscaler;
        surfaceleft.initialise(height, width);
        surfaceright.initialise(height, width);
    }

    bool open(std::string portname)
    {
        if(!allocatorCallback.open(portname))
            return false;

        start();
        return true;
    }

    void onStop()
    {
        allocatorCallback.close();
        allocatorCallback.releaseDataLock();
    }


    void run()
    {
        static int maxqs = 4;
        bool allowproc = true;

        while(true) {

            ev::vQueue *q = 0;
            while(!q && !isStopping()) {
                q = allocatorCallback.read(ystamp);
            }
            if(isStopping()) break;


            int nqs = allocatorCallback.queryunprocessed();

            if(allowproc)
                m.lock();

            if(nqs >= maxqs)
                allowproc = false;
            else if(nqs < maxqs)
                allowproc = true;

            int dt = q->back()->stamp - vstamp;
            if(dt < 0) dt += vtsHelper::max_stamp;
            cpudelayL += dt;
            cpudelayR += dt;
            vstamp = q->back()->stamp;

            for(ev::vQueue::iterator qi = q->begin(); qi != q->end(); qi++) {

                if((*qi)->getChannel() == 0)
                    surfaceleft.addEvent(*qi);
                else if((*qi)->getChannel() == 1)
                    surfaceright.addEvent(*qi);

            }

            if(allowproc)
                m.unlock();

            //allocatorCallback.scrapQ();

        }

    }

    vQueue queryROI(int channel, int numEvts, int r)
    {

        vQueue q;

        m.lock();
        double cpunow = yarp::os::Time::now();

        if(channel == 0) {

            cpudelayL -= (cpunow - cputimeL) * vtsHelper::vtsscaler * 1.1;
            cputimeL = cpunow;

            if(cpudelayL < 0) cpudelayL = 0;
            if(cpudelayL > maxcpudelay) {
                yWarning() << "CPU delay hit maximum";
                cpudelayL = maxcpudelay;
            }

            surfaceleft.getSurfaceN(q, cpudelayL, numEvts, r);
        }
        else {

            cpudelayR -= (cpunow - cputimeR) * vtsHelper::vtsscaler * 1.1;
            cputimeR = cpunow;

            if(cpudelayR < 0) cpudelayR = 0;
            if(cpudelayR > maxcpudelay) {
                yWarning() << "CPU delay hit maximum";
                cpudelayR = maxcpudelay;
            }

            surfaceright.getSurfaceN(q, cpudelayR, numEvts, r);
        }

        m.unlock();

        return q;
    }

    vQueue queryROI(int channel, unsigned int querySize, int x, int y, int r)
    {


        vQueue q;

        m.lock();

        double cpunow = yarp::os::Time::now();

        if(channel == 0) {

            cpudelayL -= (cpunow - cputimeL) * vtsHelper::vtsscaler * 1.01;
            cputimeL = cpunow;

            if(cpudelayL < 0) cpudelayL = 0;
            if(cpudelayL > maxcpudelay) {
                yWarning() << "CPU delay hit maximum";
                cpudelayL = maxcpudelay;
            }

            q = surfaceleft.getSurface(cpudelayL, querySize, r, x, y);
        } else {

            cpudelayR -= (cpunow - cputimeR) * vtsHelper::vtsscaler * 1.01;
            cputimeR = cpunow;

            if(cpudelayR < 0) cpudelayR = 0;
            if(cpudelayR > maxcpudelay) {
                yWarning() << "CPU delay hit maximum";
                cpudelayR = maxcpudelay;
            }

            q = surfaceright.getSurface(cpudelayR, querySize, r, x, y);
        }

        m.unlock();

        return q;
    }

    vQueue queryWindow(int channel, unsigned int querySize)
    {
        vQueue q;

        m.lock();

        double cpunow = yarp::os::Time::now();

        if(channel == 0) {

            cpudelayL -= (cpunow - cputimeL) * vtsHelper::vtsscaler * 1.01;
            cputimeL = cpunow;

            if(cpudelayL < 0) cpudelayL = 0;
            if(cpudelayL > maxcpudelay) {
                yWarning() << "CPU delay hit maximum";
                cpudelayL = maxcpudelay;
            }

            q = surfaceleft.getSurface(cpudelayL, querySize);
        }
        else {

            cpudelayR -= (cpunow - cputimeR) * vtsHelper::vtsscaler * 1.01;
            cputimeR = cpunow;

            if(cpudelayR < 0) cpudelayR = 0;
            if(cpudelayR > maxcpudelay) {
                yWarning() << "CPU delay hit maximum";
                cpudelayR = maxcpudelay;
            }

            q = surfaceright.getSurface(cpudelayR, querySize);
        }

        m.unlock();

        return q;
    }

    double queryDelay(int channel = 0)
    {
        if(channel) {
            return cpudelayR * vtsHelper::tsscaler;
        } else {
            return cpudelayL * vtsHelper::tsscaler;
        }
    }

    yarp::os::Stamp queryYstamp()
    {
        return ystamp;
    }

    int queryVstamp(int channel = 0)
    {
        int modvstamp;
        m.lock();
        if(channel) {
            modvstamp = vstamp - cpudelayR;
        } else {
            modvstamp = vstamp - cpudelayL;
        }
        m.unlock();

        if(modvstamp < 0) modvstamp += vtsHelper::max_stamp;
        return modvstamp;

    }

    int queryQDelay()
    {
        return allocatorCallback.queryunprocessed();
    }

};

/// \brief automatically accept events from a port and push them into a
/// vTempWindow
class tWinThread : public yarp::os::Thread
{
private:

    ev::vReadPort<vQueue> allocatorCallback;
    //ev::queueAllocator allocatorCallback;
    vTempWindow windowleft;
    vTempWindow windowright;

    std::mutex safety;

    int strictUpdatePeriod;
    int currentPeriod;
    std::mutex waitforquery;
    yarp::os::Stamp yarpstamp;
    unsigned int ctime;
    bool updated;

public:

    tWinThread()
    {
        ctime = 0;
        strictUpdatePeriod = 0;
        currentPeriod = 0;
        updated = false;
    }

    bool open(std::string portname, int period = 0)
    {
        strictUpdatePeriod = period;
        if(strictUpdatePeriod) yInfo() << "Forced update every" << period * vtsHelper::tsscaler <<"s, or"<< period << "event timestamps";
        if(!allocatorCallback.open(portname))
            return false;

        return start();
    }

    void onStop()
    {
        allocatorCallback.close();
        //allocatorCallback.releaseDataLock();
        waitforquery.unlock();
    }

    void run()
    {
        if(strictUpdatePeriod) {
            safety.lock();
            waitforquery.lock();
        }

        while(!isStopping()) {


            const ev::vQueue *q = allocatorCallback.read(yarpstamp);
            if(!q) break;

            if(!strictUpdatePeriod) safety.lock();

            if(!ctime) ctime = q->front()->stamp;

            for(ev::vQueue::const_iterator qi = q->begin(); qi != q->end(); qi++) {
                if((*qi)->getChannel() == 0)
                    windowleft.addEvent(*qi);
                else if((*qi)->getChannel() == 1)
                    windowright.addEvent(*qi);
            }

            if(strictUpdatePeriod) {
                int dt = q->back()->stamp - ctime;
                if(dt < 0) dt += vtsHelper::max_stamp;
                currentPeriod += dt;
                if(currentPeriod > strictUpdatePeriod) {
                    safety.unlock();
                    waitforquery.lock();
                    safety.lock();
                    currentPeriod = 0;
                }

            }

            ctime = q->back()->stamp;

            updated = true;

            if(!strictUpdatePeriod) safety.unlock();

        }
        if(strictUpdatePeriod)
            safety.unlock();
    }

    vQueue queryWindow(int channel)
    {
        vQueue q;

        safety.lock();
        //std::cout << "vFramer unprcd: " << allocatorCallback.queryunprocessed() << std::endl;
        if(channel == 0)
            q = windowleft.getWindow();
        else
            q = windowright.getWindow();
        updated = false;
        waitforquery.unlock();
        safety.unlock();
        return q;
    }

    void queryStamps(yarp::os::Stamp &yStamp, int &vStamp)
    {
        yStamp = yarpstamp;
        vStamp = ctime;
    }

    bool queryUpdated()
    {
        return updated;
    }

    unsigned int queryUnprocd()
    {
        return allocatorCallback.queryunprocessed();
    }

    std::string readDelayStats()
    {
        return allocatorCallback.delayStatString();
    }

};

/// \brief automatically accept multiple event types from different ports
/// (e.g. as in the vFramer)
class syncvstreams
{
private:

    std::map<std::string, ev::tWinThread> iPorts;
    //std::deque<tWinThread> iPorts;
    yarp::os::Stamp yStamp;
    int vStamp;
    int strictUpdatePeriod;
    bool using_yarp_stamps;
    //std::map<std::string, int> labelMap;

public:

    syncvstreams(void)
    {
        strictUpdatePeriod = 0;
        vStamp = 0;
        using_yarp_stamps = false;
    }

    bool open(std::string moduleName, std::string eventType)
    {
        //check already have an input of that type
        if(iPorts.count(eventType))
            return true;

        //otherwise open a new port
        if(!iPorts[eventType].open(moduleName + "/" + eventType + ":i", strictUpdatePeriod))
            return false;

        return true;
    }

    vQueue queryWindow(std::string vType, int channel)
    {

        updateStamps();
        return iPorts[vType].queryWindow(channel);
    }

    void updateStamps()
    {
        //query each input port and ask for the timestamp
        yarp::os::Stamp ys; int vs;
        std::map<std::string, ev::tWinThread>::iterator i;
        for(i = iPorts.begin(); i != iPorts.end(); i++) {
            i->second.queryStamps(ys, vs);

            //we assume envelopes aren't being used
            if(!using_yarp_stamps) vStamp = vs;
            if(!ys.isValid()) continue;

            //set the stamps based on the yarp_stamp
            using_yarp_stamps = true;
            double pt = yStamp.getTime();
            double ct = ys.getTime();
            //if we have a more recent packet, or we went back in time 5 seconds
            if(ct > pt || ct < pt - 5.0) {
                yStamp = ys;
                vStamp = vs;
            }
        }
    }

    void close()
    {
        std::map<std::string, ev::tWinThread>::iterator i;
        for(i = iPorts.begin(); i != iPorts.end(); i++)
            i->second.stop();
    }

    yarp::os::Stamp getystamp()
    {
        return yStamp;
    }

    int getvstamp()
    {
        return vStamp;
    }

    void setStrictUpdatePeriod(int period)
    {
        strictUpdatePeriod = period;
    }

    bool hasUpdated()
    {
        if(strictUpdatePeriod) return true;
        std::map<std::string, ev::tWinThread>::iterator i;
        for(i = iPorts.begin(); i != iPorts.end(); i++)
            if(i->second.queryUpdated()) return true;
        return false;
    }

    unsigned int queryMaxUnproced()
    {
        unsigned int unprocd = 0;
        std::map<std::string, ev::tWinThread>::iterator i;
        for(i = iPorts.begin(); i != iPorts.end(); i++)
            unprocd = std::max(i->second.queryUnprocd(), unprocd);
        return unprocd;
    }

    std::string delayStats()
    {
        std::ostringstream oss;
        std::map<std::string, ev::tWinThread>::iterator i;
        for(i = iPorts.begin(); i != iPorts.end(); i++)
            oss << i->first << ": " << i->second.readDelayStats() << " ";

        return oss.str();
    }

};

}

#endif
