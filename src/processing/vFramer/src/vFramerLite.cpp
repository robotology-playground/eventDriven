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

#include "vFramerLite.h"
#include <sstream>
#include <yarp/cv/Cv.h>
#include "event-driven/vDrawSkin.h"

using namespace ev;

int main(int argc, char * argv[])
{
    yarp::os::Network yarp;
    if(!yarp.checkNetwork()) {
        yError() << "Could not find yarp network";
        return 1;
    }

    yarp::os::ResourceFinder rf;
    rf.setVerbose( true );
    rf.setDefaultContext( "event-driven" );
    rf.setDefaultConfigFile( "vFramer.ini" );
    rf.configure( argc, argv );

    vFramerModule framerModule;
    return framerModule.runModule(rf);
}
/*////////////////////////////////////////////////////////////////////////////*/
// drawer factory
/*////////////////////////////////////////////////////////////////////////////*/
vDraw * createDrawer(std::string tag)
{

    if(tag == addressDraw::drawtype)
        return new addressDraw();
    if(tag == binaryDraw::drawtype)
        return new binaryDraw();
    if(tag == grayDraw::drawtype)
        return new grayDraw();
    if(tag == blackDraw::drawtype)
        return new blackDraw();
    if(tag == isoDraw::drawtype)
        return new isoDraw();
    if(tag == interestDraw::drawtype)
        return new interestDraw();
    if(tag == circleDraw::drawtype)
        return new circleDraw();
    if(tag == flowDraw::drawtype)
        return new flowDraw();
    if(tag == clusterDraw::drawtype)
        return new clusterDraw();
    if(tag == blobDraw::drawtype)
        return new blobDraw();
    if(tag == skinDraw::drawtype)
        return new skinDraw();
    if(tag == skinsampleDraw::drawtype)
        return new skinsampleDraw();
    if(tag == isoDrawSkin::drawtype)
        return new isoDrawSkin();
    if(tag == taxelsampleDraw::drawtype)
        return new taxelsampleDraw();
    if(tag == taxeleventDraw::drawtype)
        return new taxeleventDraw();
    if(tag == accDraw::drawtype)
        return new accDraw();
    if(tag == isoInterestDraw::drawtype)
        return new isoInterestDraw();
    if(tag == isoCircDraw::drawtype)
        return new isoCircDraw();
    if(tag == overlayStereoDraw::drawtype)
        return new overlayStereoDraw();
    if(tag == saeDraw::drawtype)
        return new saeDraw();
    if(tag == imuDraw::drawtype)
        return new imuDraw();
    if(tag == cochleaDraw::drawtype)
        return new cochleaDraw();
    if(tag == rasterDraw::drawtype)
        return new rasterDraw();
    if(tag == rasterDrawHN::drawtype)
        return new rasterDrawHN();
    return 0;

}

/*////////////////////////////////////////////////////////////////////////////*/
//channelInstance
/*////////////////////////////////////////////////////////////////////////////*/
channelInstance::channelInstance(string channel_name, cv::Size render_size) : RateThread(0.1)
{
    this->channel_name = channel_name;
    this->limit_time = 1.0 * vtsHelper::vtsscaler;
    calib_configured = false;
    this->render_size = render_size;
}

string channelInstance::getName()
{
    return channel_name;
}

bool channelInstance::addFrameDrawer(unsigned int width, unsigned int height, 
    const std::string &calibration_file)
{
    calib_configured = unwarp.configure(calibration_file);
    if(!calib_configured)
        yWarning() << "Calibration was not configured";

    desired_res.width = width;
    desired_res.height = height;
    return frame_read_port.open(channel_name + "/frame:i");
}

bool channelInstance::addDrawer(string drawer_name, unsigned int width,
                                unsigned int height, unsigned int window_size,
                                double isoWindow, bool flip)
{
    //make the drawer
    vDraw * new_drawer = createDrawer(drawer_name);
    if(new_drawer) {
        new_drawer->setRetinaLimits(width, height);
        new_drawer->setTemporalLimits(window_size, isoWindow);
        new_drawer->setFlip(flip);
        new_drawer->initialise();
        drawers.push_back(new_drawer);
    } else {
        return false;
    }

    string event_type = new_drawer->getEventType();

    //check to see if we need to open a new input port
    if(read_ports.count(event_type))
        return true;

    //open the port
    total_time[event_type] = 0;
    prev_vstamp[event_type] = 0;
    limit_time = isoWindow;
    return read_ports[event_type].open(channel_name + "/" + event_type + ":i");

}

