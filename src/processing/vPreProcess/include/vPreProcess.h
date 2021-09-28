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

// \defgroup Modules Modules
// \defgroup vPepper vPepper
// \ingroup Modules
// \brief removes salt-and-pepper noise from the event stream

#ifndef __VPREPROCESS__
#define __VPREPROCESS__

#include <yarp/os/all.h>
#include <yarp/sig/all.h>
#include <event-driven/all.h>
#include <event-driven/vIPT.h>
#include <opencv2/opencv.hpp>

class vPreProcess : public yarp::os::RFModule, public yarp::os::Thread
{
private:

    //output port for the vBottle with the new events computed by the module

    ev::vReadPort < vector<int32_t> > inPort;
    ev::vWritePort outPortCamLeft;
    ev::vWritePort outPortCamLeft_pos;
    ev::vWritePort outPortCamLeft_neg;
    ev::vWritePort outPortCamRight;
    ev::vWritePort outPortCamRight_pos;
    ev::vWritePort outPortCamRight_neg;
    ev::vWritePort outPortCamStereo;
    ev::vWritePort outPortCamStereo_pos;
    ev::vWritePort outPortCamStereo_neg;
    ev::vWritePort outPortSkin;
    ev::vWritePort outPortSkinSamples;
    ev::vWritePort out_port_aps_left;
    ev::vWritePort out_port_aps_right;
    ev::vWritePort out_port_aps_stereo;
    ev::vWritePort out_port_imu_samples;
    ev::vWritePort out_port_audio;
    ev::vWritePort out_port_crn_left;
    ev::vWritePort out_port_crn_right;
    ev::vWritePort out_port_crn_stereo;
    yarp::os::BufferedPort< yarp::sig::Vector > rate_port;

    //parameters
    std::string name;
    ev::resolution res;

    //pre-pre processing
    bool precheck;
    bool flipx;
    bool flipy;

    //filter class
    bool apply_filter;
    ev::vNoiseFilter filter_left;
    ev::vNoiseFilter filter_right;
    int v_total;
    int v_dropped;

    //we store an openCV map to use as a look-up table for the undistortion
    //given the camera parameters provided
    bool undistort;
    ev::vIPT calibrator;

    //output
    bool split_stereo;
    bool split_polarities;
    bool combined_stereo;
    bool use_local_stamp;
    bool corners;

    //timing stats
    std::deque<double> delays;
    std::deque<double> rates;
    std::deque<double> intervals;
    std::deque<double> proc_times;
    
    bool vis{false};
    std::deque<double> plot_rates;
    void visualise_rate();

public:

    vPreProcess();
    ~vPreProcess();


    //inherited functions
    virtual bool configure(yarp::os::ResourceFinder &rf);
    double getPeriod();
    bool interruptModule();
    void onStop();
    bool threadInit();
    bool updateModule();
    void run();

};

#endif
