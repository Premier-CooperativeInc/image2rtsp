#include "rclcpp/rclcpp.hpp"
#include "sensor_msgs/msg/image.hpp"
#include <gst/gst.h>
#include <gst/rtsp-server/rtsp-server.h>
#include <gst/app/gstappsrc.h>
#include "../include/image2rtsp.hpp"

using std::placeholders::_1;

Image2rtsp::Image2rtsp() : Node("image2rtsp"){
    // Declare and get the parameters
    this->declare_parameter("topic",        topics);
    this->declare_parameter("mountpoint",   mountpoints);
    this->declare_parameter("port",         "8554");
    this->declare_parameter("local_only",   true);
    this->declare_parameter("camera",       false);
    this->declare_parameter("compressed",   is_compressed);
    this->declare_parameter("username",   "test");
    this->declare_parameter("password",   "123");
    this->declare_parameter("password_protect",   false);

    this->declare_parameter("default_pipeline",   R"(
                                                    ( appsrc name=imagesrc do-timestamp=true min-latency=0 max-latency=0 max-bytes=1000 is-live=true !
                                                    videoconvert !
                                                    videoscale !
                                                    video/x-raw, framerate=30/1, width=640, height=480 !
                                                    x264enc tune=zerolatency bitrate=500 key-int-max=30 !
                                                    video/x-h264, profile=baseline !
                                                    rtph264pay name=pay0 pt=96 )
                                                    )");

    this->declare_parameter("camera_pipeline",    R"(
                                                    ( v4l2src device=/dev/video0 !
                                                    videoconvert !
                                                    videoscale !
                                                    video/x-raw, framerate=30/1, width=640, height=480 !
                                                    x264enc tune=zerolatency bitrate=500 key-int-max=30 !
                                                    video/x-h264, profile=baseline !
                                                    rtph264pay name=pay0 pt=96 )
                                                    )");

    topics           = this->get_parameter("topic").as_string_array();
    mountpoints      = this->get_parameter("mountpoint").as_string_array();
    port             = this->get_parameter("port").as_string();
    local_only       = this->get_parameter("local_only").as_bool();
    camera           = this->get_parameter("camera").as_bool();
    is_compressed    = this->get_parameter("compressed").as_bool_array();
    username         = this->get_parameter("username").as_string();
    password         = this->get_parameter("password").as_string();
    password_protect = this->get_parameter("password_protect").as_bool();
    default_pipeline = this->get_parameter("default_pipeline").as_string();
    camera_pipeline  = this->get_parameter("camera_pipeline").as_string();

    // Failsafe
    if (topics.size() != mountpoints.size() || topics.size() != is_compressed.size()) {
        throw std::runtime_error("topic / mountpoint / compressed parameter sizes do not match");
    }

    // Iterating through all parameters and creating a struct for all streams
    streams_.clear();
    for (size_t i = 0; i < topics.size(); i++) {
        
        Stream s;
        s.topic = topics[i];
        s.mountpoint = mountpoints[i];
        s.appsrc = NULL;
        s.compressed = is_compressed[i];
        streams_.push_back(s);
    }

    if (!camera) {
        for (Stream &s : streams_) {

            if (!s.compressed) {
                s.sub = this->create_subscription<sensor_msgs::msg::Image>(
                    s.topic,
                    10,
                    [this, &s](sensor_msgs::msg::Image::SharedPtr msg) {
                        this->topic_callback(msg, &s);
                    });

                RCLCPP_INFO(this->get_logger(),
                            "Subscribed to Image topic: %s",
                            s.topic.c_str());
            }
            else {
                s.sub_compressed =
                    this->create_subscription<sensor_msgs::msg::CompressedImage>(
                        s.topic,
                        10,
                        [this, &s](sensor_msgs::msg::CompressedImage::SharedPtr msg) {
                            this->compressed_topic_callback(msg, &s);
                        });

                RCLCPP_INFO(this->get_logger(),
                            "Subscribed to CompressedImage topic: %s",
                            s.topic.c_str());
            }
        }
    }
    else {
        RCLCPP_INFO(this->get_logger(), "Trying to access camera device");
    }


    // Start the RTSP server
    pipeline = camera ? camera_pipeline : default_pipeline;
    framerate = extract_framerate(pipeline, 30);

    video_mainloop_start();
    rtsp_server = rtsp_server_create(port, local_only);
    if (password_protect){
        setup_auth(username.c_str(), password.c_str());  
    }
    for (Stream &s : streams_) {
        s.appsrc = NULL;
        rtsp_server_add_url(s.mountpoint.c_str(), pipeline.c_str(), camera ? nullptr : (GstElement **)&s.appsrc);
        RCLCPP_INFO(this->get_logger(), "Stream available at rtsp://%s:%s%s", gst_rtsp_server_get_address(rtsp_server), port.c_str(), s.mountpoint.c_str());

    }
}

uint Image2rtsp::extract_framerate(const std::string& pipeline, uint default_framerate = 30) {
    std::string search_str = "framerate=";
    size_t pos = pipeline.find(search_str);
    if (pos == std::string::npos) {
        RCLCPP_WARN(this->get_logger(), "Framerate not found in pipeline, using default: %d", default_framerate);
        return default_framerate;
    }

    pos += search_str.length();

    size_t end_pos = pipeline.find_first_of("/,", pos);
    if (end_pos == std::string::npos) {
        RCLCPP_WARN(this->get_logger(), "Invalid framerate format in pipeline, using default: %d", default_framerate);
        return default_framerate;
    }

    std::string framerate_str = pipeline.substr(pos, end_pos - pos);

    framerate_str.erase(0, framerate_str.find_first_not_of(" \t"));
    framerate_str.erase(framerate_str.find_last_not_of(" \t") + 1);
    
    try {
        uint framerate = std::stoi(framerate_str);
        if (framerate <= 0) {
            RCLCPP_WARN(this->get_logger(), "Invalid framerate value %d, using default: %d", framerate, default_framerate);
            return default_framerate;
        }
        RCLCPP_INFO(this->get_logger(), "Using set framerate %d", framerate);
        return framerate;
    } catch (const std::exception& e) {
        RCLCPP_WARN(this->get_logger(), "Failed to parse framerate '%s', using default: %d", framerate_str.c_str(), default_framerate);
        return default_framerate;
    }
}

int main(int argc, char *argv[]){
    rclcpp::init(argc, argv);
    auto node = std::make_shared<Image2rtsp>();
    rclcpp::spin(node);
    rclcpp::shutdown();
    return 0;
}