bool channelInstance::threadInit()
{
    return image_port.open(channel_name + "/image:o");
}

bool channelInstance::updateQs()
{
    bool updated = false;
    Stamp yarp_stamp;
    //fill up the q's as much as possible
    map<string, int> qs_available;
    std::map<string, vReadPort<vQueue> >::iterator port_i;
    for(port_i = read_ports.begin(); port_i != read_ports.end(); port_i++) {
        qs_available[port_i->first] = port_i->second.queryunprocessed();
        if(qs_available[port_i->first]) updated = true;
    }

    for(port_i = read_ports.begin(); port_i != read_ports.end(); port_i++) {
        const string &event_type = port_i->first;
        for(int i = 0; i < qs_available[event_type]; i++) {
            const vQueue *q = port_i->second.read(yarp_stamp);

            int q_dt = (int)q->back()->stamp - prev_vstamp[event_type];
            if(q_dt < 0) q_dt += vtsHelper::max_stamp;

            prev_vstamp[event_type] = (int)q->back()->stamp;
            total_time[event_type] += q_dt;
            bookmark_time[event_type].push_back(q_dt);
            bookmark_n_events[event_type].push_back(q->size());

            for(unsigned j = 0; j < q->size(); j++)
                event_qs[event_type].push_back(q->at(j));
        }
        while(total_time[event_type] > limit_time) {
            for(unsigned int i = 0; i < bookmark_n_events[event_type].front(); i++)
                event_qs[event_type].pop_front();
            total_time[event_type] -= bookmark_time[event_type].front();
            bookmark_time[event_type].pop_front();
            bookmark_n_events[event_type].pop_front();
        }
    }

    if(!frame_read_port.isClosed()) {
        auto image_p = frame_read_port.read(false);
        if(image_p) {
            updated = true;
            cv::Mat temp = yarp::cv::toCvMat(*image_p);
            temp.copyTo(current_frame);
            if(calib_configured) {
                unwarp.denseProjectCam1ToCam0(current_frame);
            }
            cv::resize(current_frame, current_frame,
                       cv::Size(desired_res.width,
                                desired_res.height));
        }
    }

    return updated;
}


void channelInstance::run()
{

    if(!updateQs())
        return;

    cv::Mat canvas;
    //the first drawer will reset the base image, then drawing proceeds
    if(!current_frame.empty()) {
        current_frame.copyTo(canvas);
    } else {
        drawers.front()->resetImage(canvas);
    }

    vector<vDraw *>::iterator drawer_i;
    for(drawer_i = drawers.begin(); drawer_i != drawers.end(); drawer_i++) {
        (*drawer_i)->draw(canvas, event_qs[(*drawer_i)->getEventType()], -1);
    }

    //here we want to rescale the image.
    static cv::Mat resized;
    if(render_size.width > 0) {
        cv::resize(canvas, resized, render_size);
    } else {
        resized = canvas;
    }

    if (canvas.type() == CV_8UC3) {
        image_port.prepare().copy(yarp::cv::fromCvMat<PixelBgr>(resized));
    } else if (canvas.type() == CV_8UC1) {
        image_port.prepare().copy(yarp::cv::fromCvMat<PixelMono>(resized));
    }
    ts.update();
    image_port.setEnvelope(ts);
    image_port.write();
}

void channelInstance::threadRelease()
{
    //close input ports
    std::map<string, vReadPort<vQueue> >::iterator port_i;
    for(port_i = read_ports.begin(); port_i != read_ports.end(); port_i++) {
        port_i->second.close();
    }

    frame_read_port.close();

    //close output port
    image_port.close();

    //delete allocated memory
    std::vector<vDraw *>::iterator drawer_i;
    for(drawer_i = drawers.begin(); drawer_i != drawers.end(); drawer_i++) {
        delete *drawer_i;
    }

}


