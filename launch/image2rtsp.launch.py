from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration, PathJoinSubstitution
from launch_ros.actions import Node
from ament_index_python.packages import get_package_share_directory
import os

def generate_launch_description():

    bringup_share = get_package_share_directory('image2rtsp')
    default_config_dir = os.path.join(bringup_share, 'config')

    config_dir_arg = DeclareLaunchArgument(
        'config',
        default_value=default_config_dir,
        description='Path to config directory'
    )

    config_dir = LaunchConfiguration('config')
    stream_file = PathJoinSubstitution([config_dir, 'parameters.yaml'])


    stream_node = Node(
        package='image2rtsp',
        executable='image2rtsp',
        name='image2rtsp',
        parameters=[stream_file],
        output='log'
    )


    return LaunchDescription([
        config_dir_arg,
        stream_node
    ])
