/*
 *   Copyright (C) 2017 Event-driven Perception for Robotics
 *   Author: arren.glover@iit.it
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "vRepTest.h"

using namespace ev;

/**********************************************************/
bool vRepTestHandler::configure(yarp::os::ResourceFinder &rf)
{
    //set the name of the module
    std::string moduleName =
            rf.check("name", yarp::os::Value("vRepTest")).asString();
    setName(moduleName.c_str());

    std::string vis = rf.check("vis", yarp::os::Value("all")).asString();

    bool strict = rf.check("strict") &&
            rf.check("strict", yarp::os::Value(true)).asBool();


    reptest.setVisType(vis);
    //reptest.setTemporalWindow(rf.check("tWin", yarp::os::Value(125000)).asInt32());
    reptest.setFixedWindow(rf.check("fWin", yarp::os::Value(1000)).asInt32());

    /* create the thread and pass pointers to the module parameters */
    return reptest.open(moduleName, strict);

    return true ;
}

/**********************************************************/
bool vRepTestHandler::interruptModule()
{
    reptest.interrupt();
    yarp::os::RFModule::interruptModule();
    return true;
}

/**********************************************************/
bool vRepTestHandler::close()
{
    reptest.close();
    yarp::os::RFModule::close();
    return true;
}

/**********************************************************/
bool vRepTestHandler::updateModule()
{
    return true;
}

/**********************************************************/
double vRepTestHandler::getPeriod()
{
    return 1;
}

/**********************************************************/
vRepTest::vRepTest()
{
    edge.track();
    fWindow.setFixedWindowSize(1000);
    tWindow.setTemporalSize(125000);
    edge.setThickness(1);
    ytime = 0;
    //here we should initialise the module

}
/**********************************************************/
bool vRepTest::open(const std::string &name, bool strict)
{
    //and open the input port

    this->useCallback();
    if(strict) this->setStrict();

    yarp::os::BufferedPort<ev::vBottle>::open("/" + name + "/vBottle:i");

    dumper.open("/" + name + "/dump:o");
    eventsOut.open("/" + name + "/vBottle:o");
    imPort.open("/" + name + "/image:o");

    return true;
}

/**********************************************************/
void vRepTest::close()
{
    //close ports
    dumper.close();
    yarp::os::BufferedPort<ev::vBottle>::close();

    //remember to also deallocate any memory allocated by this class


}

/**********************************************************/
void vRepTest::interrupt()
{
    //pass on the interrupt call to everything needed
    dumper.interrupt();
    yarp::os::BufferedPort<ev::vBottle>::interrupt();

}

/**********************************************************/
void vRepTest::onRead(ev::vBottle &inBottle)
{
    yarp::os::Stamp yts; getEnvelope(yts);
    if(ytime == 0) ytime = yts.getTime() + 0.033;
    unsigned long unwts = 0;

    //create event queue
    ev::vQueue q = inBottle.getAll();
    ev::qsort(q, true);
    for(ev::vQueue::iterator qi = q.begin(); qi != q.end(); qi++)
    {
        auto ae = as_event<AE>(*qi);
        if(!ae || ae->getChannel()) continue;
        //the following is a hack until all hard-coded values can be removed
        //from the module
        if(ae->x > 127 || ae->y > 127) continue;

        unwts = unwrapper((*qi)->stamp);
        tWindow.addEvent(*qi);
        fWindow.addEvent(*qi);
        lWindow.addEvent(*qi);
        edge.addEventToEdge(ae);
        fedge.addEventToEdge(ae);
    }

    //dump modified dataset
    if(eventsOut.getOutputCount() && q.size()) {
        ev::vBottle &outBottle = eventsOut.prepare();
        outBottle.clear();

        for(ev::vQueue::iterator qi = q.begin(); qi != q.end(); qi++)
        {
            auto v = as_event<AE>(*qi);
            if(v && v->x < 128)
                outBottle.addEvent(*qi);
        }

        eventsOut.setEnvelope(yts);
        eventsOut.writeStrict();
    }

    //dump statistics
    if(dumper.getOutputCount() && q.size()) {
        yarp::os::Bottle &outBottle = dumper.prepare();
        outBottle.clear();
        outBottle.addInt64(unwts);
        outBottle.addInt32(tWindow.getEventCount());
        outBottle.addInt32(fWindow.getEventCount());
        outBottle.addInt32(lWindow.getEventCount());
        outBottle.addInt32(edge.getEventCount());

        dumper.setEnvelope(yts);
        dumper.writeStrict();
    }

    //make debug image
    if(yts.getTime() < ytime - 0.01)
        ytime = yts.getTime();

    if(imPort.getOutputCount() && yts.getTime() > ytime) {
        ytime += 0.01;
        yarp::sig::ImageOf<yarp::sig::PixelBgr> &image = imPort.prepare();

        if(vistype == "all") {
            image.resize(128 * 3 + 20, 128 * 2 + 15);
            image.zero();
            drawDebug(image, tWindow.getSurf(), 5, 5);
            drawDebug(image, fWindow.getSurf(), 5, 127 + 10);
            drawDebug(image, lWindow.getSurf(), 127 + 10, 5);
            drawDebug(image, edge.getSurf(0, 127, 0, 127), 127+10, 127+10);
            drawDebug(image, fedge.getSURF(0, 127, 0, 127), 127+127+15, 127+10);
        } else {
            image.resize(128, 128);
            image.zero();
        }

        if(vistype == "time")
            drawDebug(image, tWindow.getSurf(), 0, 0);
        else if(vistype == "fixed")
            drawDebug(image, fWindow.getSurf(), 0, 0);
        else if(vistype == "life")
            drawDebug(image, lWindow.getSurf(), 0, 0);
        else if(vistype == "edge")
            drawDebug(image, edge.getSurf(0, 127, 0, 127), 0, 0);
        else if(vistype == "fedge")
            drawDebug(image, fedge.getSURF(0, 127, 0, 127), 0, 0);

        imPort.setEnvelope(yts);
        imPort.writeStrict();
    }

}

void vRepTest::drawDebug(yarp::sig::ImageOf<yarp::sig::PixelBgr> &image,
                         const ev::vQueue &q, int xoff, int yoff)
{

    for(unsigned int i = 0; i < q.size(); i++) {
        auto v = as_event<AE>(q[i]);
//        if(q[i]->getAs<eventdriven::FlowEvent>())
//            image(v->getY()+yoff, image.width() - 1 - v->getX() - xoff) =
//                    yarp::sig::PixelBgr(0, 255, 0);
//        else
            image(v->y+xoff, image.height() - 1 - v->x - yoff) =
                    yarp::sig::PixelBgr(255, 0, 255);
    }

}

