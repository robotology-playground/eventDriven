#pragma once

#include <opencv2/opencv.hpp>

namespace ev {

class surface 
{
protected:
    int kernel_size{0};
    int half_kernel{0};
    double parameter{0};
    double time_now{0};

    cv::Rect actual_region;
    cv::Mat surf;

public:
   
    virtual cv::Mat getSurface();
    virtual void init(int width, int height, int kernel_size = 5, double parameter = 0.0);
    virtual inline void update(int x, int y, double ts, int p) = 0;
    void temporalDecay(double ts, double alpha);
    void spatialDecay(int k);
};

class EROS : public surface {
   public:
    inline void update(int x, int y, double t = 0, int p = 0) override 
    {
        static double odecay = pow(parameter, 1.0 / kernel_size);
        surf({x, y, kernel_size, kernel_size}) *= odecay;
        surf.at<double>(y+half_kernel, x+half_kernel) = 255.0;
    }
};

class TOS : public surface 
{
   public:
    inline void update(int x, int y, double t = 0, int p = 0) override {
        (void)t; (void)p;
        // parameter default = 2
        static const double threshold = 255.0 - kernel_size * parameter;
        static cv::Rect roi = {0, 0, kernel_size, kernel_size};

        roi.x = x; roi.y = y;
        static cv::Mat region = surf(roi);

        for (auto xi = 0; xi < region.cols; xi++) {
            for (auto yi = 0; yi < region.rows; yi++) {
                double &p = region.at<double>(yi, xi);
                if (p < threshold) p = 0;
                else p--;
            }
        }
        surf.at<double>(y+half_kernel, x+half_kernel) = 255.0;
    }
};

class SITS : public surface {
   public:
    inline void update(int x, int y, double t = 0, int p = 0) override {
        static const int maximum_value = kernel_size * kernel_size;
        static cv::Rect roi = {0, 0, kernel_size, kernel_size};

        roi.x = x; roi.y = y;
        static cv::Mat region = surf(roi);

        double &c = surf.at<double>(y+half_kernel, x+half_kernel);
        for (auto xi = 0; xi < region.cols; xi++) {
            for (auto yi = 0; yi < region.rows; yi++) {
                double &p = region.at<double>(yi, xi);
                if (p > c) p--;
            }
        }
        c = maximum_value;
    }
};

class PIM : public surface {
   public:
    inline void update(int x, int y, double t = 0, int p = 0) override
    {
        if (p)
            surf.at<double>(y+half_kernel, x+half_kernel) -= 1.0f;
        else
            surf.at<double>(y+half_kernel, x+half_kernel) += 1.0f;
    }
};

class SAE : public surface 
{
   public:
    inline void update(int x, int y, double t = 0, int p = 0) override
    {
        surf.at<double>(y+half_kernel, x+half_kernel) = t;
    }
};

class BIN : public surface
{
   public:
    inline void update(int x, int y, double t = 0, int p = 0) override
    {
        surf.at<double>(y+half_kernel, x+half_kernel) = 255.0;
    }
};

// Set of Centre Active Retinal Fields
class CARF
{
friend class SCARF;
private:
    //parameters
    struct pnt {
        int u:14;
        int v:14;
        int c:4;
    };
    int N{0};

    //variables
    int i{0};
    std::vector<pnt> points;

public:

    CARF(int N) {
        points.resize(N, {0, 0, 0});
        this->N = N;
    }

    inline void add(const CARF::pnt &p)
    {
         points[i] = p;
         if(++i >= N) i = 0;
    }
};

class SCARF
{
private:
    //parameters
    cv::Size count{{0, 0}};
    cv::Size dims{{0, 0}};

    //variables
    cv::Mat img;
    std::vector<CARF> rfs;
    std::vector<std::array<CARF::pnt, 4>> cons_map;

public:

    void initialise(int img_w, int img_h, int rf_size)
    {
        initialise(img_w, img_h, img_w/rf_size, img_h/rf_size);
    }

    void initialise(int img_w, int img_h, int rfs_x, int rfs_y, double alpha = 1.0)
    {
        img = cv::Mat(img_h, img_w, CV_32F);
        count = {rfs_x, rfs_y};
        dims = {img_w / rfs_x, img_h / rfs_y};
        int N = dims.area() * alpha * 0.5;

        cons_map.resize(img_w*img_h);
        rfs.resize(count.height*count.width, CARF(N));
        
        for(int y = 0; y < img_h; y++) {
            for(int x = 0; x < img_w; x++) {
                int rfx = x / dims.width;
                int rfy = y / dims.height;
                if(rfx >= count.width || rfy >= count.height)
                    continue;
                
                int i = 0;
                auto &conxs = cons_map[y*img_w + x];
                conxs[i++] = {rfx, rfy, 1};

                int ky = y%dims.height;
                int kx = x%dims.width;

                bool top{false}, bot{false}, lef{false}, rig{false};
                if(ky < dims.height * 0.5) 
                    {if(rfy > 0) top = true;}
                else 
                    {if(rfy < count.height-1) bot = true;}
                if(kx < dims.width * 0.5) 
                    {if(rfx > 0) lef = true;}
                else 
                    {if(rfx < count.width-1) rig = true;}

                if(top) conxs[i++] = {rfx, rfy-1, 0};
                if(bot) conxs[i++] = {rfx, rfy+1, 0};
                if(lef) conxs[i++] = {rfx-1, rfy, 0};
                if(rig) conxs[i++] = {rfx+1, rfy, 0};
                if(top && lef) conxs[i++] = {rfx-1, rfy-1, 0};
                if(top && rig) conxs[i++] = {rfx+1, rfy-1, 0};
                if(bot && lef) conxs[i++] = {rfx-1, rfy+1, 0};
                if(bot && rig) conxs[i++] = {rfx+1, rfy+1, 0};

                while(i < 4) conxs[i++] = {-1, -1, -1};
            }
        }
    }

    inline void update(const int &u, const int &v)
    {
        auto &conxs = cons_map[v*img.cols+u];
        for(auto &conx : conxs) {
            if(conx.c < 0) return;
            rfs[conx.v * count.width + conx.u].add({u, v, conx.c});
        }
    }

    cv::Mat getSurface()
    {
        img.setTo(0.0);
        for(int rf = 0; rf < count.area(); rf++)
            for(auto &p : rfs[rf].points)
                if(p.c) img.at<float>(p.v, p.u) += 0.2;
                     
        return img;
    }

    std::vector<cv::Point> getList(int u, int v)
    {
        std::vector<cv::Point> p;
        for(auto &i : rfs[v*count.width+u].points)
            if(i.c) p.push_back({i.u, i.v});
        return p;
    }
};



}
