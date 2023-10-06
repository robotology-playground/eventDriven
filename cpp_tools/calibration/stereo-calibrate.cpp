/*
 *   Copyright (C) 2021 Event-driven Perception for Robotics
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
#include <yarp/os/all.h>
#include "event-driven/core.h"
#include "event-driven/vis.h"
#include <thread>

using namespace ev;
using namespace yarp::os;

class calibration_module : public RFModule {

private:
    //input port
    ev::window<ev::AE> cam1;
    ev::window<ev::AE> cam2;

    //provided parameters
    cv::Size img_size_1, img_size_2, board_size;
    double edge_length;

    //calculated parameters
    cv::Mat camera_matrix;
    cv::Mat dist_coeffs;
    cv::Mat map1, map2;

    //internal storage
    std::vector<std::vector<cv::Point2f>> image_points;
    std::stringstream str_maker;
    std::string board_info;

    //file output
    std::ofstream writer;
    

public:

    bool configure(yarp::os::ResourceFinder& rf) override
    {
        //help output
        if(rf.check("h") || rf.check("help")) {
            yInfo() << "Stereo calibration of event-camera";
            yInfo() << "--name <str>\t: internal port name prefix";
            yInfo() << "--fout <str>\t: full path to output file";
            yInfo() << "--ch <int> --cw <int>\t: checkerboard corners height/width";
            yInfo() << "--cs <double>\t: checker square edge length in metres";
            yInfo() << "--cam1cal  <string>\t: path to camera 1 parameter file";
            yInfo() << "--cam2cal <string>\t: path to camera 2 parameter file";
            yInfo() << "--cam1 <string>\t: port name of camera 1";
            yInfo() << "--cam2 <string>\t: port name of camera 2";
            return false;
        }

        //check network
        setName((rf.check("name", Value("/stereo-ev-calibrate")).asString()).c_str());
        if(!yarp::os::Network::checkNetwork(2.0)) {
            std::cout << "Could not connect to YARP" << std::endl;
            return false;
        }

        //extrinsic parameters out
        if(!rf.check("fout")) {
            yError() << "please supply the full path to the output file in --fout <string>";
            return false;
        }
        std::string fout = rf.find("fout").asString();
        writer.open(fout, std::ios_base::trunc);
        if(!writer.is_open()) {
            yError() << "could not open file (ensure path exists?):" << fout;
            return false;
        }

        //supply checkerboard edge size and number of squares
        if (!rf.check("cs")) {
            yError() << "please supply the checker square edge length in metres with --cs <double>";
            return false;
        }
        edge_length = rf.find("cs").asFloat32();
        board_size = cv::Size(rf.check("cw", Value(8)).asInt32(), rf.check("ch", Value(6)).asInt32());

        //get the intrinsic parameters
        ResourceFinder calibfinder;

        if(!rf.check("cam1cal")) {
            yError() << "please supply left camera parameters using --cam1cal <path>";
            return false;
        }
        calibfinder.setDefault("from", rf.find("cam1cal").asString());
        calibfinder.configure(0, 0);
        yarp::os::Bottle params_1 = calibfinder.findGroup("CAMERA_CALIBRATION");
        if(params_1.isNull()) {
            yError() << "Could not find [CAMERA_CALIBRATION] in camera 1 file";
            return false;
        }
        img_size_1 = {params_1.find("w").asInt32(), params_1.find("h").asInt32()};

        if(!rf.check("cam2cal")) {
            yError() << "please supply left camera parameters using --cam2cal <path>";
            return false;
        }
        calibfinder.setDefault("from", rf.find("cam2cal").asString());
        calibfinder.configure(0, 0);
        yarp::os::Bottle params_2 = calibfinder.findGroup("CAMERA_CALIBRATION");
        if(params_2.isNull()) {
            yError() << "Could not find [CAMERA_CALIBRATION] in camera 2 file";
            return false;
        }
        img_size_2 = {params_2.find("w").asInt32(), params_2.find("h").asInt32()};

        yInfo() << "STEREO EVENT-CAMERA CALIBRATION";
        yInfo() << "saving extrinsic calibration:" << fout;
        str_maker.str("");
        str_maker << board_size.width << "x" << board_size.height << " at " << edge_length*1000 << "mm";
        board_info = str_maker.str();
        yInfo() << "board parameters:" << board_info;

        if(!cam1.open(getName("/cam1/AE:i"))) {
            yError() << "could not open input port";
            return false;
        }

        if(!cam2.open(getName("/cam2/AE:i"))) {
            yError() << "could not open input port";
            return false;
        }

        Network::connect(rf.check("cam1", Value("/atis4/cam1/AE:o")).asString(), getName("/cam1/AE:i"), "fast_tcp");
        Network::connect(rf.check("cam2", Value("/atis4/cam2/AE:o")).asString(), getName("/cam2/AE:i"), "fast_tcp");
        
        return true;
    }

    double getPeriod() override
    {
        return 0.2; //period of synchronous thread
    }

    bool interruptModule() override
    {
        //when stop(), isStopping()=true and interruptModule() is called
        //black_thread.join();
        cam1.stop();
        cam2.stop();
        writer.close();
        return true;
    }

    //synchronous thread
    bool updateModule() override
    {
        static cv::Mat black_img_1 = cv::Mat(img_size_1, CV_8UC3);
        static cv::Mat black_img_2 = cv::Mat(img_size_2, CV_8UC3);
        static cv::Mat detected_img_1 = cv::Mat(img_size_1, CV_8UC3, black);
        static cv::Mat detected_img_2 = cv::Mat(img_size_2, CV_8UC3, black);
        static std::vector<int> bci = {0, 
                                       board_size.width-1, 
                                       board_size.area()-board_size.width, 
                                       board_size.area()-1};

        ev::info stats_1 = cam1.readSlidingWinT(0.033, false);
        ev::info stats_2 = cam2.readSlidingWinT(0.033, false);
        
        black_img_1 = ev::black;
        black_img_2 = ev::black;
        for (auto& v : cam1)
            black_img_1.at<cv::Vec3b>(v.y, v.x) = white*0.5;
        for (auto& v : cam2)
            black_img_2.at<cv::Vec3b>(v.y, v.x) = white*0.5;

        std::vector<cv::Point2f> corners_1, corners_2;
        bool found_1 = cv::findChessboardCorners(black_img_1, board_size, corners_1);
        bool found_2 = cv::findChessboardCorners(black_img_2, board_size, corners_2);
        cv::drawChessboardCorners(black_img_1, board_size, corners_1, found_1);
        cv::drawChessboardCorners(black_img_2, board_size, corners_2, found_2);
        

        // bool calibrated = !map1.empty() && !map2.empty();
        if(found_1 && found_2) {
            //image_points.push_back(corners);
            cv::line(detected_img_1, corners_1[bci[0]], corners_1[bci[1]], violet);
            cv::line(detected_img_1, corners_1[bci[0]], corners_1[bci[2]], violet);
            cv::line(detected_img_1, corners_1[bci[3]], corners_1[bci[1]], violet);
            cv::line(detected_img_1, corners_1[bci[3]], corners_1[bci[2]], violet);

            cv::line(detected_img_2, corners_2[bci[0]], corners_2[bci[1]], violet);
            cv::line(detected_img_2, corners_2[bci[0]], corners_2[bci[2]], violet);
            cv::line(detected_img_2, corners_2[bci[3]], corners_2[bci[1]], violet);
            cv::line(detected_img_2, corners_2[bci[3]], corners_2[bci[2]], violet);
        }

        //blue green red
        // if(calibrated) {
        //     remap(black_img, black_img, map1, map2, cv::INTER_LINEAR);
        //     cv::rectangle(black_img, cv::Rect(0, 0, img_size.width, img_size.height), green*0.5, 10);
        //     cv::putText(black_img, "ESC to finish", cv::Point(img_size.width*0.05, img_size.height*0.95), cv::FONT_HERSHEY_PLAIN, 1.0, white);
        // } else {
            black_img_1 += detected_img_1;
            black_img_2 += detected_img_2;
            // cv::rectangle(black_img, cv::Rect(0, 0, img_size.width, img_size.height), red*0.8, 10);
            // cv::putText(black_img, "Collecting images... press SPACE to perform calibration", cv::Point(img_size.width*0.05, img_size.height*0.95), cv::FONT_HERSHEY_PLAIN, 1.0, white);
            // cv::putText(black_img, board_info, cv::Point(img_size.width*0.05, img_size.height*0.05), cv::FONT_HERSHEY_PLAIN, 1.0, white);
            // str_maker.str("");
            // str_maker << image_points.size();
            // cv::putText(black_img, str_maker.str(), cv::Point(img_size.width*0.95, img_size.height*0.95), cv::FONT_HERSHEY_PLAIN, 1.0, white);
        // }
        cv::imshow("camera 1", black_img_1);
        cv::imshow("camera 2", black_img_2);
        char c = cv::waitKey(1);
        if(c == 32) {
            yInfo() << "calibrating...";
            calib_wrapper();
            yInfo() << "saving ... ";
            save_file_wrapper();
            yInfo() << "done .. ";
        }
        if(c == 27) 
        {
            return false;
        }

        
        return true;
    }


    void calib_wrapper()
    {

        std::vector<std::vector<cv::Point3f>> object_points(1);
        std::vector<cv::Point3f> processed_object_points;
        std::vector<cv::Mat> rvecs;
        std::vector<cv::Mat> tvecs;

        //make the object points
        for( int i = 0; i < board_size.height; ++i )
            for( int j = 0; j < board_size.width; ++j )
                object_points[0].push_back(cv::Point3f(j*edge_length, i*edge_length, 0));

        object_points[0][board_size.width - 1].x = object_points[0][0].x + (edge_length * (board_size.width - 1));
        processed_object_points = object_points[0];
        object_points.resize(image_points.size(), object_points[0]);

        //initialise camera matrices
        camera_matrix = cv::Mat::eye(3, 3, CV_64F);
        dist_coeffs = cv::Mat::zeros(8, 1, CV_64F);

        // call calibrate camera
        double rms = cv::calibrateCamera(object_points, image_points, img_size_1,
                                         camera_matrix, dist_coeffs, rvecs, tvecs, cv::CALIB_USE_LU | cv::CALIB_FIX_K3);

        cv::initUndistortRectifyMap(
            camera_matrix, dist_coeffs, cv::Mat(),
            cv::getOptimalNewCameraMatrix(camera_matrix, dist_coeffs, img_size_1, 1, img_size_1, 0), img_size_1,
            CV_16SC2, map1, map2);

        std::cout << camera_matrix << std::endl;
        std::cout << dist_coeffs << std::endl;
    }

    void save_file_wrapper()
    {
        writer << "[CAMERA_CALIBRATION]" << std::endl;
        writer << std::endl;
        writer << "w " << img_size_1.width << std::endl;
        writer << "h " << img_size_1.height << std::endl;
        writer << "fx " << camera_matrix.at<double>(0, 0) << std::endl;
        writer << "fy " << camera_matrix.at<double>(1, 1) << std::endl;
        writer << "cx " << camera_matrix.at<double>(0, 2) << std::endl;
        writer << "cy " << camera_matrix.at<double>(1, 2) << std::endl;
        writer << "k1 " << dist_coeffs.at<double>(0, 0) << std::endl;
        writer << "k2 " << dist_coeffs.at<double>(1, 0) << std::endl;
        writer << "p1 " << dist_coeffs.at<double>(2, 0) << std::endl;
        writer << "p2 " << dist_coeffs.at<double>(3, 0) << std::endl;
        writer.flush();
    }

};

int main(int argc, char * argv[])
{

    /* prepare and configure the resource finder */
    yarp::os::ResourceFinder rf;
    rf.configure( argc, argv );

    /* create the module */
    calibration_module instance;
    return instance.runModule(rf);
}