/*////////////////////////////////////////////////////////////////////////////*/
//module
/*////////////////////////////////////////////////////////////////////////////*/
bool vFramerModule::configure(yarp::os::ResourceFinder &rf)
{
    //admin options
    string moduleName = rf.check("name", Value("/vFramer")).asString();
    setName(moduleName.c_str());

    int height = rf.check("height", Value(240)).asInt();
    int width = rf.check("width", Value(304)).asInt();

    double eventWindow = rf.check("eventWindow", Value(0.1)).asDouble();
    eventWindow *= vtsHelper::vtsscaler;
    eventWindow = std::min(eventWindow, vtsHelper::max_stamp / 2.0);

    double isoWindow = rf.check("isoWindow", Value(1.0)).asDouble();
    isoWindow *= vtsHelper::vtsscaler;
    isoWindow = std::min(isoWindow, vtsHelper::max_stamp / 2.0);

    int frameRate = rf.check("frameRate", Value(30)).asInt();
    double period = 1000.0 / frameRate;

    bool flip =
            rf.check("flip") && rf.check("flip", Value(true)).asBool();

    cv::Size render_size = cv::Size(-1, -1);
    if(rf.check("out_height") && rf.check("out_width"))
    {
        render_size = cv::Size(rf.find("out_width").asInt(), rf.find("out_height").asInt());
    }

    //bool useTimeout =
    //        rf.check("timeout") && rf.check("timeout", Value(true)).asBool();
    
    //bool forceRender =
    //        rf.check("forcerender") &&
    //        rf.check("forcerender", Value(true)).asBool();
//    if(forceRender) {
//        vReader.setStrictUpdatePeriod(vtsHelper::vtsscaler * period);
//        period = 0;
//    }

    //viewer options
    //set up the default channel list
    yarp::os::Bottle tempDisplayList, *bp;
    tempDisplayList.addString("/Left");
    bp = &(tempDisplayList.addList()); bp->addString("AE");
    tempDisplayList.addString("/Right");
    bp = &(tempDisplayList.addList()); bp->addString("AE");

    //set the output channels
    yarp::os::Bottle * displayList = rf.find("displays").asList();
    if(!displayList)
        displayList = &tempDisplayList;

    yInfo() << displayList->toString();

    if(displayList->size() % 2) {
        yError() << "Error: display list configured incorrectly" << displayList->size();
        return false;
    }

    int nDisplays = displayList->size() / 2;


    for(int i = 0; i < nDisplays; i++) {

        string channel_name =
                moduleName + displayList->get(i*2).asString();

        channelInstance * new_ci = new channelInstance(channel_name, render_size);
        new_ci->setRate(period);

        Bottle * drawtypelist = displayList->get(i*2 + 1).asList();
        for(unsigned int j = 0; j < drawtypelist->size(); j++)
        {
            string draw_type = drawtypelist->get(j).asString();
            if(draw_type == "F") {
                new_ci->addFrameDrawer(width, height);
            }
            else if(!new_ci->addDrawer(draw_type, width, height, eventWindow, isoWindow, flip))
            {
                yError() << "Could not create specified publisher"
                         << channel_name << draw_type;
                return false;
            }
        }

        publishers.push_back(new_ci);

    }

    vector<channelInstance *>::iterator pub_i;
    for(pub_i = publishers.begin(); pub_i != publishers.end(); pub_i++) {
        if(!(*pub_i)->start()) {
            yError() << "Could not start publisher" << (*pub_i)->getName();
            return false;
        }
    }

    return true;
}

bool vFramerModule::interruptModule()
{
    vector<channelInstance *>::iterator pub_i;
    for(pub_i = publishers.begin(); pub_i != publishers.end(); pub_i++)
        (*pub_i)->stop();

    return true;
}

bool vFramerModule::close()
{
    vector<channelInstance *>::iterator pub_i;
    for(pub_i = publishers.begin(); pub_i != publishers.end(); pub_i++)
        (*pub_i)->stop();

    return true;
}

bool vFramerModule::updateModule()
{
    return !isStopping();
}

double vFramerModule::getPeriod()
{
    return 1.0;
}

vFramerModule::~vFramerModule()
{

}

