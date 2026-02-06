#ifndef IMAGE2RTSP_IMAGE2RTSP_HPP
#define IMAGE2RTSP_IMAGE2RTSP_HPP

#include <gst/gst.h>
#include <gst/rtsp-server/rtsp-server.h>
#include <rclcpp/rclcpp.hpp>
#include <string>
#include <vector>
#include "sensor_msgs/msg/image.hpp"
#include "sensor_msgs/msg/compressed_image.hpp"
#include <opencv2/opencv.hpp>

using namespace std;


struct Stream {
    std::string topic;
    std::string mountpoint;
    bool compressed = false;
    GstAppSrc* appsrc = nullptr;
    rclcpp::Subscription<sensor_msgs::msg::Image>::SharedPtr sub;
    rclcpp::Subscription<sensor_msgs::msg::CompressedImage>::SharedPtr sub_compressed;
};


class Image2rtsp : public rclcpp::Node{
public:
    Image2rtsp();
    GstRTSPServer *rtsp_server;

private:
    vector<Stream> streams_;
    vector<string> topics = {"/topic1","/topic2","/topic3"};
    vector<string> mountpoints = {"/1","/2","/3"};
    vector<bool> is_compressed = {false,false,false};
    string port;
    string pipeline;
    string default_pipeline;
    string camera_pipeline;
    uint framerate;
    bool local_only;
    bool camera;

    void video_mainloop_start();
    void rtsp_server_add_url(const char *url, const char *sPipeline, GstElement **appsrc);
    void topic_callback(const sensor_msgs::msg::Image::SharedPtr msg, Stream* stream);
    void compressed_topic_callback(const sensor_msgs::msg::CompressedImage::SharedPtr msg, Stream* stream);
    uint extract_framerate(const std::string& pipeline, uint default_framerate);
    GstRTSPServer *rtsp_server_create(const string &port, const bool local_only);
    GstCaps *gst_caps_new_from_image(const sensor_msgs::msg::Image::SharedPtr &msg);
};

static void media_configure(GstRTSPMediaFactory *factory, GstRTSPMedia *media, GstElement **appsrc);
static void *mainloop(void *arg);
static gboolean session_cleanup(Image2rtsp *node, rclcpp::Logger logger, gboolean ignored);

#endif // IMAGE2RTSP_IMAGE2RTSP_HPP
