/*
 *   Copyright (C) 2017 Event-driven Perception for Robotics
 *   Author: arren.glover@iit.it
 *           valentina.vasco@iit.it
 *           chiara.bartolozzi@iit.it
 *           massimiliano.iacono@iit.it
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

#include "vDraw.h"

using namespace ev;

// BLOB DRAW //
// ========= //

const std::string blobDraw::drawtype = "BLOB";

std::string blobDraw::getDrawType()
{
    return blobDraw::drawtype;
}

std::string blobDraw::getEventType()
{
    return AE::tag;
}

void blobDraw::draw(cv::Mat &image, const ev::vQueue &eSet, int vTime)
{

    if(eSet.empty()) return;
    if(vTime < 0) vTime = eSet.back()->stamp;

    ev::vQueue::const_reverse_iterator qi;
    for(qi = eSet.rbegin(); qi != eSet.rend(); qi++) {


        int dt = vTime - (*qi)->stamp;
        if(dt < 0) dt += ev::vtsHelper::max_stamp;
        if((unsigned int)dt > display_window) break;

        auto aep = as_event<AE>(*qi);
        if(!aep) continue;

        int y = aep->y;
        int x = aep->x;

        if(flip) {
            y = Ylimit - 1 - y;
            x = Xlimit - 1 - x;
        }

        if(!aep->polarity)
            image.at<cv::Vec3b>(y, x) = cv::Vec3b(0, 0, 0);
    }

    cv::medianBlur(image, image, 5);
    cv::blur(image, image, cv::Size(5, 5));
}

// CIRCLE DRAW //
// =========== //

const std::string circleDraw::drawtype = "CIRC";

std::string circleDraw::getDrawType()
{
    return circleDraw::drawtype;
}

std::string circleDraw::getEventType()
{
    return GaussianAE::tag;
}

void circleDraw::draw(cv::Mat &image, const vQueue &eSet, int vTime)
{
    cv::Scalar blue = CV_RGB(0, 0, 255);
    cv::Scalar red = CV_RGB(255, 0, 0);

    //update the 'persistence' the current state of each of the cluster ID's
    for(vQueue::const_iterator qi = eSet.begin(); qi != eSet.end(); qi++) {
        auto vp = is_event<GaussianAE>(*qi);
        if(vp) {
            persistance[vp->ID] = vp;
        }
    }

    std::map<int, event<GaussianAE> >::iterator ci;
    for(ci = persistance.begin(); ci != persistance.end(); ci++) {

        auto v = ci->second;
        if(v->polarity) continue;

        if(v->x < 0 || v->x >= Xlimit || v->y < 0 || v->y >= Ylimit) continue;
        if(v->sigxy >= v->sigx) continue;

        cv::Point centr(v->x, v->y);
        if(flip) {
            centr.x = Xlimit - 1 - centr.x;
            centr.y = Ylimit - 1 - centr.y;
        }

        cv::circle(image, centr, v->sigx - v->sigxy, red, 1.0);
        cv::circle(image, centr, v->sigx + v->sigxy, red, 1.0);

        continue;
    }

    for(ci = persistance.begin(); ci != persistance.end(); ci++) {

        auto v = ci->second;
        if(!v->polarity) continue;

        if(v->x < 0 || v->x >= Xlimit || v->y < 0 || v->y >= Ylimit) continue;
        if(v->sigxy >= v->sigx) continue;

        cv::Point centr(v->x, v->y);
        if(flip) {
            centr.x = Xlimit - 1 - centr.x;
            centr.y = Ylimit - 1 - centr.y;
        }

        cv::circle(image, centr, v->sigx - v->sigxy, blue, 1.0);
        cv::circle(image, centr, v->sigx + v->sigxy, blue, 1.0);

        continue;
    }

}

// GRAY DRAW //
// =========== //

const std::string grayDraw::drawtype = "GRAY";

std::string grayDraw::getDrawType()
{
    return grayDraw::drawtype;
}

std::string grayDraw::getEventType()
{
    return AddressEvent::tag;
}

void grayDraw::draw(cv::Mat &image, const ev::vQueue &eSet, int vTime)
{
    image = cv::Scalar(127, 127, 127);
    if(eSet.empty()) return;
    if(vTime < 0) vTime = eSet.back()->stamp;
    ev::vQueue::const_reverse_iterator qi;
    for(qi = eSet.rbegin(); qi != eSet.rend(); qi++) {

        int dt = vTime - (*qi)->stamp;
        if(dt < 0) dt += ev::vtsHelper::max_stamp;
        if((unsigned int)dt > display_window) break;


        auto aep = is_event<AddressEvent>(*qi);
        int y = aep->y;
        int x = aep->x;
        if(flip) {
            y = Ylimit - 1 - y;
            x = Xlimit - 1 - x;
        }

        cv::Vec3b &cpc = image.at<cv::Vec3b>(y, x);

        if(!aep->polarity)
        {
            cpc[0] = 0;
            cpc[1] = 0;
            cpc[2] = 0;
        }
        else
        {
            cpc[0] = 255;
            cpc[1] = 255;
            cpc[2] = 255;

        }
    }
}

// STEREO OVERLAY DRAW //
// =================== //

const std::string overlayStereoDraw::drawtype = "OVERLAY";

std::string overlayStereoDraw::getDrawType()
{
    return overlayStereoDraw::drawtype;
}

std::string overlayStereoDraw::getEventType()
{
    return AddressEvent::tag;
}

void overlayStereoDraw::draw(cv::Mat &image, const ev::vQueue &eSet, int vTime)
{
    if(eSet.empty()) return;
    if(vTime < 0) vTime = eSet.back()->stamp;
    ev::vQueue::const_reverse_iterator qi;
    for(qi = eSet.rbegin(); qi != eSet.rend(); qi++) {

        int dt = vTime - (*qi)->stamp;
        if(dt < 0) dt += ev::vtsHelper::max_stamp;
        if((unsigned int)dt > display_window) break;


        auto aep = is_event<AddressEvent>(*qi);
        int y = aep->y;
        int x = aep->x;
        if(flip) {
            y = Ylimit - 1 - y;
            x = Xlimit - 1 - x;
        }


        cv::Vec3b &cpc = image.at<cv::Vec3b>(y, x);

        if(cpc[0]==0 && cpc[1]==255 && cpc[2]==255) //skip marking already overlapping pixels
            continue;
        if(!aep->channel)
        {
            if(cpc[0]==0 && cpc[1]==0 && cpc[2]==255) //both left and right channel, mark YELLOW
            {
                cpc[0]=0;
                cpc[1]=255;
                cpc[2]=255;
            }
            else   //only left channel, mark BLUE
            {
                cpc[0]=255;
                cpc[1]=0;
                cpc[2]=0;
            }
        }
        else
        {
            if(cpc[0]==255 && cpc[1]==0 && cpc[2]==0)   //both left and right channel, mark YELLOW
            {
                cpc[0]=0;
                cpc[1]=255;
                cpc[2]=255;
            }
            else    //only right channel, mark RED
            {
                cpc[0]=0;
                cpc[1]=0;
                cpc[2]=255;
            }
        }
    }
}

//Rasterplot Draw//
//=============//

const std::string rasterDraw::drawtype = "RASTER";

std::string rasterDraw::getDrawType()
{
    return rasterDraw::drawtype;
}

std::string rasterDraw::getEventType()
{
    return AddressEvent::tag;
}

void rasterDraw::draw(cv::Mat &image, const ev::vQueue &eSet, int vTime)
{
    // eSet is a merge of all queues received by the vFramer:i-port in frameRate time
    // The vFramer:i-port accumulates events, so the eSet gets longer over time!

    // check if eSet contains elements
    if(eSet.empty()){
        return;
    }
    // get latest timestamp
    if(vTime < 0){
        vTime = eSet.back()->stamp;
    }
    // Reverse Timeelement-Iterator:
    for(int y=(Ylimit-1.0); y>=0; y--){
        for(int x=(Xlimit-1); x>=0; x--){

            //check if there is an event in the storage
            if (pixelStorage[y][x] > 0){

                //print this event as a blue point
                image.at<cv::Vec3b>(y, x) = cv::Vec3b(255, 0, 0);

                //if event reaches the end reset the pixelStorage
                if(x == (Xlimit-1)){
                    pixelStorage[y][x] = 0;
                }
                //else increase the timeelement of this event and reset the old field
                else{
                    pixelStorage[y][x+1] = pixelStorage[y][x];
                    pixelStorage[y][x] = 0;
                    }
            }
        }
    }
    // go through the eSet-q and start with the latest event
    ev::vQueue::const_reverse_iterator qi;
    for(qi = eSet.rbegin(); qi != eSet.rend(); qi++) {

        // calculate the time difference between latest and this event
        int dt = vTime - (*qi)->stamp;

        // if the difference is negative, a time wrap occured at Maxstamp (~85 ms)
        if(dt < 0)
            dt += ev::vtsHelper::max_stamp;

        // Ignore events in eSet that are older than display_window (= 1 ms)
        if((unsigned int)dt > display_window){
            break;
        }

        // Safety: Make whatevers inside this q-element to an AE
        auto aep = as_event<AE>(*qi);

        if(!aep){
            continue;
        }
        // Safe it as an accessible AE
        AE v = *(aep);

        // Get the neuronID as a decimal number
        unsigned int y = v._coded_data;

        // scale neuronID to YLimit of vFramer
        if(scaling){
            y = round(y * yScaler);
            if(flip) {
                y = (Ylimit - 1) - y;
            }
            pixelStorage[y][0] = 1;
        }
        // else check that neuronID does not reach limits of pixelStorage
        else if((Ylimit-1) >= y){
            if(flip) {
                y = (Ylimit - 1) - y;
            }
            pixelStorage[y][0] = 1;
        }
    }
}
